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
#include <Wire.h>
#include <esp_sleep.h>

#include "config.h"
#include "calendar_logic.h"
#include "interaction.h"

// WeAct Studio 2.9-inch monochrome panel, common SSD1680/GDEY029T94 variant.
// Check the panel revision on the rear label if the display does not initialize.
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
constexpr uint32_t WIFI_RETRY_MS = 10UL * 1000UL;
constexpr size_t MAX_EVENTS = 8;
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
bool calendarLoaded = false;
bool reminderActive = false;
deskbuddy::NavigationState navigation;
uint32_t lastButtonAt = 0;
uint32_t modePressedAt = 0;
bool previousButtonsHigh[4] = {true, true, true, true};
uint32_t pomodoroDeadlineMs = 0;
uint32_t pomodoroRemainingSeconds = 25UL * 60UL;
bool pomodoroHasStarted = false;
Preferences uiPreferences;
bool noteStateDirty = false;
uint32_t noteStateChangedAt = 0;

void loadNoteState() {
  if (!uiPreferences.begin("deskbuddy", false)) return;
  if (uiPreferences.getBool("noteSaved", false)) {
    navigation.screen = deskbuddy::Screen::Note;
    navigation.dayOffset = uiPreferences.getUChar("noteDay", 0) > 1 ? 0 : uiPreferences.getUChar("noteDay", 0);
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
  uiPreferences.putUChar("noteDay", navigation.dayOffset);
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

int selectedEventArrayIndex() {
  if (!rtcReady || eventCount == 0) return -1;
  const DateTime now = rtc.now();
  const int64_t target = deskbuddy::daysFromCivil(now.year(), now.month(), now.day()) + navigation.dayOffset;
  uint8_t ordinal = 0;
  for (size_t i = 0; i < eventCount; ++i) {
    if (eventLocalDay(events[i]) != target) continue;
    if (ordinal++ == navigation.eventIndex) return static_cast<int>(i);
  }
  return -1;
}

void moveCalendarCursor(int direction) {
  if (!rtcReady) return;
  const DateTime now = rtc.now();
  const int64_t today = deskbuddy::daysFromCivil(now.year(), now.month(), now.day());
  int counts[2] = {0, 0};
  for (size_t i = 0; i < eventCount; ++i) {
    const int64_t day = eventLocalDay(events[i]);
    if (day == today) ++counts[0]; else if (day == today + 1) ++counts[1];
  }
  const int current = navigation.dayOffset;
  const int count = counts[current];
  if (count == 0) { navigation.dayOffset = current == 0 ? 1 : 0; navigation.eventIndex = 0; }
  else if (direction > 0 && navigation.eventIndex + 1 < count) ++navigation.eventIndex;
  else if (direction < 0 && navigation.eventIndex > 0) --navigation.eventIndex;
  else { navigation.dayOffset = current == 0 ? 1 : 0; navigation.eventIndex = direction > 0 ? 0 : (counts[navigation.dayOffset] ? counts[navigation.dayOffset] - 1 : 0); }
  navigation.notePage = 0;
}

void moveNoteCursor(int direction) {
  const int index = selectedEventArrayIndex();
  if (index < 0) { moveCalendarCursor(direction); return; }
  const size_t descriptionLength = events[index].description.length();
  const uint8_t pages = descriptionLength ? (descriptionLength + 83) / 84 : 1;
  if (direction > 0 && navigation.notePage + 1 < pages) ++navigation.notePage;
  else if (direction < 0 && navigation.notePage > 0) --navigation.notePage;
  else moveCalendarCursor(direction);
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
    epaperText.setCursor(8, 18);
    switch (navigation.screen) {
      case deskbuddy::Screen::Calendar: epaperText.print(navigation.dayOffset ? "Lich - Ngay mai" : "Lich - Hom nay"); break;
      case deskbuddy::Screen::Note: epaperText.print("Ghi chu su kien"); break;
      case deskbuddy::Screen::Pomodoro: epaperText.print("Pomodoro"); break;
      case deskbuddy::Screen::Account: epaperText.print("Tai khoan Google"); break;
    }
    epaper.drawLine(6, 23, epaper.width() - 6, 23, GxEPD_BLACK);

    if (navigation.screen == deskbuddy::Screen::Pomodoro) {
      epaperText.setCursor(12, 58); epaperText.print(navigation.pomodoroRunning ? "Dang tap trung" : "San sang tap trung");
      epaperText.setCursor(12, 84); epaperText.print("Thoi luong: "); epaperText.print(navigation.pomodoroMinutes); epaperText.print(" phut");
      epaperText.setCursor(12, 108); epaperText.print("OK: bat/tam dung UP/DOWN: chinh");
    } else if (navigation.screen == deskbuddy::Screen::Account) {
      String link = String(WEB_APP_URL);
      link += link.indexOf('?') >= 0 ? '&' : '?';
      link += "device_id=";
      link += urlEncode(DEVICE_ID);
      const int8_t qrStatus = qrcode_initText(&accountQr, accountQrData, 8, ECC_LOW, link.c_str());
      const int moduleScale = 2;
      const int quiet = 4;
      epaper.fillRect(4, 7, 120, 114, GxEPD_WHITE);
      for (uint8_t y = 0; qrStatus == 0 && y < accountQr.size; ++y) {
        for (uint8_t x = 0; x < accountQr.size; ++x) {
          if (qrcode_getModule(&accountQr, x, y)) {
            epaper.fillRect(4 + (x + quiet) * moduleScale, 7 + (y + quiet) * moduleScale,
                            moduleScale, moduleScale, GxEPD_BLACK);
          }
        }
      }
      if (qrStatus == 0) {
        epaperText.setCursor(132, 48); epaperText.print("Quet QR bang dien thoai");
        epaperText.setCursor(132, 68); epaperText.print("Chon tai khoan Google");
        epaperText.setCursor(132, 88); epaperText.print("de cap quyen doc lich");
        epaperText.setCursor(132, 112); epaperText.print("Thiet bi: "); epaperText.print(DEVICE_ID);
      } else {
        epaperText.setCursor(132, 52); epaperText.print("Khong tao duoc QR");
        epaperText.setCursor(132, 76); epaperText.print("Kiem tra WEB_APP_URL");
        epaperText.setCursor(132, 100); epaperText.print("URL can ngan hon");
      }
    } else if (!calendarLoaded) {
      epaperText.setCursor(8, 50);
      epaperText.print(WiFi.status() == WL_CONNECTED ? "Dang tai lich..." : "Mat Wi-Fi - giu lich cu");
    } else {
      const int selected = selectedEventArrayIndex();
      if (selected < 0) {
        epaperText.setCursor(8, 56); epaperText.print("Khong co su kien trong ngay");
      } else if (navigation.screen == deskbuddy::Screen::Calendar) {
        epaperText.setCursor(8, 52); epaperText.print(events[selected].start.substring(11, 16)); epaperText.print(" ");
        epaperText.print(events[selected].title.substring(0, 27));
        epaperText.setCursor(8, 82); epaperText.print("Su kien "); epaperText.print(navigation.eventIndex + 1);
        epaperText.setCursor(8, 108); epaperText.print("OK: xem ghi chu");
      } else {
        epaperText.setCursor(8, 39); epaperText.print(events[selected].title.substring(0, 30));
        const String description = events[selected].description;
        const size_t offset = static_cast<size_t>(navigation.notePage) * 84;
        for (int line = 0; line < 3; ++line) {
          const size_t start = offset + line * 28;
          if (start >= description.length()) break;
          epaperText.setCursor(8, 62 + line * 19);
          epaperText.print(description.substring(start, start + 28));
        }
        const size_t totalPages = description.length() ? (description.length() + 83) / 84 : 1;
        epaperText.setCursor(8, 122); epaperText.print("Trang "); epaperText.print(navigation.notePage + 1);
        epaperText.print("/"); epaperText.print(totalPages);
      }
      if (navigation.screen == deskbuddy::Screen::Calendar) {
        epaperText.setCursor(8, 122); epaperText.print("UP/DOWN: su kien  OK: note");
      } else {
        epaperText.setCursor(80, 122); epaperText.print("UP/DOWN: cuon; OK: ve");
      }
    }
  } while (epaper.nextPage());
  epaper.hibernate();
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
        digitalWrite(BUZZER_PIN, HIGH);
        delay(250);
        digitalWrite(BUZZER_PIN, LOW);
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
    showLcdMessage("Calendar loi URL", "Kiem tra API URL");
    return;
  }
  http.addHeader("Authorization", String("Bearer ") + DEVICE_API_TOKEN);
  http.addHeader("Accept", "application/json");
  http.setTimeout(12000);
  const int status = http.GET();
  if (status == HTTP_CODE_OK) {
    DynamicJsonDocument document(16 * 1024);
    const DeserializationError error = deserializeJson(document, http.getStream());
    if (!error && document["device_id"].as<String>() == DEVICE_ID) {
      eventCount = 0;
      JsonArray sourceEvents = document["events"].as<JsonArray>();
      for (JsonObject source : sourceEvents) {
        if (eventCount >= MAX_EVENTS) break;
        events[eventCount].title = source["title"] | "(Khong co tieu de)";
        events[eventCount].start = source["start"] | "";
        events[eventCount].description = source["description"] | "";
        eventCount++;
      }
      calendarLoaded = true;
      drawCalendar();
      setStatusLed(false, true, false);
    } else {
      showLcdMessage("JSON Calendar loi", "Kiem tra API");
    }
  } else if (status == HTTP_CODE_UNAUTHORIZED) {
    showLcdMessage("Token/API loi", "Kiem tra ket noi");
  } else {
    showLcdMessage("Calendar offline", "Se thu lai sau");
  }
  http.end();
}

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  const uint32_t now = millis();
  if (lastWifiAttempt != 0 && now - lastWifiAttempt < WIFI_RETRY_MS) return;
  lastWifiAttempt = now;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void syncRtcFromNtp() {
  if (!rtcReady || WiFi.status() != WL_CONNECTED) return;
  configTzTime("ICT-7", "pool.ntp.org", "time.google.com");
  struct tm localTime;
  if (getLocalTime(&localTime, 8000)) {
    rtc.adjust(DateTime(localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
                        localTime.tm_hour, localTime.tm_min, localTime.tm_sec));
  }
}

