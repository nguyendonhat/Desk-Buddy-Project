#include <Arduino.h>
#include <ArduinoJson.h>
#include <GxEPD2_BW.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <RTClib.h>
#include <qrcode.h>
#include <SPI.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <Wire.h>

#include "config.h"
#include "calendar_logic.h"
#include "interaction.h"

// WeAct Studio 2.9-inch monochrome panel, common SSD1680/GDEY029T94 variant.
constexpr int EPD_CS = 27;
constexpr int EPD_DC = 26;
constexpr int EPD_RST = 25;
constexpr int EPD_BUSY = 34;
constexpr int I2C_SDA = 21;
constexpr int I2C_SCL = 22;
constexpr int RGB_RED = 13;
constexpr int RGB_GREEN = 14;
constexpr int RGB_BLUE = 16;
constexpr int BUZZER_PIN = 17;
constexpr int BUTTON_MODE = 32;
constexpr int BUTTON_UP = 33;
constexpr int BUTTON_DOWN = 19;
constexpr int BUTTON_OK = 35; // Input-only; requires an external 10 kOhm pull-up.

constexpr uint32_t CALENDAR_REFRESH_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t CALENDAR_RETRY_MS = 10UL * 1000UL;
constexpr uint32_t WIFI_RETRY_MS = 10UL * 1000UL;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 12000UL;
constexpr uint32_t EVENT_NOTE_FLIP_MS = 4000UL;
constexpr uint32_t NOTE_IDLE_RESET_MS = 15000UL;
constexpr uint32_t BUZZER_DOUBLE_PULSE_MS = 1000UL;
constexpr uint32_t BUZZER_GAP_MS = 300UL;
constexpr uint32_t BUZZER_FINISH_MS = 3000UL;
constexpr size_t MAX_EVENTS = 30;
constexpr long REMINDER_WINDOW_SECONDS = 15L * 60L;

