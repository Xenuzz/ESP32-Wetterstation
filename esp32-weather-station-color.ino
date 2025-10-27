 /**The MIT License (MIT) 
Copyright (c) 2018 by Daniel Eichhorn

Modified 2020 by Zihatec GmbH for AZ-Touch ESP with ESP32

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
See more at https://blog.squix.org
*/


/*****************************
 * Important: see settings.h to configure your settings!!!
 * ***************************/
#include "settings.h"

#include <Arduino.h>
#include <ctype.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <XPT2046_Touchscreen.h>
#include "TouchControllerWS.h"


/***
 * Install the following libraries through Arduino Library Manager
 * - Mini Grafx by Daniel Eichhorn
 * - ESP32 WeatherStation by Zihatec
 * - Json Streaming Parser by Daniel Eichhorn
 * - simpleDSTadjust by neptune2
 ***/

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Astronomy.h>
#include <MiniGrafx.h>
#include <Carousel.h>
#include <ILI9341_SPI.h>


#include "ArialRounded.h"
#include "moonphases.h"
#include "weathericons.h"




#define MINI_BLACK 0
#define MINI_WHITE 1
#define MINI_YELLOW 2
#define MINI_BLUE 3

#define MAX_FORECASTS 12

// defines the colors usable in the paletted 16 color frame buffer
uint16_t palette[] = {ILI9341_BLACK, // 0
                      ILI9341_WHITE, // 1
                      ILI9341_YELLOW, // 2
                      0x7E3C
                      }; //3

int SCREEN_WIDTH = 240;
int SCREEN_HEIGHT = 320;
// Limited to 4 colors due to memory constraints
int BITS_PER_PIXEL = 2; // 2^2 =  4 colors


//ADC_MODE(ADC_VCC);


ILI9341_SPI tft = ILI9341_SPI(TFT_CS, TFT_DC, TFT_RST);
MiniGrafx gfx = MiniGrafx(&tft, BITS_PER_PIXEL, palette);
Carousel carousel(&gfx, 0, 0, 240, 100);


XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
TouchControllerWS touchController(&ts);

void calibrationCallback(int16_t x, int16_t y);
CalibrationCallback calibration = &calibrationCallback;
  

struct BrightSkyCurrentWeather {
  String icon;
  String condition;
  String description;
  float temperature = NAN;
  float windSpeed = NAN;
  float windDirection = NAN;
  float humidity = NAN;
  float pressure = NAN;
  float precipitation = NAN;
  float visibility = NAN;
  float cloudCover = NAN;
  time_t observationTime = 0;
  time_t sunrise = 0;
  time_t sunset = 0;
};

struct BrightSkyForecastData {
  time_t observationTime = 0;
  float temperature = NAN;
  float humidity = NAN;
  float precipitation = NAN;
  float pressure = NAN;
  float windSpeed = NAN;
  float windDirection = NAN;
  String icon;
  String summary;
};

struct WeatherAlert {
  String headline;
  String event;
  String severity;
  String description;
  String instruction;
  time_t onset = 0;
  time_t expires = 0;
};

BrightSkyCurrentWeather currentWeather;
BrightSkyForecastData forecasts[MAX_FORECASTS];
uint8_t forecastCount = 0;
WeatherAlert weatherAlerts[MAX_WEATHER_ALERTS];
uint8_t weatherAlertCount = 0;
bool hasWeatherAlerts = false;
simpleDSTadjust dstAdjusted(StartRule, EndRule);
Astronomy::MoonData moonData;

void updateData();
void drawProgress(uint8_t percentage, String text);
void drawTime();
void drawWifiQuality();
void drawCurrentWeather();
void drawForecast();
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex);
void drawAstronomy();
void drawCurrentWeatherDetail();
void drawLabelValue(uint8_t line, String label, String value);
void drawForecastTable(uint8_t start);
void drawAbout();
void drawSeparator(uint16_t y);
String getTime(time_t *timestamp);
const char* getMeteoconIconFromProgmem(String iconText);
const char* getMiniMeteoconIconFromProgmem(String iconText);
void drawForecast1(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y);
void drawForecast2(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y);
void drawForecast3(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y);
FrameCallback frames[] = { drawForecast1, drawForecast2, drawForecast3 };
int frameCount = 3;
bool fetchCurrentWeather();
bool fetchForecasts();
bool fetchWeatherAlerts();
bool fetchSunTimes();
time_t parseTimestamp(const String &isoString);
String formatAlertTime(time_t timestamp);
String formatNullableFloat(float value, uint8_t decimals = 1, const String &suffix = "");
String translateCondition(const String &condition);
String capitalizeSentence(const String &text);
String severityLabel(const String &severity);
void drawWeatherAlertPopup();

