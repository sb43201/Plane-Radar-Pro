#pragma once

#include <Arduino.h>

namespace Config {
constexpr const char *APP_NAME = "Plane Radar Pro";
constexpr const char *APP_SUBTITLE = "ESP32 ADS-B Tracker";
constexpr const char *WIFI_AP_NAME = "PlaneRadar-Setup";
constexpr const char *WIFI_SETUP_URL = "192.168.4.1";

constexpr uint8_t LCD_CS = 15;
constexpr uint8_t LCD_DC = 2;
constexpr uint8_t LCD_SCK = 14;
constexpr uint8_t LCD_MOSI = 13;
constexpr uint8_t LCD_MISO = 12;
constexpr uint8_t LCD_BL = 27;

constexpr uint8_t TOUCH_CS_PIN = 33;
constexpr uint8_t TOUCH_IRQ_PIN = 36;
constexpr uint8_t TOUCH_MOSI = LCD_MOSI;
constexpr uint8_t TOUCH_MISO = LCD_MISO;
constexpr uint8_t TOUCH_SCK = LCD_SCK;

constexpr uint8_t SD_CS = 5;
constexpr uint8_t SD_SCK = 18;
constexpr uint8_t SD_MOSI = 23;
constexpr uint8_t SD_MISO = 19;
constexpr const char *AIRPORT_CSV_PATH = "/airports.csv";
constexpr float AIRPORT_LOAD_RADIUS_KM = 150.0f;
constexpr size_t AIRPORT_MAX_RETAINED = 50;
constexpr uint32_t AIRPORT_REFRESH_MS = 30000;
constexpr float AIRPORT_RELOAD_MOVE_KM = 0.5f;
constexpr uint16_t AIRPORT_LABEL_OPTIONS[] = {10, 25, 50, 100, 150};
constexpr size_t AIRPORT_LABEL_OPTION_COUNT = sizeof(AIRPORT_LABEL_OPTIONS) / sizeof(AIRPORT_LABEL_OPTIONS[0]);

constexpr bool GPS_ENABLED = true;
constexpr int8_t GPS_RX = 39;
constexpr int8_t GPS_TX = -1;
constexpr uint32_t GPS_BAUD = 9600;
constexpr uint32_t GPS_FIX_MAX_AGE_MS = 10000;
constexpr const char *GPS_LOG_PATH = "/gps_log.csv";
constexpr uint32_t GPS_LOG_INTERVAL_MS = 10000;
constexpr size_t GPS_LOG_MAX_BYTES = 262144;

constexpr uint8_t BATTERY_ADC_PIN = 34;
// Board battery sense uses a 100k/100k divider, so IO34 reads half of BAT+.
constexpr float BATTERY_ADC_DIVIDER = 2.0f;
constexpr uint16_t BATTERY_ADC_SAMPLES = 16;
constexpr uint32_t BATTERY_REFRESH_MS = 2000;

constexpr uint16_t SCREEN_W = 320;
constexpr uint16_t SCREEN_H = 480;
constexpr uint8_t DEFAULT_ROTATION = 0;

constexpr int TOUCH_MIN_X = 300;
constexpr int TOUCH_MAX_X = 3800;
constexpr int TOUCH_MIN_Y = 280;
constexpr int TOUCH_MAX_Y = 3850;

constexpr float DEFAULT_HOME_LAT = 39.7684f;
constexpr float DEFAULT_HOME_LON = -86.1581f;
constexpr uint16_t DEFAULT_RANGE_KM = 25;
constexpr uint16_t RANGE_OPTIONS[] = {10, 25, 50, 100};
constexpr size_t RANGE_OPTION_COUNT = sizeof(RANGE_OPTIONS) / sizeof(RANGE_OPTIONS[0]);

constexpr uint32_t ADSB_REFRESH_MS = 5000;
constexpr uint32_t WIFI_RECONNECT_MS = 10000;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 60000;
constexpr uint32_t WIFI_RESET_HOLD_MS = 3000;
constexpr uint32_t UI_CLOCK_MS = 1000;
constexpr float ALERT_DISTANCE_KM = 2.0f;
constexpr int32_t ALERT_LOW_ALT_FT = 3000;
constexpr float EDGE_MARKER_RANGE_MULTIPLIER = 1.5f;
constexpr size_t MAX_AIRCRAFT = 80;
constexpr size_t TRAIL_POINTS = 20;
constexpr size_t JSON_DOC_SIZE = 65536;
}  // namespace Config