GxEPD2_BW<GxEPD2_290_GDEY029T94, GxEPD2_290_GDEY029T94::HEIGHT> epaper(
  GxEPD2_290_GDEY029T94(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
U8G2_FOR_ADAFRUIT_GFX epaperText;
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 rtc;
QRCode accountQr;
uint8_t accountQrData[400]; // QR version 8 module buffer (352 bytes).

struct CalendarEvent {
  String title;
  String start;
  String description;
};

CalendarEvent events[MAX_EVENTS];
size_t eventCount = 0;
uint32_t lastCalendarFetch = 0;
uint32_t lastWifiAttempt = 0;
uint32_t lastLcdUpdate = 0;
String lastReminderKey;
bool rtcReady = false;
bool ntpSynced = false;
bool calendarLoaded = false;
bool reminderActive = false;
deskbuddy::NavigationState navigation;
uint32_t lastButtonAt = 0;
bool previousButtonsHigh[4] = {true, true, true, true};
uint32_t pomodoroDeadlineMs = 0;
uint8_t pomodoroCycles = 4;
uint8_t pomodoroCyclesRemaining = 0;
uint8_t pomodoroBreakMinutes = 5;
uint8_t pomodoroSetupStep = 0; // cycles, work duration, break duration
bool pomodoroOnBreak = false;
uint32_t lastPomodoroDisplayMinute = UINT32_MAX;
uint8_t buzzerPulsesRemaining = 0;
bool buzzerSignalActive = false;
bool buzzerSignalOn = false;
bool buzzerContinuous = false;
uint32_t buzzerSignalDeadline = 0;
bool showingEventNote = false;
uint32_t lastEventNoteFlip = 0;
uint32_t lastNoteInteraction = 0;
Preferences uiPreferences;
Preferences wifiPreferences;
WebServer wifiConfigServer(80);
String wifiSsid;
String wifiPassword;
String wifiApName;
String wifiApPassword;
bool noteStateDirty = false;
uint32_t noteStateChangedAt = 0;
bool restoreWeekDay = false;

void drawCalendar();

void loadNoteState() {
  if (!uiPreferences.begin("deskbuddy", false)) return;
  if (uiPreferences.getBool("noteSaved", false)) {
    navigation.screen = deskbuddy::Screen::Note;
    const uint8_t savedWeekDay = uiPreferences.getUChar("weekDay", 255);
    if (savedWeekDay < 7) {
      navigation.dayOffset = savedWeekDay;
      restoreWeekDay = true;
    }
    navigation.eventIndex = uiPreferences.getUChar("noteEvent", 0);
    navigation.notePage = uiPreferences.getUChar("notePage", 0);
  }
}

void queueNoteStateSave() {
  noteStateDirty = true;
  noteStateChangedAt = millis();
}

void saveNoteStateIfIdle(uint32_t now) {
  if (!noteStateDirty || static_cast<uint32_t>(now - noteStateChangedAt) < 800) return;
  uiPreferences.putBool("noteSaved", true);
  uiPreferences.putUChar("weekDay", navigation.dayOffset);
  uiPreferences.putUChar("noteEvent", navigation.eventIndex);
  uiPreferences.putUChar("notePage", navigation.notePage);
  noteStateDirty = false;
}

int64_t eventLocalDay(const CalendarEvent& event) {
  int64_t epoch = 0;
  if (deskbuddy::parseEventLocalEpoch(std::string(event.start.c_str()), LOCAL_UTC_OFFSET_SECONDS, epoch)) return epoch / 86400;
  int year, month, day;
  const std::string start(event.start.c_str());
  if (deskbuddy::readDigits(start, 0, 4, year) && deskbuddy::readDigits(start, 5, 2, month) &&
      deskbuddy::readDigits(start, 8, 2, day)) return deskbuddy::daysFromCivil(year, month, day);
  return -1;
}

String formatEventLocalTime(const CalendarEvent& event) {
  int64_t localEpoch = 0;
  if (deskbuddy::parseEventLocalEpoch(std::string(event.start.c_str()), LOCAL_UTC_OFFSET_SECONDS, localEpoch)) {
    int64_t secondsInDay = localEpoch % 86400;
    if (secondsInDay < 0) secondsInDay += 86400;
    const int hour = static_cast<int>(secondsInDay / 3600);
    const int minute = static_cast<int>((secondsInDay % 3600) / 60);
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
    return String(buf);
  }
  if (event.start.length() >= 16) {
    return event.start.substring(11, 16);
  }
  return "Ca ngay";
}

// Tách văn bản UTF-8 thành từng dòng (tôn trọng dấu \n và không cắt đứt ký tự tiếng Việt)
size_t extractNotePageLines(const String& raw, size_t startLine, String outLines[3], size_t maxCharsPerLine = 32) {
  if (outLines) {
    outLines[0] = "";
    outLines[1] = "";
    outLines[2] = "";
  }
  size_t totalLines = 0;
  size_t i = 0;
  const size_t len = raw.length();

  while (i < len) {
    while (i < len && (raw[i] == '\r' || raw[i] == '\n' || raw[i] == ' ')) ++i;
    if (i >= len) break;

    size_t lineStart = i;
    size_t charCount = 0;
    size_t lastSpace = 0;

    while (i < len && raw[i] != '\n' && raw[i] != '\r') {
      if (raw[i] == ' ') lastSpace = i;
      uint8_t c = static_cast<uint8_t>(raw[i]);
      size_t step = 1;
      if ((c & 0xE0) == 0xC0) step = 2;
      else if ((c & 0xF0) == 0xE0) step = 3;
      else if ((c & 0xF8) == 0xF0) step = 4;

      if (charCount >= maxCharsPerLine) {
        if (lastSpace > lineStart) i = lastSpace;
        break;
      }
      i += step;
      if (i > len) i = len;
      charCount++;
    }

    if (outLines && totalLines >= startLine && totalLines < startLine + 3) {
      String cleanLine = raw.substring(lineStart, i);
      cleanLine.trim();
      outLines[totalLines - startLine] = cleanLine;
    }
    totalLines++;

    while (i < len && (raw[i] == ' ' || raw[i] == '\r')) ++i;
    if (i < len && raw[i] == '\n') ++i;
  }
  return totalLines == 0 ? 1 : totalLines;
}

int64_t weekMondayEpochDay() {
  if (!rtcReady) return 0;
  const DateTime now = rtc.now();
  const int64_t today = deskbuddy::daysFromCivil(now.year(), now.month(), now.day());
  const int mondayOffset = (now.dayOfTheWeek() + 6) % 7; // RTClib: Sunday = 0.
  return today - mondayOffset;
}

int64_t selectedCalendarEpochDay() {
  return weekMondayEpochDay() + navigation.dayOffset;
}

int selectedDayEventCount() {
  const int64_t selectedDay = selectedCalendarEpochDay();
  int count = 0;
  for (size_t i = 0; i < eventCount; ++i) {
    if (eventLocalDay(events[i]) == selectedDay) ++count;
  }
  return count;
}

int selectedEventArrayIndex() {
  if (!rtcReady || eventCount == 0) return -1;
  const int64_t target = selectedCalendarEpochDay();
  uint8_t ordinal = 0;
  for (size_t i = 0; i < eventCount; ++i) {
    if (eventLocalDay(events[i]) != target) continue;
    if (ordinal++ == navigation.eventIndex) return static_cast<int>(i);
  }
  return -1;
}

void moveCalendarCursor(int direction) {
  navigation.dayOffset = static_cast<uint8_t>((navigation.dayOffset + 7 + direction) % 7);
  navigation.eventIndex = 0;
  navigation.notePage = 0;
}

void moveNoteCursor(int direction) {
  const int count = selectedDayEventCount();
  if (count == 0) return;
  navigation.eventIndex = static_cast<uint8_t>((navigation.eventIndex + count + direction) % count);
  navigation.notePage = 0;
  showingEventNote = false;
  lastEventNoteFlip = millis();
}

String urlEncode(const String& value) {
  String encoded;
  const char hex[] = "0123456789ABCDEF";
  for (size_t i = 0; i < value.length(); i++) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += static_cast<char>(c);
    } else {
      encoded += '%';
      encoded += hex[c >> 4];
      encoded += hex[c & 0x0F];
    }
  }
  return encoded;
}

void setStatusLed(bool red, bool green, bool blue) {
  digitalWrite(RGB_RED, red ? HIGH : LOW);
  digitalWrite(RGB_GREEN, green ? HIGH : LOW);
  digitalWrite(RGB_BLUE, blue ? HIGH : LOW);
}

void showLcdMessage(const char* first, const char* second) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(first);
  lcd.setCursor(0, 1);
  lcd.print(second);
}