// how many different screens do we have?
int screenCount = 5;
long lastDownloadUpdate = millis();

String moonAgeImage = "";
uint8_t moonAge = 0;
uint16_t screen = 0;
long timerPress;
bool canBtnPress;
time_t dstOffset = 0;

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  //Manual Wifi
  Serial.print("Connecting to WiFi ");
  Serial.print(WIFI_SSID);
  Serial.print("/");
  Serial.println(WIFI_PASS);
  WiFi.mode(WIFI_STA);
  //WiFi.hostname(WIFI_HOSTNAME);
  WiFi.begin(WIFI_SSID,WIFI_PASS);
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (i>80) i=0;
    drawProgress(i,"Connecting to WiFi '" + String(WIFI_SSID) + "'");
    i+=10;
    Serial.print(".");
  }
  drawProgress(100,"Connected to WiFi '" + String(WIFI_SSID) + "'");
  Serial.print("Connected...");
}

void setup() {
  Serial.begin(115200);
  
  // switch TFT backlight on
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, LOW);    // HIGH to Turn on;
  
  gfx.init();
  gfx.fillBuffer(MINI_BLACK);
  gfx.commit();

  connectWifi();

  Serial.println("Initializing touch screen...");
  ts.begin();

  carousel.setFrames(frames, frameCount);
  carousel.disableAllIndicators();

  // update the weather information
  updateData();
  timerPress = millis();
  canBtnPress = true;

   
  // 
}

long lastDrew = 0;
bool btnClick;
uint8_t MAX_TOUCHPOINTS = 10;
TS_Point points[10];
uint8_t currentTouchPoint = 0;
void loop() {
  gfx.fillBuffer(MINI_BLACK);
  if (touchController.isTouched(10)) {
    TS_Point p = touchController.getPoint();
    Serial.print("Touch event");
    screen = (screen + 1) % screenCount;
    
  }

  
  if (screen == 0) {
    drawTime();
    drawWifiQuality();
    int remainingTimeBudget = carousel.update();
    if (remainingTimeBudget > 0) {
      // You can do some work here
      // Don't do stuff if you are below your
      // time budget.
      delay(remainingTimeBudget);
    }
    drawCurrentWeather();
    drawAstronomy();
  } else if (screen == 1) {
    drawCurrentWeatherDetail();
  } else if (screen == 2) {
    drawForecastTable(0);
  } else if (screen == 3) {
    drawForecastTable(4);
  } else if (screen == 4) {
    drawAbout();
  }
  if (hasWeatherAlerts) {
    drawWeatherAlertPopup();
  }
  gfx.commit();

  // Check if we should update weather information
  if (millis() - lastDownloadUpdate > 1000 * UPDATE_INTERVAL_SECS) {
      updateData();
      lastDownloadUpdate = millis();
  }

  /*
  if (SLEEP_INTERVAL_SECS && millis() - timerPress >= SLEEP_INTERVAL_SECS * 1000){ // after 2 minutes go to sleep
    drawProgress(25,"Going to Sleep!");
    delay(1000);
    drawProgress(50,"Going to Sleep!");
    delay(1000);
    drawProgress(75,"Going to Sleep!");
    delay(1000);    
    drawProgress(100,"Going to Sleep!");
    // go to deepsleep for xx minutes or 0 = permanently
    ESP.deepSleep(0,  WAKE_RF_DEFAULT);                       // 0 delay = permanently to sleep
    tone(16, 1000, 100);
  } 
  */ 
}

// Update the internet based information and update screen
void updateData() {

  gfx.fillBuffer(MINI_BLACK);
  gfx.setFont(ArialRoundedMTBold_14);

  drawProgress(10, "Updating time...");
  configTime(UTC_OFFSET * 3600, 0, NTP_SERVERS);
  while(!time(nullptr)) {
    Serial.print("#");
    delay(100);
  }
  // calculate for time calculation how much the dst class adds.
  dstOffset = UTC_OFFSET * 3600 + dstAdjusted.time(nullptr) - time(nullptr);
  Serial.printf("Time difference for DST: %d", dstOffset);

  drawProgress(40, "Updating conditions...");
  if (!fetchCurrentWeather()) {
    Serial.println("Failed to load current weather from Bright Sky");
  }

  drawProgress(60, "Updating forecasts...");
  if (!fetchForecasts()) {
    Serial.println("Failed to load forecast data from Bright Sky");
  }

  drawProgress(70, "Loading sun times...");
  if (!fetchSunTimes()) {
    Serial.println("Failed to load sunrise and sunset information");
  }

  drawProgress(80, "Checking alerts...");
  if (!fetchWeatherAlerts()) {
    Serial.println("Failed to load weather alerts");
  }
  hasWeatherAlerts = weatherAlertCount > 0;

  drawProgress(90, "Updating astronomy...");
  Astronomy *astronomy = new Astronomy();
  moonData = astronomy->calculateMoonData(time(nullptr));
  float lunarMonth = 29.53;
  moonAge = moonData.phase <= 4 ? lunarMonth * moonData.illumination / 2 : lunarMonth - moonData.illumination * lunarMonth / 2;
  moonAgeImage = String((char) (65 + ((uint8_t) ((26 * moonAge / 30) % 26))));
  delete astronomy;
  astronomy = nullptr;
  delay(1000);
}

