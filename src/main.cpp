/**
 * WeatherAlert — WiFi + Open-Meteo + OLED
 */

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <math.h>

#include <ArduinoJson.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy include/secrets.h.example to include/secrets.h and set WIFI_SSID / WIFI_PASSWORD"
#endif

#include "config.h"

namespace {

constexpr uint32_t kWifiTimeoutMs = 20000;
constexpr int kWeatherFetchFailed = -1;
constexpr int kScreenWidth = 128;
constexpr int kScreenHeight = 64;

enum class WeatherKind { Sunny, Cloudy, Rainy, Error };

uint32_t lastWeatherFetchMs = 0;
bool displayReady = false;
bool testModeActive = false;
int currentWeatherCode = kWeatherFetchFailed;

Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, -1);

bool isRainy(int weathercode) {
  return (weathercode >= 51 && weathercode <= 67) ||
         (weathercode >= 80 && weathercode <= 82) ||
         (weathercode >= 95 && weathercode <= 99);
}

bool isCloudy(int weathercode) {
  return (weathercode >= 1 && weathercode <= 3) || weathercode == 45 ||
         weathercode == 48;
}

WeatherKind classifyWeather(int weathercode) {
  if (weathercode == kWeatherFetchFailed) {
    return WeatherKind::Error;
  }
  if (isRainy(weathercode)) {
    return WeatherKind::Rainy;
  }
  if (isCloudy(weathercode)) {
    return WeatherKind::Cloudy;
  }
  return WeatherKind::Sunny;
}

void drawHeader() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("WeatherAlert");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
}

void drawSunIcon(int x, int y) {
  const int cx = x + 16;
  const int cy = y + 18;
  display.drawCircle(cx, cy, 9, SSD1306_WHITE);
  for (int i = 0; i < 8; i++) {
    const float angle = i * 3.14159265f / 4.0f;
    const int x1 = cx + static_cast<int>(12 * cosf(angle));
    const int y1 = cy + static_cast<int>(12 * sinf(angle));
    const int x2 = cx + static_cast<int>(15 * cosf(angle));
    const int y2 = cy + static_cast<int>(15 * sinf(angle));
    display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
  }
}

void drawCloudIcon(int x, int y) {
  display.fillCircle(x + 12, y + 22, 7, SSD1306_WHITE);
  display.fillCircle(x + 22, y + 18, 9, SSD1306_WHITE);
  display.fillCircle(x + 32, y + 22, 7, SSD1306_WHITE);
  display.fillRect(x + 8, y + 22, 30, 8, SSD1306_WHITE);
}

void drawRainIcon(int x, int y) {
  drawCloudIcon(x, y);
  display.drawLine(x + 14, y + 32, x + 14, y + 38, SSD1306_WHITE);
  display.drawLine(x + 22, y + 32, x + 22, y + 40, SSD1306_WHITE);
  display.drawLine(x + 30, y + 32, x + 30, y + 38, SSD1306_WHITE);
}

void drawErrorIcon(int x, int y) {
  display.drawCircle(x + 16, y + 18, 12, SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(x + 11, y + 10);
  display.print("!");
}

void drawWeatherText(const char* title, const char* subtitle) {
  display.setTextSize(2);
  display.setCursor(54, 16);
  display.println(title);
  display.setTextSize(1);
  display.setCursor(54, 38);
  display.println(subtitle);
}

void drawFooter(int weathercode) {
  display.setTextSize(1);
  display.setCursor(0, 56);
  if (weathercode == kWeatherFetchFailed) {
    display.print("API unavailable");
  } else {
    display.printf("WMO %d", weathercode);
  }
}

bool initDisplay() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed (0x3C). Check SDA/SCL wiring.");
    return false;
  }

  display.clearDisplay();
  drawHeader();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("Starting...");
  display.display();

  Serial.println("OLED ready");
  return true;
}

void updateDisplay(int weathercode) {
  if (!displayReady) {
    return;
  }

  display.clearDisplay();
  drawHeader();

  switch (classifyWeather(weathercode)) {
    case WeatherKind::Rainy:
      drawRainIcon(0, 12);
      drawWeatherText("Rain", "Take umbrella");
      break;
    case WeatherKind::Cloudy:
      drawCloudIcon(0, 12);
      drawWeatherText("Cloudy", "Check later");
      break;
    case WeatherKind::Sunny:
      drawSunIcon(0, 12);
      drawWeatherText("Sunny", "No umbrella");
      break;
    case WeatherKind::Error:
      drawErrorIcon(0, 12);
      drawWeatherText("Error", "Retry soon");
      break;
  }

  drawFooter(weathercode);
  display.display();
}

void connectWiFi() {
  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - started >= kWifiTimeoutMs) {
      Serial.println("\nWiFi connection timed out. Retrying in 5s...");
      WiFi.disconnect(true);
      delay(5000);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      started = millis();
      continue;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected. IP: ");
  Serial.println(WiFi.localIP());
  Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
}