String htmlEscape(const String& value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    switch (value[i]) {
      case '&': escaped += F("&amp;"); break;
      case '<': escaped += F("&lt;"); break;
      case '>': escaped += F("&gt;"); break;
      case '\"': escaped += F("&quot;"); break;
      case '\'': escaped += F("&#39;"); break;
      default: escaped += value[i]; break;
    }
  }
  return escaped;
}

String wifiConfigPage(const String& message = "") {
  const int count = WiFi.scanNetworks(false, true);
  String options;
  for (int i = 0; i < count; ++i) {
    const String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    options += F("<option value=\"");
    options += htmlEscape(ssid);
    options += F("\">");
    options += htmlEscape(ssid);
    options += F(" (");
    options += String(WiFi.RSSI(i));
    options += F(" dBm)</option>");
  }
  if (!options.length()) options = F("<option value=\"\">Không tìm thấy mạng; thử quét lại</option>");

  String page = F("<!doctype html><html lang=\"vi\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  page += F("<title>Cài Wi-Fi Desk Buddy</title><style>body{font:16px system-ui;max-width:440px;margin:36px auto;padding:0 18px;color:#222}select,input,button{box-sizing:border-box;width:100%;padding:12px;margin:8px 0 16px;font-size:16px}button{background:#222;color:white;border:0;border-radius:6px}</style>");
  page += F("<h1>Cài Wi-Fi Desk Buddy</h1><p>Chọn mạng Wi-Fi và nhập mật khẩu. Mật khẩu chỉ lưu trên thiết bị.</p><p>");
  page += htmlEscape(message);
  page += F("</p><form method=\"post\" action=\"/save\"><label>Mạng Wi-Fi</label><select name=\"ssid\" required>");
  page += options;
  page += F("</select><label>Mật khẩu Wi-Fi</label><input name=\"password\" type=\"password\" autocomplete=\"current-password\"><button type=\"submit\">Lưu và kết nối</button></form><form method=\"get\"><button>Quét lại mạng</button></form></html>");
  return page;
}

void showWifiSetupScreen() {
  epaper.setRotation(1);
  epaper.setFullWindow();
  epaper.firstPage();
  do {
    epaper.fillScreen(GxEPD_WHITE);
    epaperText.begin(epaper);
    epaperText.setFontMode(1);
    epaperText.setForegroundColor(GxEPD_BLACK);
    epaperText.setBackgroundColor(GxEPD_WHITE);
    epaperText.setFont(u8g2_font_unifont_t_vietnamese2);
    epaperText.setCursor(8, 20); epaperText.print("Cai dat Wi-Fi");
    epaperText.setCursor(8, 47); epaperText.print("Ket noi: "); epaperText.print(wifiApName);
    epaperText.setCursor(8, 73); epaperText.print("Mat khau: "); epaperText.print(wifiApPassword);
    epaperText.setCursor(8, 100); epaperText.print("Mo trinh duyet: 192.168.4.1");
    epaperText.setCursor(8, 122); epaperText.print("Nhap Wi-Fi nha cua ban");
  } while (epaper.nextPage());
  epaper.hibernate();
  showLcdMessage("Cai dat Wi-Fi", "192.168.4.1");
}

void startWifiSetup() {
  wifiApName = String("DeskBuddy-") + DEVICE_ID;
  char generatedPassword[11];
  snprintf(generatedPassword, sizeof(generatedPassword), "%010lu",
           static_cast<unsigned long>(esp_random() % 10000000000ULL));
  wifiApPassword = generatedPassword;
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(wifiApName.c_str(), wifiApPassword.c_str());

  wifiConfigServer.on("/", HTTP_GET, []() {
    wifiConfigServer.send(200, "text/html; charset=utf-8", wifiConfigPage());
  });
  wifiConfigServer.on("/save", HTTP_POST, []() {
    const String ssid = wifiConfigServer.arg("ssid");
    const String password = wifiConfigServer.arg("password");
    if (!ssid.length() || ssid.length() > 32 || password.length() > 63) {
      wifiConfigServer.send(400, "text/html; charset=utf-8", wifiConfigPage("Tên mạng hoặc mật khẩu không hợp lệ."));
      return;
    }
    if (!wifiPreferences.begin("wifi", false)) {
      wifiConfigServer.send(500, "text/plain; charset=utf-8", "Không lưu được cấu hình Wi-Fi.");
      return;
    }
    wifiPreferences.putString("ssid", ssid);
    wifiPreferences.putString("password", password);
    wifiPreferences.end();
    wifiConfigServer.send(200, "text/html; charset=utf-8",
      "<!doctype html><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width\"><h2>Đã lưu Wi-Fi</h2><p>Desk Buddy đang khởi động lại và kết nối mạng.</p>");
    delay(1200);
    ESP.restart();
  });
  wifiConfigServer.onNotFound([]() {
    wifiConfigServer.sendHeader("Location", "http://192.168.4.1/");
    wifiConfigServer.send(302, "text/plain", "");
  });
  wifiConfigServer.begin();
  Serial.printf("Wi-Fi setup AP: %s, address: 192.168.4.1\n", wifiApName.c_str());
  showWifiSetupScreen();
  while (true) {
    wifiConfigServer.handleClient();
    delay(5);
  }
}

bool connectSavedWifi() {
  if (wifiPreferences.begin("wifi", true)) {
    wifiSsid = wifiPreferences.getString("ssid", "");
    wifiPassword = wifiPreferences.getString("password", "");
    wifiPreferences.end();
  }
#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
  if (!wifiSsid.length() && strlen(WIFI_SSID) > 0) {
    wifiSsid = WIFI_SSID;
    wifiPassword = WIFI_PASSWORD;
  }
#endif
  if (!wifiSsid.length()) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS) delay(100);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Wi-Fi connected: %s\n", wifiSsid.c_str());
    return true;
  }
  Serial.println("Saved Wi-Fi did not connect; opening setup portal.");
  return false;
}