bool fetchCurrentWeather() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String timezoneParam = String(BRIGHT_SKY_TIMEZONE);
  timezoneParam.replace("/", "%2F");
  String url = String("https://api.brightsky.dev/current_weather?lat=") + String(BRIGHT_SKY_LATITUDE, 4) +
               "&lon=" + String(BRIGHT_SKY_LONGITUDE, 4) + "&tz=" + timezoneParam;

  if (!http.begin(client, url)) {
    Serial.println("Unable to initialize HTTP connection for current weather");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Bright Sky current weather request failed: %d\n", httpCode);
    http.end();
    return false;
  }

  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, http.getStream());
  http.end();
  if (error) {
    Serial.printf("Failed to parse current weather JSON: %s\n", error.c_str());
    return false;
  }

  JsonObject weather = doc["weather"];
  if (weather.isNull()) {
    return false;
  }

  currentWeather.temperature = weather["temperature"].isNull() ? NAN : weather["temperature"].as<float>();
  currentWeather.windSpeed = weather["wind_speed"].isNull() ? NAN : weather["wind_speed"].as<float>();
  currentWeather.windDirection = weather["wind_direction"].isNull() ? NAN : weather["wind_direction"].as<float>();
  currentWeather.humidity = weather["relative_humidity"].isNull() ? NAN : weather["relative_humidity"].as<float>();
  currentWeather.pressure = weather["pressure_msl"].isNull() ? NAN : weather["pressure_msl"].as<float>();
  currentWeather.precipitation = weather["precipitation"].isNull() ? NAN : weather["precipitation"].as<float>();
  currentWeather.visibility = weather["visibility"].isNull() ? NAN : weather["visibility"].as<float>();
  currentWeather.cloudCover = weather["cloud_cover"].isNull() ? NAN : weather["cloud_cover"].as<float>();
  currentWeather.condition = weather["condition"].as<String>();
  currentWeather.icon = weather["icon"].as<String>();
  currentWeather.observationTime = parseTimestamp(weather["timestamp"].as<String>());

  String translatedCondition = translateCondition(currentWeather.condition);
  currentWeather.description = translatedCondition.length() ? translatedCondition : currentWeather.condition;
  currentWeather.description = capitalizeSentence(currentWeather.description);
  if (currentWeather.description.length() == 0) {
    currentWeather.description = "--";
  }

  return true;
}

bool fetchForecasts() {
  forecastCount = 0;

  time_t nowTs = time(nullptr);
  struct tm startTm = *gmtime(&nowTs);
  char startDate[11];
  strftime(startDate, sizeof(startDate), "%Y-%m-%d", &startTm);

  time_t endTs = nowTs + 48 * 3600;
  struct tm endTm = *gmtime(&endTs);
  char endDate[11];
  strftime(endDate, sizeof(endDate), "%Y-%m-%d", &endTm);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String timezoneParam = String(BRIGHT_SKY_TIMEZONE);
  timezoneParam.replace("/", "%2F");
  String url = String("https://api.brightsky.dev/weather?lat=") + String(BRIGHT_SKY_LATITUDE, 4) +
               "&lon=" + String(BRIGHT_SKY_LONGITUDE, 4) +
               "&date=" + startDate + "&last_date=" + endDate +
               "&tz=" + timezoneParam + "&max_entries=" + String(MAX_FORECASTS);

  if (!http.begin(client, url)) {
    Serial.println("Unable to initialize HTTP connection for forecast");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Bright Sky forecast request failed: %d\n", httpCode);
    http.end();
    return false;
  }

  DynamicJsonDocument doc(24576);
  DeserializationError error = deserializeJson(doc, http.getStream());
  http.end();
  if (error) {
    Serial.printf("Failed to parse forecast JSON: %s\n", error.c_str());
    return false;
  }

  JsonArray weatherArray = doc["weather"].as<JsonArray>();
  if (weatherArray.isNull()) {
    return false;
  }

  for (JsonObject entry : weatherArray) {
    if (forecastCount >= MAX_FORECASTS) {
      break;
    }
    BrightSkyForecastData &forecast = forecasts[forecastCount];
    forecast.observationTime = parseTimestamp(entry["timestamp"].as<String>());
    forecast.temperature = entry["temperature"].isNull() ? NAN : entry["temperature"].as<float>();
    forecast.humidity = entry["relative_humidity"].isNull() ? NAN : entry["relative_humidity"].as<float>();
    forecast.precipitation = entry["precipitation"].isNull() ? NAN : entry["precipitation"].as<float>();
    forecast.pressure = entry["pressure_msl"].isNull() ? NAN : entry["pressure_msl"].as<float>();
    forecast.windSpeed = entry["wind_speed"].isNull() ? NAN : entry["wind_speed"].as<float>();
    forecast.windDirection = entry["wind_direction"].isNull() ? NAN : entry["wind_direction"].as<float>();
    forecast.icon = entry["icon"].as<String>();
    String summary = translateCondition(entry["condition"].as<String>());
    forecast.summary = capitalizeSentence(summary.length() ? summary : entry["condition"].as<String>());
    forecastCount++;
  }

  return forecastCount > 0;
}