void updateLcd() {
  if (!rtcReady) {
    showLcdMessage("RTC chua san sang", "Kiem tra DS3231");
    return;
  }
  const DateTime now = rtc.now();
  char first[17];
  char second[17];
  snprintf(first, sizeof(first), "%02d/%02d/%04d", now.day(), now.month(), now.year());
  if (navigation.screen == deskbuddy::Screen::Pomodoro) {
    uint32_t remaining = navigation.pomodoroRunning && static_cast<int32_t>(pomodoroDeadlineMs - millis()) > 0
      ? (pomodoroDeadlineMs - millis()) / 1000UL : pomodoroRemainingSeconds;
    snprintf(second, sizeof(second), "Pomo %02lu:%02lu", static_cast<unsigned long>(remaining / 60),
             static_cast<unsigned long>(remaining % 60));
  } else if (WiFi.status() == WL_CONNECTED) {
    snprintf(second, sizeof(second), "%02d:%02d:%02d Online", now.hour(), now.minute(), now.second());
  } else {
    snprintf(second, sizeof(second), "%02d:%02d:%02d Offline", now.hour(), now.minute(), now.second());
  }
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 0);
  lcd.print(first);
  lcd.setCursor(0, 1);
  lcd.print("                ");
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
  if (rtcReady && rtc.lostPower()) {
    showLcdMessage("RTC mat gio", "Dang cho Wi-Fi");
  }

  SPI.begin(18, -1, 23, EPD_CS);
  epaper.init(115200);
  epaperText.begin(epaper);
  drawCalendar();

  connectWifi();
  const uint32_t waitStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - waitStart < 12000) {
    delay(100);
  }
  if (WiFi.status() == WL_CONNECTED) syncRtcFromNtp();
  updateLcd();
  fetchCalendar();
  lastCalendarFetch = millis();
}