void stopPomodoroBuzzer() {
  buzzerSignalActive = false;
  buzzerSignalOn = false;
  buzzerContinuous = false;
  buzzerPulsesRemaining = 0;
  digitalWrite(BUZZER_PIN, LOW);
}

void startBreakBuzzer(uint32_t now) {
  buzzerSignalActive = true;
  buzzerSignalOn = true;
  buzzerContinuous = false;
  buzzerPulsesRemaining = 2;
  buzzerSignalDeadline = now + BUZZER_DOUBLE_PULSE_MS;
  digitalWrite(BUZZER_PIN, HIGH);
}

void startFinishedBuzzer(uint32_t now) {
  buzzerSignalActive = true;
  buzzerSignalOn = true;
  buzzerContinuous = true;
  buzzerPulsesRemaining = 0;
  buzzerSignalDeadline = now + BUZZER_FINISH_MS;
  digitalWrite(BUZZER_PIN, HIGH);
}

void updatePomodoroBuzzer(uint32_t now) {
  if (!buzzerSignalActive || static_cast<int32_t>(now - buzzerSignalDeadline) < 0) return;
  if (buzzerContinuous) {
    stopPomodoroBuzzer();
  } else if (buzzerSignalOn) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerSignalOn = false;
    if (buzzerPulsesRemaining > 0) --buzzerPulsesRemaining;
    if (buzzerPulsesRemaining == 0) buzzerSignalActive = false;
    else buzzerSignalDeadline = now + BUZZER_GAP_MS;
  } else {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerSignalOn = true;
    buzzerSignalDeadline = now + BUZZER_DOUBLE_PULSE_MS;
  }
}

void startPomodoro(uint32_t now) {
  navigation.pomodoroRunning = true;
  pomodoroCyclesRemaining = pomodoroCycles;
  pomodoroOnBreak = false;
  pomodoroDeadlineMs = now + static_cast<uint32_t>(navigation.pomodoroMinutes) * 60000UL;
  lastPomodoroDisplayMinute = navigation.pomodoroMinutes;
}

void finishPomodoro(uint32_t now) {
  navigation.pomodoroRunning = false;
  pomodoroCyclesRemaining = 0;
  pomodoroOnBreak = false;
  pomodoroSetupStep = 0;
  navigation.screen = deskbuddy::Screen::Pomodoro;
  startFinishedBuzzer(now);
  drawCalendar();
}

void updatePomodoro(uint32_t now) {
  updatePomodoroBuzzer(now);
  if (!navigation.pomodoroRunning) return;

  // Khi hết thời gian hiệp hiện tại -> chuyển sang Nghỉ hoặc Chu kỳ kế tiếp
  if (static_cast<int32_t>(now - pomodoroDeadlineMs) >= 0) {
    if (!pomodoroOnBreak) {
      if (pomodoroCyclesRemaining <= 1) {
        finishPomodoro(now);
        return;
      }
      pomodoroOnBreak = true;
      pomodoroDeadlineMs = now + static_cast<uint32_t>(pomodoroBreakMinutes) * 60000UL;
      startBreakBuzzer(now);
    } else {
      if (pomodoroCyclesRemaining > 0) --pomodoroCyclesRemaining;
      pomodoroOnBreak = false;
      pomodoroDeadlineMs = now + static_cast<uint32_t>(navigation.pomodoroMinutes) * 60000UL;
      startBreakBuzzer(now);
    }
    lastPomodoroDisplayMinute = UINT32_MAX; // Ép vẽ lại ngay khi chuyển hiệp
  }

  // Refresh màn hình E-Paper đúng mỗi khi số phút còn lại giảm đi 1
  if (navigation.screen == deskbuddy::Screen::Pomodoro) {
    const uint32_t remainingMs = static_cast<int32_t>(pomodoroDeadlineMs - now) > 0
      ? pomodoroDeadlineMs - now : 0;
    const uint32_t displayMinute = (remainingMs + 59999UL) / 60000UL;
    if (displayMinute != lastPomodoroDisplayMinute) {
      lastPomodoroDisplayMinute = displayMinute;
      drawCalendar();
    }
  }
}

