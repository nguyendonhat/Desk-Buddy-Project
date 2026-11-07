#include <Arduino.h>
#include <ArduinoJson.h>
#include <GxEPD2_BW.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <SPI.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>

#include "config.h"

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

constexpr uint32_t CALENDAR_REFRESH_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t WIFI_RETRY_MS = 10UL * 1000UL;
constexpr size_t MAX_EVENTS = 8;
constexpr long REMINDER_WINDOW_SECONDS = 15L * 60L;

GxEPD2_BW<GxEPD2_290_GDEY029T94, GxEPD2_290_GDEY029T94::HEIGHT> epaper(
  GxEPD2_290_GDEY029T94(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
U8G2_FOR_ADAFRUIT_GFX epaperText;
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 rtc;

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
    epaperText.print("Desk Buddy - Lich");
    epaper.drawLine(6, 23, epaper.width() - 6, 23, GxEPD_BLACK);

    if (WiFi.status() != WL_CONNECTED) {
      epaperText.setCursor(8, 44);
      epaperText.print("Mat Wi-Fi - dang thu lai");
    } else if (!calendarLoaded) {
      epaperText.setCursor(8, 44);
      epaperText.print("Chua tai duoc lich");
    } else if (eventCount == 0) {
      epaperText.setCursor(8, 44);
      epaperText.print("7 ngay toi khong co lich");
    } else {
      const size_t shown = eventCount < 4 ? eventCount : 4;
      for (size_t i = 0; i < shown; i++) {
        const int y = 43 + static_cast<int>(i) * 20;
        String timeLabel = events[i].start.length() >= 16 && events[i].start[10] == 'T'
          ? events[i].start.substring(11, 16)
          : String("Ca ngay");
        String title = events[i].title;
        if (title.length() > 24) title = title.substring(0, 23) + "…";
        epaperText.setCursor(8, y);
        epaperText.print(timeLabel);
        epaperText.print("  ");
        epaperText.print(title);
      }
    }
  } while (epaper.nextPage());
  epaper.hibernate();
}

bool parseEventLocalEpoch(const String& value, long& localEpoch) {
  if (value.length() < 16 || value[10] != 'T') return false; // All-day events have no time.

  const int year = value.substring(0, 4).toInt();
  const int month = value.substring(5, 7).toInt();
  const int day = value.substring(8, 10).toInt();
  const int hour = value.substring(11, 13).toInt();
  const int minute = value.substring(14, 16).toInt();
  const int second = value.length() >= 19 ? value.substring(17, 19).toInt() : 0;
  DateTime wallTime(year, month, day, hour, minute, second);

  long eventOffset = value.endsWith("Z") ? 0 : LOCAL_UTC_OFFSET_SECONDS;
  const int zoneStart = value.lastIndexOf('+') > 10 ? value.lastIndexOf('+') : value.lastIndexOf('-');
  if (zoneStart > 10 && value.length() >= static_cast<size_t>(zoneStart + 6)) {
    const int sign = value[zoneStart] == '-' ? -1 : 1;
    const int offsetHours = value.substring(zoneStart + 1, zoneStart + 3).toInt();
    const int offsetMinutes = value.substring(zoneStart + 4, zoneStart + 6).toInt();
    eventOffset = sign * (offsetHours * 3600L + offsetMinutes * 60L);
  }

  // Convert the event's RFC3339 wall time to the device's configured local offset.
  localEpoch = static_cast<long>(wallTime.unixtime()) - eventOffset + LOCAL_UTC_OFFSET_SECONDS;
  return true;
}

void checkReminder() {
  if (!rtcReady || !calendarLoaded || eventCount == 0) return;
  const long now = static_cast<long>(rtc.now().unixtime());
  reminderActive = false;
  for (size_t i = 0; i < eventCount; i++) {
    long start = 0;
    if (!parseEventLocalEpoch(events[i].start, start)) continue;
    const long secondsUntil = start - now;
    if (secondsUntil >= 0 && secondsUntil <= REMINDER_WINDOW_SECONDS) {
      reminderActive = true;
      if (events[i].start != lastReminderKey) {
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
  if (WiFi.status() == WL_CONNECTED) {
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

void loop() {
  connectWifi();
  const uint32_t now = millis();

  if (now - lastLcdUpdate >= 1000) {
    lastLcdUpdate = now;
    updateLcd();
  }
  if (now - lastCalendarFetch >= CALENDAR_REFRESH_MS) {
    lastCalendarFetch = now;
    if (WiFi.status() == WL_CONNECTED) fetchCalendar();
  }
  checkReminder();
  delay(20);
}
