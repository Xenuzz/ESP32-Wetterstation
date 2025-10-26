/**The MIT License (MIT)
Copyright (c) 2015 by Daniel Eichhorn
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
See more at http://blog.squix.ch
*/

#include <simpleDSTadjust.h>

// Setup
#define WIFI_SSID "your ssid"
#define WIFI_PASS "your password"
#define WIFI_HOSTNAME "Zihatec-weather-station-ESP32"

const int UPDATE_INTERVAL_SECS = 15 * 60; // Update every 10 minutes
const int SLEEP_INTERVAL_SECS = 0;   // Going to Sleep after idle times, set 0 for dont sleep


// Bright Sky (DWD) settings
// See https://brightsky.dev/docs/ for details about the available parameters.
const double BRIGHT_SKY_LATITUDE = 52.2620;   // Latitude for the requested location
const double BRIGHT_SKY_LONGITUDE = 12.2922;  // Longitude for the requested location
const char* BRIGHT_SKY_TIMEZONE = "Europe/Berlin";
const uint8_t MAX_FORECASTS = 10;
const uint8_t MAX_WEATHER_ALERTS = 3;
String DISPLAYED_CITY_NAME = "Ziesar";

// Adjust according to your language
const String WDAY_NAMES[] = {"SO", "MO", "DI", "MI", "DO", "FR", "SA"};
const String MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OKT", "NOV", "DEZ"};
const String MOON_PHASES[] = {"Neumond", "zunehmende Sichel", "Erstes Viertel", "Waxing Gibbous",
                              "Vollmond", "Waning Gibbous", "Drittes Viertel", "Waning Crescent"};

#define UTC_OFFSET +1
struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600}; // Central European Summer Time = UTC/GMT +2 hours
struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};       // Central European Time = UTC/GMT +1 hour

// Settings for Boston
// #define UTC_OFFSET -5
// struct dstRule StartRule = {"EDT", Second, Sun, Mar, 2, 3600}; // Eastern Daylight time = UTC/GMT -4 hours
// struct dstRule EndRule = {"EST", First, Sun, Nov, 1, 0};       // Eastern Standard time = UTC/GMT -5 hour

// values in metric or imperial system?
const boolean IS_METRIC = true;

// Change for 12 Hour/ 24 hour style clock
bool IS_STYLE_12HR = false;

// change for different ntp (time servers)
#define NTP_SERVERS "0.ch.pool.ntp.org", "1.ch.pool.ntp.org", "2.ch.pool.ntp.org"
// #define NTP_SERVERS "us.pool.ntp.org", "time.nist.gov", "pool.ntp.org"



// Pins for the ILI9341 and ESP32
#define TFT_DC 4
#define TFT_CS 5
#define TFT_LED 15
#define TFT_RST 22

//#define HAVE_TOUCHPAD
#define TOUCH_CS 14
#define TOUCH_IRQ 2 // enable this line for older ArduiTouch or AZ-Touch ESP pcb
// #define TOUCH_IRQ 27 // enable this line for new AZ-Touch MOD pcb




/***************************
 * End Settings
 **************************/