void drawCalendar() {
  epaper.setRotation(1); // Landscape: 296 x 128.
  epaper.setFullWindow();
  epaper.firstPage();
  do {
    epaper.fillScreen(GxEPD_WHITE);
    epaper.drawRect(0, 0, epaper.width(), epaper.height(), GxEPD_BLACK);
    epaperText.begin(epaper);
    epaperText.setFontMode(1);
    epaperText.setFontDirection(0);
    epaperText.setForegroundColor(GxEPD_BLACK);
    epaperText.setBackgroundColor(GxEPD_WHITE);
    epaperText.setFont(u8g2_font_unifont_t_vietnamese2);
    if (navigation.screen != deskbuddy::Screen::Account) {
      epaperText.setCursor(8, 18);
      switch (navigation.screen) {
        case deskbuddy::Screen::Calendar: epaperText.print("Lich tuan"); break;
        case deskbuddy::Screen::Note: epaperText.print("Ghi chu su kien"); break;
        case deskbuddy::Screen::Pomodoro: epaperText.print("Pomodoro"); break;
        default: break;
      }
      epaper.drawLine(6, 23, epaper.width() - 6, 23, GxEPD_BLACK);
    }

    if (navigation.screen == deskbuddy::Screen::Pomodoro) {
      if (navigation.pomodoroRunning) {
        const uint32_t remainingMs = static_cast<int32_t>(pomodoroDeadlineMs - millis()) > 0
          ? pomodoroDeadlineMs - millis() : 0;
        const uint32_t displayMinute = (remainingMs + 59999UL) / 60000UL;
        lastPomodoroDisplayMinute = displayMinute;

        char countdown[20];
        snprintf(countdown, sizeof(countdown), "Còn: %02lu phut", static_cast<unsigned long>(displayMinute));

        epaperText.setCursor(12, 49); epaperText.print("Chu ky con: "); epaperText.print(pomodoroCyclesRemaining);
        epaperText.setCursor(12, 72); epaperText.print(pomodoroOnBreak ? "Thoi gian nghi" : "Thoi gian lam");
        epaperText.setCursor(12, 105); epaperText.print(countdown);
        epaperText.setCursor(142, 105); epaperText.print("MODE: doi che do");
      } else {
        epaperText.setCursor(12, 47); epaperText.print(pomodoroSetupStep == 0 ? "> Chu ky: " : "  Chu ky: ");
        epaperText.print(pomodoroCycles);
        epaperText.setCursor(12, 70); epaperText.print(pomodoroSetupStep == 1 ? "> Lam viec: " : "  Lam viec: ");
        epaperText.print(navigation.pomodoroMinutes); epaperText.print(" phut");
        epaperText.setCursor(12, 93); epaperText.print(pomodoroSetupStep == 2 ? "> Nghi: " : "  Nghi: ");
        epaperText.print(pomodoroBreakMinutes); epaperText.print(" phut");
        epaperText.setCursor(12, 118); epaperText.print("UP/DOWN sua  OK tiep/bat dau");
      }
    } else if (navigation.screen == deskbuddy::Screen::Account) {
      String link = String(WEB_APP_URL);
      link += link.indexOf('?') >= 0 ? '&' : '?';
      link += "device_id=";
      link += urlEncode(DEVICE_ID);
      const int8_t qrStatus = qrcode_initText(&accountQr, accountQrData, 8, ECC_LOW, link.c_str());
      const int moduleScale = 2;
      const int qrOffsetX = 12;
      const int qrOffsetY = 15;
      for (uint8_t y = 0; qrStatus == 0 && y < accountQr.size; ++y) {
        for (uint8_t x = 0; x < accountQr.size; ++x) {
          if (qrcode_getModule(&accountQr, x, y)) {
            epaper.fillRect(qrOffsetX + x * moduleScale, qrOffsetY + y * moduleScale,
                            moduleScale, moduleScale, GxEPD_BLACK);
          }
        }
      }
      // Chuyển sang phông chữ nhỏ 6x13 px để vừa khít cột bên phải
      epaperText.setFont(u8g2_font_6x13_tf);
      if (qrStatus == 0) {
        epaperText.setCursor(124, 28); epaperText.print("Tai khoan Google");
        epaperText.setCursor(124, 50); epaperText.print("Quet QR bang dien thoai");
        epaperText.setCursor(124, 68); epaperText.print("Chon tai khoan Google");
        epaperText.setCursor(124, 86); epaperText.print("de cap quyen doc lich");
        epaperText.setCursor(124, 110); epaperText.print("OK: cai dat Wi-Fi");
      } else {
        epaperText.setCursor(124, 36); epaperText.print("Khong tao duoc QR");
        epaperText.setCursor(124, 58); epaperText.print("Kiem tra WEB_APP_URL");
        epaperText.setCursor(124, 80); epaperText.print("URL can ngan hon");
        epaperText.setCursor(124, 106); epaperText.print("OK: cai dat Wi-Fi");
      }

    } else if (navigation.screen == deskbuddy::Screen::Calendar) {
      static const char* weekdayLabels[7] = {"T2", "T3", "T4", "T5", "T6", "T7", "CN"};
      int weekYear;
      unsigned weekMonth, weekDate;
      const int64_t monday = weekMondayEpochDay();
      deskbuddy::civilFromDays(monday, weekYear, weekMonth, weekDate);
      char header[32];
      snprintf(header, sizeof(header), "Tuan %02u/%02u - %02u/%02u",
               weekDate, weekMonth,
               static_cast<unsigned>(weekDate + 6 > deskbuddy::daysInMonth(weekYear, static_cast<int>(weekMonth))
                 ? weekDate + 6 - deskbuddy::daysInMonth(weekYear, static_cast<int>(weekMonth))
                 : weekDate + 6),
               static_cast<unsigned>(weekDate + 6 > deskbuddy::daysInMonth(weekYear, static_cast<int>(weekMonth))
                 ? (weekMonth == 12 ? 1 : weekMonth + 1) : weekMonth));
      epaperText.setCursor(112, 18); epaperText.print(header);
      for (uint8_t day = 0; day < 7; ++day) {
        const int x = 7 + day * 40;
        const int64_t dateEpoch = monday + day;
        int year; unsigned month, date;
        deskbuddy::civilFromDays(dateEpoch, year, month, date);
        uint8_t eventCountForDay = 0;
        for (size_t i = 0; i < eventCount; ++i) if (eventLocalDay(events[i]) == dateEpoch) ++eventCountForDay;
        if (day == navigation.dayOffset) {
          epaper.fillRect(x, 30, 39, 70, GxEPD_BLACK);
          epaperText.setForegroundColor(GxEPD_WHITE);
          epaperText.setBackgroundColor(GxEPD_BLACK);
        } else {
          epaper.drawRect(x, 30, 39, 70, GxEPD_BLACK);
          epaperText.setForegroundColor(GxEPD_BLACK);
          epaperText.setBackgroundColor(GxEPD_WHITE);
        }
        epaperText.setCursor(x + 5, 49); epaperText.print(weekdayLabels[day]);
        epaperText.setCursor(x + 7, 73); epaperText.print(date);
        epaperText.setCursor(x + 5, 92);
        if (eventCountForDay) { epaperText.print(eventCountForDay); epaperText.print(" SK"); }
        else epaperText.print("-");
      }
      epaperText.setForegroundColor(GxEPD_BLACK);
      epaperText.setBackgroundColor(GxEPD_WHITE);
      epaperText.setCursor(8, 119); epaperText.print("UP/DOWN: ngay  OK: xem");
      if (!calendarLoaded) {
        epaperText.setCursor(190, 119); epaperText.print(WiFi.status() == WL_CONNECTED ? "Tai..." : "Offline");
      }
    } else if (!calendarLoaded) {
      epaperText.setCursor(8, 56);
      epaperText.print(WiFi.status() == WL_CONNECTED ? "Dang tai lich..." : "Chua co du lieu lich");
      epaperText.setCursor(8, 119); epaperText.print("OK: ve lich tuan");
    } else {
      const int selected = selectedEventArrayIndex();
      if (selected < 0) {
        epaperText.setCursor(8, 60); epaperText.print("Ngay nay khong co su kien");
        epaperText.setCursor(8, 119); epaperText.print("OK: ve lich tuan");
      } else {
        int year; unsigned month, date;
        deskbuddy::civilFromDays(selectedCalendarEpochDay(), year, month, date);
        char noteHeader[40];
        if (showingEventNote) {
          snprintf(noteHeader, sizeof(noteHeader), "%02u/%02u  SU KIEN %u/%u - GHI CHU",
                   date, month, navigation.eventIndex + 1, selectedDayEventCount());
          epaperText.setCursor(8, 39); epaperText.print(noteHeader);

          const String description = events[selected].description;
          if (!description.length()) {
            epaperText.setCursor(8, 74); epaperText.print("Khong co ghi chu");
          } else {
            String pageLines[3];
            const size_t startLine = static_cast<size_t>(navigation.notePage) * 3;
            const size_t totalLines = extractNotePageLines(description, startLine, pageLines, 32);
            const size_t totalPages = (totalLines + 2) / 3;

            for (int line = 0; line < 3; ++line) {
              if (pageLines[line].length() == 0) continue;
              epaperText.setCursor(8, 58 + line * 18);
              epaperText.print(pageLines[line]);
            }

            epaperText.setCursor(8, 120);
            epaperText.print("Trang ");
            epaperText.print(navigation.notePage + 1);
            epaperText.print("/");
            epaperText.print(totalPages);
          }
        } else {
          const String timeStr = formatEventLocalTime(events[selected]);
          snprintf(noteHeader, sizeof(noteHeader), "%02u/%02u  SU KIEN %u/%u (%s)",
                   date, month, navigation.eventIndex + 1, selectedDayEventCount(), timeStr.c_str());
          epaperText.setCursor(8, 39); epaperText.print(noteHeader);

          String titleLines[3];
          extractNotePageLines(events[selected].title, 0, titleLines, 32);
          epaperText.setCursor(8, 66); epaperText.print(titleLines[0]);
          if (titleLines[1].length() > 0) {
            epaperText.setCursor(8, 86); epaperText.print(titleLines[1]);
          }
        }
        epaperText.setCursor(116, 120); epaperText.print("UP/DOWN: doi OK: lich");
      }
    }
  } while (epaper.nextPage());
  epaper.hibernate();

  if (navigation.screen == deskbuddy::Screen::Note) {
    lastEventNoteFlip = millis();
  }
}