bool fetchSunTimes() {
  time_t nowTs = time(nullptr);
  struct tm localTm = *localtime(&nowTs);
  char date[11];
  strftime(date, sizeof(date), "%Y-%m-%d", &localTm);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String timezoneParam = String(BRIGHT_SKY_TIMEZONE);
  timezoneParam.replace("/", "%2F");
  String url = String("https://api.brightsky.dev/sunrise-sunset?lat=") + String(BRIGHT_SKY_LATITUDE, 4) +
               "&lon=" + String(BRIGHT_SKY_LONGITUDE, 4) +
               "&date=" + date + "&tz=" + timezoneParam;

  if (!http.begin(client, url)) {
    Serial.println("Unable to initialize HTTP connection for sun times");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Bright Sky sun times request failed: %d\n", httpCode);
    http.end();
    return false;
  }

  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, http.getStream());
  http.end();
  if (error) {
    Serial.printf("Failed to parse sun times JSON: %s\n", error.c_str());
    return false;
  }

  currentWeather.sunrise = parseTimestamp(doc["sunrise"].as<String>());
  currentWeather.sunset = parseTimestamp(doc["sunset"].as<String>());
  return currentWeather.sunrise != 0 && currentWeather.sunset != 0;
}

bool fetchWeatherAlerts() {
  weatherAlertCount = 0;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = String("https://api.brightsky.dev/alerts?lat=") + String(BRIGHT_SKY_LATITUDE, 4) +
               "&lon=" + String(BRIGHT_SKY_LONGITUDE, 4) + "&status=actual";

  if (!http.begin(client, url)) {
    Serial.println("Unable to initialize HTTP connection for alerts");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Bright Sky alert request failed: %d\n", httpCode);
    http.end();
    return false;
  }

  DynamicJsonDocument doc(16384);
  DeserializationError error = deserializeJson(doc, http.getStream());
  http.end();
  if (error) {
    Serial.printf("Failed to parse alert JSON: %s\n", error.c_str());
    return false;
  }

  JsonArray alerts = doc["alerts"].as<JsonArray>();
  if (alerts.isNull()) {
    return false;
  }

  for (JsonObject alert : alerts) {
    if (weatherAlertCount >= MAX_WEATHER_ALERTS) {
      break;
    }
    WeatherAlert &target = weatherAlerts[weatherAlertCount];
    target.headline = alert["headline"].as<String>();
    target.event = alert["event"].as<String>();
    target.severity = alert["severity"].as<String>();
    target.description = alert["description"].as<String>();
    target.instruction = alert["instruction"].as<String>();
    target.onset = parseTimestamp(alert["onset"].as<String>());
    target.expires = parseTimestamp(alert["expires"].as<String>());
    weatherAlertCount++;
  }

  return true;
}

time_t parseTimestamp(const String &isoString) {
  if (!isoString.length()) {
    return 0;
  }

  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
  char tzSign = '+';
  int tzHour = 0, tzMinute = 0;

  if (isoString.endsWith("Z")) {
    sscanf(isoString.c_str(), "%d-%d-%dT%d:%d:%dZ", &year, &month, &day, &hour, &minute, &second);
  } else {
    sscanf(isoString.c_str(), "%d-%d-%dT%d:%d:%d%c%d:%d", &year, &month, &day, &hour, &minute, &second, &tzSign, &tzHour, &tzMinute);
  }

  if (year == 0) {
    return 0;
  }

  if (tzSign != '+' && tzSign != '-') {
    tzSign = '+';
  }

  if (tzHour < 0 || tzHour > 14) {
    tzHour = 0;
  }
  if (tzMinute < 0 || tzMinute > 59) {
    tzMinute = 0;
  }

  if (isoString.endsWith("Z")) {
    tzHour = 0;
    tzMinute = 0;
    tzSign = '+';
  }

  if (month <= 2) {
    month += 12;
    year -= 1;
  }

  long era = year / 400;
  long yoe = year - era * 400;
  long doy = (153 * (month - 3) + 2) / 5 + day - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  long days = era * 146097 + doe - 719468; // Days since Unix epoch

  time_t utc = days * 86400 + hour * 3600 + minute * 60 + second;
  int offsetSeconds = tzHour * 3600 + tzMinute * 60;
  if (tzSign == '+') {
    utc -= offsetSeconds;
  } else {
    utc += offsetSeconds;
  }

  return utc;
}