void enterSleepMode() {
  epaper.hibernate();
  lcd.noBacklight();
  showLcdMessage("Desk Buddy", "Tam ngu");
  lcd.noBacklight();
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_32, 0);
  esp_deep_sleep_start();
}

void loop() {
  connectWifi();
  const uint32_t now = millis();
  if (digitalRead(BUTTON_MODE) == LOW) {
    if (modePressedAt == 0) modePressedAt = now;
    if (now - modePressedAt >= 5000) enterSleepMode();
  } else {
    modePressedAt = 0;
  }
  const int buttonPins[4] = {BUTTON_MODE, BUTTON_UP, BUTTON_DOWN, BUTTON_OK};
  bool buttonPressed[4] = {false, false, false, false};
  for (int i = 0; i < 4; ++i) {
    const bool isHigh = digitalRead(buttonPins[i]) == HIGH;
    buttonPressed[i] = previousButtonsHigh[i] && !isHigh;
    previousButtonsHigh[i] = isHigh;
  }
  if (now - lastButtonAt > 60) {
    bool changed = false;
    if (buttonPressed[0]) { deskbuddy::nextScreen(navigation); changed = true; }
    else if (buttonPressed[1]) { if (navigation.screen == deskbuddy::Screen::Pomodoro) { deskbuddy::scrollUp(navigation); if (!pomodoroHasStarted) pomodoroRemainingSeconds = navigation.pomodoroMinutes * 60UL; } else if (navigation.screen == deskbuddy::Screen::Note) moveNoteCursor(-1); else moveCalendarCursor(-1); changed = true; }
    else if (buttonPressed[2]) { if (navigation.screen == deskbuddy::Screen::Pomodoro) { deskbuddy::scrollDown(navigation); if (!pomodoroHasStarted) pomodoroRemainingSeconds = navigation.pomodoroMinutes * 60UL; } else if (navigation.screen == deskbuddy::Screen::Note) moveNoteCursor(1); else moveCalendarCursor(1); changed = true; }
    else if (buttonPressed[3]) {
      if (navigation.screen == deskbuddy::Screen::Pomodoro) {
        if (navigation.pomodoroRunning) {
          const int32_t left = static_cast<int32_t>(pomodoroDeadlineMs - now);
          pomodoroRemainingSeconds = left > 0 ? static_cast<uint32_t>(left / 1000) : 0;
        } else {
          if (!pomodoroHasStarted) pomodoroRemainingSeconds = static_cast<uint32_t>(navigation.pomodoroMinutes) * 60UL;
          pomodoroDeadlineMs = now + pomodoroRemainingSeconds * 1000UL;
          pomodoroHasStarted = true;
        }
      }
      deskbuddy::confirm(navigation); changed = true;
    }
    if (changed) { lastButtonAt = now; queueNoteStateSave(); drawCalendar(); }
  }

  saveNoteStateIfIdle(now);

  if (now - lastLcdUpdate >= 1000) {
    lastLcdUpdate = now;
    if (navigation.pomodoroRunning && static_cast<int32_t>(pomodoroDeadlineMs - now) <= 0) {
      navigation.pomodoroRunning = false;
      pomodoroRemainingSeconds = 0;
      pomodoroHasStarted = false;
      digitalWrite(BUZZER_PIN, HIGH); delay(300); digitalWrite(BUZZER_PIN, LOW);
      drawCalendar();
    }
    updateLcd();
  }
  if (now - lastCalendarFetch >= CALENDAR_REFRESH_MS) {
    lastCalendarFetch = now;
    if (WiFi.status() == WL_CONNECTED) fetchCalendar();
  }
  checkReminder();
  delay(20);
}