void checkReminder() {
  if (!rtcReady || !calendarLoaded || eventCount == 0) return;
  const int64_t now = static_cast<int64_t>(rtc.now().unixtime());
  reminderActive = false;
  for (size_t i = 0; i < eventCount; i++) {
    int64_t start = 0;
    if (!deskbuddy::parseEventLocalEpoch(std::string(events[i].start.c_str()), LOCAL_UTC_OFFSET_SECONDS, start)) continue;
    if (deskbuddy::isWithinReminderWindow(start, now, REMINDER_WINDOW_SECONDS)) {
      reminderActive = true;
      if (deskbuddy::shouldBeepForEvent(std::string(events[i].start.c_str()), std::string(lastReminderKey.c_str()))) {
        lastReminderKey = events[i].start;
        setStatusLed(true, false, false);
        if (!buzzerSignalActive) {
          digitalWrite(BUZZER_PIN, HIGH);
          delay(250);
          digitalWrite(BUZZER_PIN, LOW);
        }
      }
    }
  }
  setStatusLed(reminderActive, !reminderActive, false);
}

void fetchCalendar() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setCACert(API_ROOT_CA_PEM);
  HTTPClient http;
  String url = String(API_BASE_URL) + "/api/calendar?device_id=" + urlEncode(DEVICE_ID);
  if (!http.begin(client, url)) {
    Serial.println("[CAL] Loi khoi tao URL");
    showLcdMessage("Calendar loi URL", "Kiem tra API URL");
    return;
  }
  http.addHeader("Authorization", String("Bearer ") + DEVICE_API_TOKEN);
  http.addHeader("Accept", "application/json");
  http.setTimeout(12000);

  const int status = http.GET();
  Serial.printf("[CAL] HTTP status: %d\n", status);
  if (status == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, payload);
    if (!error && document["device_id"].as<String>() == DEVICE_ID) {
      eventCount = 0;
      JsonArray sourceEvents = document["events"].as<JsonArray>();
      Serial.printf("[CAL] Server tra ve %u su kien\n", static_cast<unsigned>(sourceEvents.size()));
      for (JsonObject source : sourceEvents) {
        if (eventCount >= MAX_EVENTS) {
          Serial.println("[CAL] Da cham gioi han MAX_EVENTS!");
          break;
        }
        events[eventCount].title = source["title"] | "(Khong co tieu de)";
        events[eventCount].start = source["start"] | "";
        events[eventCount].description = source["description"] | "";
        Serial.printf("  #%u: [%s] %s\n", static_cast<unsigned>(eventCount + 1),
                      events[eventCount].start.c_str(), events[eventCount].title.c_str());
        eventCount++;
      }
      calendarLoaded = true;
      drawCalendar();
      setStatusLed(false, true, false);
    } else {
      Serial.printf("[CAL] Loi JSON (%s) hoac sai device_id. Raw: %s\n",
                    error.c_str(), payload.substring(0, 200).c_str());
      showLcdMessage("JSON Calendar loi", "Kiem tra API");
    }
  } else if (status == HTTP_CODE_UNAUTHORIZED) {
    Serial.println("[CAL] Loi 401: Sai Token hoac chua lien ket Google");
    showLcdMessage("Token/API loi", "Kiem tra ket noi");
  } else {
    Serial.printf("[CAL] Loi API: %d\n", status);
    showLcdMessage("Calendar offline", "Se thu lai sau");
  }
  http.end();
}

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (!wifiSsid.length()) return;
  const uint32_t now = millis();
  if (lastWifiAttempt != 0 && now - lastWifiAttempt < WIFI_RETRY_MS) return;
  lastWifiAttempt = now;
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
}