String formatAlertTime(time_t timestamp) {
  if (timestamp == 0) {
    return "-";
  }
  struct tm *timeInfo = localtime(&timestamp);
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d.%m. %H:%M", timeInfo);
  return String(buffer);
}

String formatNullableFloat(float value, uint8_t decimals, const String &suffix) {
  if (isnan(value)) {
    return "-";
  }
  return String(value, decimals) + suffix;
}

String translateCondition(const String &condition) {
  String lower = condition;
  lower.toLowerCase();
  if (lower == "clear" || lower == "clear-day" || lower == "clear-night") return "klar";
  if (lower == "partly-cloudy" || lower == "partly-cloudy-day" || lower == "partly-cloudy-night") return "teilweise bewölkt";
  if (lower == "mostly-cloudy" || lower == "cloudy" || lower == "overcast") return "bewölkt";
  if (lower == "fog" || lower == "mist") return "Nebel";
  if (lower == "rain" || lower == "rain-showers" || lower == "rain-showers-day" || lower == "rain-showers-night" || lower == "light-rain" || lower == "drizzle") return "Regen";
  if (lower == "sleet" || lower == "hail") return "Schneeregen";
  if (lower == "snow" || lower == "snow-showers" || lower == "snow-showers-day" || lower == "snow-showers-night") return "Schnee";
  if (lower == "thunderstorm" || lower == "thunderstorms") return "Gewitter";
  if (lower == "wind" || lower == "windy") return "Windig";
  return lower;
}

String capitalizeSentence(const String &text) {
  if (!text.length()) {
    return text;
  }
  String result = text;
  result.toLowerCase();
  result.setCharAt(0, toupper(result.charAt(0)));
  return result;
}

String severityLabel(const String &severity) {
  String lower = severity;
  lower.toLowerCase();
  if (lower == "minor") return "Gering";
  if (lower == "moderate") return "Mäßig";
  if (lower == "severe") return "Schwer";
  if (lower == "extreme") return "Extrem";
  return severity;
}
// Progress bar helper
void drawProgress(uint8_t percentage, String text) {
  gfx.fillBuffer(MINI_BLACK);
  gfx.drawPalettedBitmapFromPgm(20, 5, ThingPulseLogo);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(120, 90, "https://thingpulse.com");
  gfx.setColor(MINI_YELLOW);

  gfx.drawString(120, 146, text);
  gfx.setColor(MINI_WHITE);
  gfx.drawRect(10, 168, 240 - 20, 15);
  gfx.setColor(MINI_BLUE);
  gfx.fillRect(12, 170, 216 * percentage / 100, 11);

  gfx.commit();
}

// draws the clock
void drawTime() {

  char time_str[11];
  char *dstAbbrev;
  time_t now = dstAdjusted.time(&dstAbbrev);
  struct tm * timeinfo = localtime (&now);

  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setColor(MINI_WHITE);
  String date = ctime(&now);
  date = date.substring(0,11) + String(1900 + timeinfo->tm_year);
  gfx.drawString(120, 6, date);

  gfx.setFont(ArialRoundedMTBold_36);

  if (IS_STYLE_12HR) {
    int hour = (timeinfo->tm_hour+11)%12+1;  // take care of noon and midnight
    sprintf(time_str, "%2d:%02d:%02d\n",hour, timeinfo->tm_min, timeinfo->tm_sec);
    gfx.drawString(120, 20, time_str);
  } else {
    sprintf(time_str, "%02d:%02d:%02d\n",timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    gfx.drawString(120, 20, time_str);
  }

  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setColor(MINI_BLUE);
  if (IS_STYLE_12HR) {
    sprintf(time_str, "%s\n%s", dstAbbrev, timeinfo->tm_hour>=12?"PM":"AM");
    gfx.drawString(195, 27, time_str);
  } else {
    sprintf(time_str, "%s", dstAbbrev);
    gfx.drawString(195, 27, time_str);  // Known bug: Cuts off 4th character of timezone abbreviation
  }
}

// draws current weather information
void drawCurrentWeather() {
  gfx.setTransparentColor(MINI_BLACK);
  gfx.drawPalettedBitmapFromPgm(0, 55, getMeteoconIconFromProgmem(currentWeather.icon));
  // Weather Text

  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setColor(MINI_BLUE);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  gfx.drawString(220, 65, DISPLAYED_CITY_NAME);

  gfx.setFont(ArialRoundedMTBold_36);
  gfx.setColor(MINI_WHITE);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);

  String temperature = isnan(currentWeather.temperature)
                        ? String("--")
                        : String(currentWeather.temperature, 1) + (IS_METRIC ? "°C" : "°F");
  gfx.drawString(220, 78, temperature);

  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setColor(MINI_YELLOW);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  gfx.drawString(220, 118, currentWeather.description);

}