int fetchTodayWeatherCode() {
  char url[192];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?"
           "latitude=%.4f&longitude=%.4f&daily=weathercode&forecast_days=1&timezone=Asia/Tokyo",
           LATITUDE, LONGITUDE);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader("Accept-Encoding", "identity");

  if (!http.begin(client, url)) {
    Serial.println("HTTP begin failed");
    return kWeatherFetchFailed;
  }

  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("HTTP GET failed: %d\n", status);
    http.end();
    return kWeatherFetchFailed;
  }

  const String payload = http.getString();
  http.end();

  if (payload.isEmpty()) {
    Serial.println("Empty HTTP response");
    return kWeatherFetchFailed;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.printf("JSON parse failed: %s (len=%u)\n", error.c_str(), payload.length());
    Serial.println(payload.substring(0, 120));
    return kWeatherFetchFailed;
  }

  JsonArray codes = doc["daily"]["weathercode"].as<JsonArray>();
  if (codes.isNull() || codes.size() == 0) {
    Serial.println("weathercode missing in response");
    return kWeatherFetchFailed;
  }

  return codes[0].as<int>();
}

void printWeatherResult(int weathercode) {
  if (weathercode == kWeatherFetchFailed) {
    Serial.println("Weather fetch failed");
    return;
  }

  Serial.printf("Location: %.4f, %.4f\n", LATITUDE, LONGITUDE);
  Serial.printf("Today weathercode (WMO): %d\n", weathercode);
  Serial.printf("RAIN: %s\n", isRainy(weathercode) ? "true" : "false");
}

void printSerialHelp() {
  Serial.println();
  Serial.println("--- Serial test commands ---");
  Serial.println("  51        set WMO code (display updates immediately)");
  Serial.println("  code 51   same as above");
  Serial.println("  rain      preset: 51 (rain)");
  Serial.println("  cloudy    preset: 3");
  Serial.println("  clear     preset: 0 (sunny)");
  Serial.println("  live      exit test mode, fetch API");
  Serial.println("  refresh   fetch API now");
  Serial.println("  help      show this list");
  Serial.println("----------------------------");
}

void updateWeather();

void applyWeatherCode(int weathercode, bool testMode) {
  currentWeatherCode = weathercode;
  testModeActive = testMode;
  printWeatherResult(weathercode);
  updateDisplay(weathercode);

  if (testMode) {
    Serial.printf("[TEST] code %d applied. OLED updated. Send 'live' for API mode.\n",
                  weathercode);
  }
}

bool parseWeatherCode(const String& token, int* outCode) {
  if (token.length() == 0) {
    return false;
  }

  for (size_t i = 0; i < token.length(); i++) {
    if (!isDigit(token[i])) {
      return false;
    }
  }

  *outCode = token.toInt();
  return *outCode >= 0 && *outCode <= 99;
}

void handleSerialCommands() {
  if (!Serial.available()) {
    return;
  }

  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) {
    return;
  }

  if (line.equalsIgnoreCase("help") || line == "?") {
    printSerialHelp();
    return;
  }

  if (line.equalsIgnoreCase("live") || line.equalsIgnoreCase("auto")) {
    testModeActive = false;
    Serial.println("Test mode off. Fetching live weather...");
    updateWeather();
    return;
  }

  if (line.equalsIgnoreCase("refresh")) {
    testModeActive = false;
    updateWeather();
    return;
  }

  if (line.equalsIgnoreCase("rain")) {
    applyWeatherCode(51, true);
    return;
  }

  if (line.equalsIgnoreCase("cloudy")) {
    applyWeatherCode(3, true);
    return;
  }

  if (line.equalsIgnoreCase("clear") || line.equalsIgnoreCase("sun")) {
    applyWeatherCode(0, true);
    return;
  }

  String codeToken = line;
  if (line.startsWith("code ")) {
    codeToken = line.substring(5);
    codeToken.trim();
  } else if (line.startsWith("c ")) {
    codeToken = line.substring(2);
    codeToken.trim();
  }

  int weathercode = 0;
  if (parseWeatherCode(codeToken, &weathercode)) {
    applyWeatherCode(weathercode, true);
    return;
  }

  Serial.printf("Unknown: '%s'. Type 'help'.\n", line.c_str());
}

void updateWeather() {
  Serial.println("Fetching weather from Open-Meteo...");
  const int weathercode = fetchTodayWeatherCode();
  currentWeatherCode = weathercode;
  printWeatherResult(weathercode);
  updateDisplay(weathercode);
  lastWeatherFetchMs = millis();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=== WeatherAlert ===");

  displayReady = initDisplay();
  connectWiFi();
  updateWeather();
  printSerialHelp();
}

void loop() {
  handleSerialCommands();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost. Reconnecting...");
    connectWiFi();
  }

  if (!testModeActive && millis() - lastWeatherFetchMs >= WEATHER_REFRESH_MS) {
    updateWeather();
  }

  delay(100);
}