void syncRtcFromNtp() {
  if (!rtcReady || WiFi.status() != WL_CONNECTED) return;
  configTzTime("ICT-7", "pool.ntp.org", "time.google.com");
  struct tm localTime;
  if (getLocalTime(&localTime, 8000)) {
    rtc.adjust(DateTime(localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
                        localTime.tm_hour, localTime.tm_min, localTime.tm_sec));
    ntpSynced = true;
    if (!restoreWeekDay) {
      const DateTime now = rtc.now();
      navigation.dayOffset = static_cast<uint8_t>((now.dayOfTheWeek() + 6) % 7);
    }
  }
}

void updateLcd() {
  if (!rtcReady) {
    showLcdMessage("RTC chua san sang", "Kiem tra DS3231");
    return;
  }
  const DateTime now = rtc.now();
  char first[17];
  char secondRaw[17];
  char second[17];

  snprintf(first, sizeof(first), "%02d/%02d/%04d      ", now.day(), now.month(), now.year());
  if (WiFi.status() == WL_CONNECTED) {
    snprintf(secondRaw, sizeof(secondRaw), "%02d:%02d:%02d Online", now.hour(), now.minute(), now.second());
  } else {
    snprintf(secondRaw, sizeof(secondRaw), "%02d:%02d:%02d Offline", now.hour(), now.minute(), now.second());
  }
  snprintf(second, sizeof(second), "%-16s", secondRaw);

  lcd.setCursor(0, 0);
  lcd.print(first);
  lcd.setCursor(0, 1);
  lcd.print(second);
}

void setup() {
  Serial.begin(115200);
  pinMode(RGB_RED, OUTPUT);
  pinMode(RGB_GREEN, OUTPUT);
  pinMode(RGB_BLUE, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_MODE, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_OK, INPUT);
  loadNoteState();
  setStatusLed(false, false, false);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();
  showLcdMessage("Desk Buddy", "Dang khoi dong");

  rtcReady = rtc.begin();
  if (rtcReady && !restoreWeekDay) {
    const DateTime now = rtc.now();
    navigation.dayOffset = static_cast<uint8_t>((now.dayOfTheWeek() + 6) % 7);
  }
  if (rtcReady && rtc.lostPower()) {
    showLcdMessage("RTC mat gio", "Dang cho Wi-Fi");
  }

  SPI.begin(18, -1, 23, EPD_CS);
  epaper.init(115200);
  epaperText.begin(epaper);
  drawCalendar();

  if (!connectSavedWifi()) startWifiSetup();
  if (WiFi.status() == WL_CONNECTED) syncRtcFromNtp();
  updateLcd();
  fetchCalendar();
  lastCalendarFetch = millis();
  lastNoteInteraction = millis();
  lastEventNoteFlip = millis();
}