void drawForecast1(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y) {
  drawForecastDetail(x + 10, y + 165, 0);
  drawForecastDetail(x + 95, y + 165, 1);
  drawForecastDetail(x + 180, y + 165, 2);
}

void drawForecast2(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y) {
  drawForecastDetail(x + 10, y + 165, 3);
  drawForecastDetail(x + 95, y + 165, 4);
  drawForecastDetail(x + 180, y + 165, 5);
}

void drawForecast3(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y) {
  drawForecastDetail(x + 10, y + 165, 6);
  drawForecastDetail(x + 95, y + 165, 7);
  drawForecastDetail(x + 180, y + 165, 8);
}

// helper for the forecast columns
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex) {
  if (dayIndex >= forecastCount) {
    return;
  }
  gfx.setColor(MINI_YELLOW);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  time_t time = forecasts[dayIndex].observationTime + dstOffset;
  struct tm * timeinfo = localtime (&time);
  gfx.drawString(x + 25, y - 15, WDAY_NAMES[timeinfo->tm_wday] + " " + String(timeinfo->tm_hour) + ":00");

  gfx.setColor(MINI_WHITE);
  String tempText = isnan(forecasts[dayIndex].temperature) ? String("--") :
                    String(forecasts[dayIndex].temperature, 1) + (IS_METRIC ? "°C" : "°F");
  gfx.drawString(x + 25, y, tempText);

  gfx.drawPalettedBitmapFromPgm(x, y + 15, getMiniMeteoconIconFromProgmem(forecasts[dayIndex].icon));
  gfx.setColor(MINI_BLUE);
  String rainText = isnan(forecasts[dayIndex].precipitation) ? String("--") :
                    String(forecasts[dayIndex].precipitation, 1) + (IS_METRIC ? "mm" : "in");
  gfx.drawString(x + 25, y + 60, rainText);
}

// draw moonphase and sunrise/set and moonrise/set
void drawAstronomy() {

  gfx.setFont(MoonPhases_Regular_36);
  gfx.setColor(MINI_WHITE);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(120, 275, moonAgeImage);

  gfx.setColor(MINI_WHITE);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setColor(MINI_YELLOW);
  gfx.drawString(120, 250, MOON_PHASES[moonData.phase]);
  
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setColor(MINI_YELLOW);
  gfx.drawString(5, 250, "Sonne");
  gfx.setColor(MINI_WHITE);
  gfx.drawString(5, 276, "Rise:");
  if (currentWeather.sunrise != 0) {
    time_t time = currentWeather.sunrise + dstOffset;
    gfx.drawString(45, 276, getTime(&time));
  } else {
    gfx.drawString(45, 276, "--:--");
  }
  gfx.drawString(5, 291, "Set:");
  if (currentWeather.sunset != 0) {
    time_t time = currentWeather.sunset + dstOffset;
    gfx.drawString(45, 291, getTime(&time));
  } else {
    gfx.drawString(45, 291, "--:--");
  }

  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  gfx.setColor(MINI_YELLOW);
  gfx.drawString(235, 250, "Mond");
  gfx.setColor(MINI_WHITE);
  gfx.drawString(235, 276, String(moonAge) + "d");
  gfx.drawString(235, 291, String(moonData.illumination * 100, 0) + "%");
  gfx.drawString(200, 276, "Age:");
  gfx.drawString(200, 291, "Illum:");

}

void drawCurrentWeatherDetail() {
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(120, 2, "Current Conditions");

  //gfx.setTransparentColor(MINI_BLACK);
  //gfx.drawPalettedBitmapFromPgm(0, 20, getMeteoconIconFromProgmem(conditions.weatherIcon));

  String degreeSign = "°F";
  if (IS_METRIC) {
    degreeSign = "°C";
  }
  // String weatherIcon;
  // String weatherText;
  drawLabelValue(0, "Temperature:", isnan(currentWeather.temperature) ? "-" : String(currentWeather.temperature, 1) + degreeSign);
  drawLabelValue(1, "Wind Speed:", formatNullableFloat(currentWeather.windSpeed, 1, IS_METRIC ? "m/s" : "mph"));
  drawLabelValue(2, "Wind Dir:", isnan(currentWeather.windDirection) ? "-" : String(currentWeather.windDirection, 0) + "°");
  drawLabelValue(3, "Humidity:", formatNullableFloat(currentWeather.humidity, 0, "%"));
  drawLabelValue(4, "Pressure:", formatNullableFloat(currentWeather.pressure, 0, "hPa"));
  drawLabelValue(5, "Clouds:", formatNullableFloat(currentWeather.cloudCover, 0, "%"));
  drawLabelValue(6, "Precip.:", formatNullableFloat(currentWeather.precipitation, 1, IS_METRIC ? "mm" : "in"));


  /*gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setColor(MINI_YELLOW);
  gfx.drawString(15, 185, "Description: ");
  gfx.setColor(MINI_WHITE);
  gfx.drawStringMaxWidth(15, 200, 240 - 2 * 15, forecasts[0].forecastText);*/
}

void drawLabelValue(uint8_t line, String label, String value) {
  const uint8_t labelX = 15;
  const uint8_t valueX = 150;
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setColor(MINI_YELLOW);
  gfx.drawString(labelX, 30 + line * 15, label);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(valueX, 30 + line * 15, value);
}

// converts the dBm to a range between 0 and 100%
int8_t getWifiQuality() {
  int32_t dbm = WiFi.RSSI();
  if(dbm <= -100) {
      return 0;
  } else if(dbm >= -50) {
      return 100;
  } else {
      return 2 * (dbm + 100);
  }
}

void drawWifiQuality() {
  int8_t quality = getWifiQuality();
  gfx.setColor(MINI_WHITE);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);  
  gfx.drawString(228, 9, String(quality) + "%");
  for (int8_t i = 0; i < 4; i++) {
    for (int8_t j = 0; j < 2 * (i + 1); j++) {
      if (quality > i * 25 || j == 0) {
        gfx.setPixel(230 + 2 * i, 18 - j);
      }
    }
  }
}

void drawForecastTable(uint8_t start) {
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(120, 2, "Forecasts");
  uint16_t y = 0;

  String degreeSign = "°F";
  if (IS_METRIC) {
    degreeSign = "°C";
  }
  for (uint8_t i = start; i < start + 4 && i < forecastCount; i++) {
    gfx.setTextAlignment(TEXT_ALIGN_LEFT);
    y = 45 + (i - start) * 75;
    if (y > 320) {
      break;
    }
    gfx.setColor(MINI_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    time_t time = forecasts[i].observationTime + dstOffset;
    struct tm * timeinfo = localtime (&time);
    gfx.drawString(120, y - 15, WDAY_NAMES[timeinfo->tm_wday] + " " + String(timeinfo->tm_hour) + ":00");


    gfx.drawPalettedBitmapFromPgm(0, 15 + y, getMiniMeteoconIconFromProgmem(forecasts[i].icon));
    gfx.setTextAlignment(TEXT_ALIGN_LEFT);
    gfx.setColor(MINI_YELLOW);
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.drawString(10, y, forecasts[i].summary);
    gfx.setTextAlignment(TEXT_ALIGN_LEFT);
    
    gfx.setColor(MINI_BLUE);
    gfx.drawString(50, y, "T:");
    gfx.setColor(MINI_WHITE);
    String tempText = isnan(forecasts[i].temperature) ? String("--") : String(forecasts[i].temperature, 0) + degreeSign;
    gfx.drawString(70, y, tempText);
    
    gfx.setColor(MINI_BLUE);
    gfx.drawString(50, y + 15, "H:");
    gfx.setColor(MINI_WHITE);
    String humidityText = isnan(forecasts[i].humidity) ? String("-") : String(forecasts[i].humidity, 0) + "%";
    gfx.drawString(70, y + 15, humidityText);

    gfx.setColor(MINI_BLUE);
    gfx.drawString(50, y + 30, "P: ");
    gfx.setColor(MINI_WHITE);
    String precipText = isnan(forecasts[i].precipitation) ? String("--") : String(forecasts[i].precipitation, 2) + (IS_METRIC ? "mm" : "in");
    gfx.drawString(70, y + 30, precipText);

    gfx.setColor(MINI_BLUE);
    gfx.drawString(130, y, "Pr:");
    gfx.setColor(MINI_WHITE);
    String pressureText = isnan(forecasts[i].pressure) ? String("--") : String(forecasts[i].pressure, 0) + "hPa";
    gfx.drawString(170, y, pressureText);
    
    gfx.setColor(MINI_BLUE);
    gfx.drawString(130, y + 15, "WSp:");
    gfx.setColor(MINI_WHITE);
    String windSpeedText = isnan(forecasts[i].windSpeed) ? String("--") : String(forecasts[i].windSpeed, 0) + (IS_METRIC ? "m/s" : "mph");
    gfx.drawString(170, y + 15, windSpeedText);

    gfx.setColor(MINI_BLUE);
    gfx.drawString(130, y + 30, "WDi: ");
    gfx.setColor(MINI_WHITE);
    String windDirText = isnan(forecasts[i].windDirection) ? String("-") : String(forecasts[i].windDirection, 0) + "°";
    gfx.drawString(170, y + 30, windDirText);

  }
}

void drawWeatherAlertPopup() {
  if (!hasWeatherAlerts || weatherAlertCount == 0) {
    return;
  }

  const WeatherAlert &alert = weatherAlerts[0];
  const uint16_t popupX = 10;
  const uint16_t popupY = 50;
  const uint16_t popupWidth = 220;
  const uint16_t popupHeight = 200;

  gfx.setColor(MINI_BLACK);
  gfx.fillRect(popupX, popupY, popupWidth, popupHeight);
  gfx.setColor(MINI_WHITE);
  gfx.drawRect(popupX, popupY, popupWidth, popupHeight);

  bool blink = (millis() / 400) % 2 == 0;
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setColor(blink ? MINI_YELLOW : MINI_WHITE);
  gfx.drawString(popupX + popupWidth / 2, popupY + 16, "Wetter Alarm");

  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setColor(MINI_BLUE);
  String severityText = alert.severity.length() ? severityLabel(alert.severity) : "";
  gfx.drawString(popupX + 10, popupY + 32, severityText);

  gfx.setColor(MINI_WHITE);
  String headline = alert.event.length() ? alert.event : alert.headline;
  gfx.drawStringMaxWidth(popupX + 10, popupY + 48, popupWidth - 20, headline);

  gfx.setFont(ArialMT_Plain_10);
  gfx.setColor(MINI_YELLOW);
  gfx.drawString(popupX + 10, popupY + 70, "Gültig:");
  gfx.setColor(MINI_WHITE);
  gfx.drawStringMaxWidth(popupX + 55, popupY + 70, popupWidth - 65,
                         formatAlertTime(alert.onset) + " - " + formatAlertTime(alert.expires));

  if (alert.description.length()) {
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(popupX + 10, popupY + 86, "Info:");
    gfx.setColor(MINI_WHITE);
    gfx.drawStringMaxWidth(popupX + 10, popupY + 100, popupWidth - 20, alert.description);
  }

  if (alert.instruction.length()) {
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(popupX + 10, popupY + 138, "Hinweis:");
    gfx.setColor(MINI_WHITE);
    gfx.drawStringMaxWidth(popupX + 10, popupY + 152, popupWidth - 20, alert.instruction);
  }
}

void drawAbout() {
  gfx.fillBuffer(MINI_BLACK);
  gfx.drawPalettedBitmapFromPgm(20, 5, ThingPulseLogo);
  
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(120, 90, "https://thingpulse.com");

  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  /*
  drawLabelValue(7, "Heap Mem:", String(ESP.getFreeHeap() / 1024)+"kb");
  // drawLabelValue(8, "Flash Mem:", String(ESP.getFlashChipRealSize() / 1024 / 1024) + "MB");
  drawLabelValue(9, "WiFi Strength:", String(WiFi.RSSI()) + "dB");
  // drawLabelValue(10, "Chip ID:", String(ESP.getChipId()));
  drawLabelValue(11, "VCC: ", String(ESP.getVcc() / 1024.0) +"V");
  drawLabelValue(12, "CPU Freq.: ", String(ESP.getCpuFreqMHz()) + "MHz");
 
  char time_str[15];
  const uint32_t millis_in_day = 1000 * 60 * 60 * 24;
  const uint32_t millis_in_hour = 1000 * 60 * 60;
  const uint32_t millis_in_minute = 1000 * 60;
  uint8_t days = millis() / (millis_in_day);
  uint8_t hours = (millis() - (days * millis_in_day)) / millis_in_hour;
  uint8_t minutes = (millis() - (days * millis_in_day) - (hours * millis_in_hour)) / millis_in_minute;
  sprintf(time_str, "%2dd%2dh%2dm", days, hours, minutes);
  drawLabelValue(13, "Uptime: ", time_str);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setColor(MINI_YELLOW);
  gfx.drawString(15, 250, "Last Reset: ");
  gfx.setColor(MINI_WHITE);
  gfx.drawStringMaxWidth(15, 265, 240 - 2 * 15, ESP.getResetInfo());
  */
}

void calibrationCallback(int16_t x, int16_t y) {
  gfx.setColor(1);
  gfx.fillCircle(x, y, 10);
}

String getTime(time_t *timestamp) {
  struct tm *timeInfo = gmtime(timestamp);
  
  char buf[6];
  sprintf(buf, "%02d:%02d", timeInfo->tm_hour, timeInfo->tm_min);
  return String(buf);
}