void loop() {
  connectWifi();
  const uint32_t now = millis();
  const int buttonPins[4] = {BUTTON_MODE, BUTTON_UP, BUTTON_DOWN, BUTTON_OK};
  bool buttonPressed[4] = {false, false, false, false};
  for (int i = 0; i < 4; ++i) {
    const bool isHigh = digitalRead(buttonPins[i]) == HIGH;
    buttonPressed[i] = previousButtonsHigh[i] && !isHigh;
    previousButtonsHigh[i] = isHigh;
  }
  if (now - lastButtonAt > 60) {
    bool changed = false;
    if (buttonPressed[0]) {
      deskbuddy::nextScreen(navigation);
      if (navigation.screen == deskbuddy::Screen::Note) {
        lastNoteInteraction = now;
        lastEventNoteFlip = now;
        showingEventNote = false;
      }
      changed = true;
    }
    else if (buttonPressed[1] || buttonPressed[2]) {
      const int direction = buttonPressed[1] ? -1 : 1;
      if (navigation.screen == deskbuddy::Screen::Pomodoro && !navigation.pomodoroRunning) {
        if (pomodoroSetupStep == 0) {
          if (direction < 0 && pomodoroCycles < 12) ++pomodoroCycles;
          else if (direction > 0 && pomodoroCycles > 1) --pomodoroCycles;
        } else if (pomodoroSetupStep == 1) {
          if (direction < 0 && navigation.pomodoroMinutes < 120) navigation.pomodoroMinutes += 10;
          else if (direction > 0 && navigation.pomodoroMinutes > 10) navigation.pomodoroMinutes -= 10;
        } else {
          if (direction < 0 && pomodoroBreakMinutes < 60) pomodoroBreakMinutes += 5;
          else if (direction > 0 && pomodoroBreakMinutes > 5) pomodoroBreakMinutes -= 5;
        }
        changed = true;
      } else if (navigation.screen == deskbuddy::Screen::Note) {
        moveNoteCursor(direction);
        lastNoteInteraction = now;
        changed = true;
      } else if (navigation.screen == deskbuddy::Screen::Calendar) {
        moveCalendarCursor(direction);
        changed = true;
      }
    }
    else if (buttonPressed[3]) {
      if (navigation.screen == deskbuddy::Screen::Account) {
        startWifiSetup();
      } else if (navigation.screen == deskbuddy::Screen::Pomodoro) {
        if (!navigation.pomodoroRunning) {
          if (pomodoroSetupStep < 2) {
            ++pomodoroSetupStep;
          } else {
            startPomodoro(now);
          }
          changed = true;
        }
      } else if (navigation.screen == deskbuddy::Screen::Calendar) {
        navigation.eventIndex = 0;
        navigation.notePage = 0;
        showingEventNote = false;
        lastNoteInteraction = now;
        lastEventNoteFlip = now;
        deskbuddy::confirm(navigation);
        changed = true;
      } else if (navigation.screen == deskbuddy::Screen::Note) {
        deskbuddy::confirm(navigation);
        showingEventNote = false;
        changed = true;
      }
    }
    if (changed) { lastButtonAt = now; queueNoteStateSave(); drawCalendar(); }
  }

  saveNoteStateIfIdle(now);

  if (navigation.screen == deskbuddy::Screen::Note) {
    if (static_cast<uint32_t>(now - lastNoteInteraction) >= NOTE_IDLE_RESET_MS) {
      navigation.screen = deskbuddy::Screen::Calendar;
      showingEventNote = false;
      navigation.eventIndex = 0;
      queueNoteStateSave();
      drawCalendar();
    } else if (selectedEventArrayIndex() >= 0 && static_cast<uint32_t>(now - lastEventNoteFlip) >= EVENT_NOTE_FLIP_MS) {
      const int selected = selectedEventArrayIndex();
      if (selected >= 0 && showingEventNote) {
        const size_t totalLines = extractNotePageLines(events[selected].description, 0, nullptr, 32);
        const size_t totalPages = (totalLines + 2) / 3;
        navigation.notePage = static_cast<uint8_t>((navigation.notePage + 1) % totalPages);
        queueNoteStateSave();
      }
      showingEventNote = !showingEventNote;
      drawCalendar();
      lastNoteInteraction = millis();
    }
  }

  updatePomodoro(now);

  if (now - lastLcdUpdate >= 1000) {
    lastLcdUpdate = now;
    updateLcd();
  }

  const uint32_t fetchInterval = calendarLoaded ? CALENDAR_REFRESH_MS : CALENDAR_RETRY_MS;
  if (now - lastCalendarFetch >= fetchInterval) {
    lastCalendarFetch = now;
    if (WiFi.status() == WL_CONNECTED) {
      if (!ntpSynced) syncRtcFromNtp();
      fetchCalendar();
    }
  }
  checkReminder();
  delay(20);
}