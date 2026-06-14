#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"

enum CenterMode : uint8_t {
  CENTER_MANUAL = 0,
  CENTER_GPS = 1,
  CENTER_AIRPORT = 2
};

struct AppSettings {
  float homeLat = Config::DEFAULT_HOME_LAT;
  float homeLon = Config::DEFAULT_HOME_LON;
  uint8_t centerMode = CENTER_MANUAL;
  float airportCenterLat = NAN;
  float airportCenterLon = NAN;
  String airportCenterCode = "";
  uint16_t rangeKm = Config::DEFAULT_RANGE_KM;
  bool nightMode = false;
  uint8_t displayRotation = Config::DEFAULT_ROTATION;
  int touchMinX = Config::TOUCH_MIN_X;
  int touchMaxX = Config::TOUCH_MAX_X;
  int touchMinY = Config::TOUCH_MIN_Y;
  int touchMaxY = Config::TOUCH_MAX_Y;
  bool touchCalibrated = false;
  bool gpsLogging = false;
  bool scopeMode = true;
  bool airportOverlay = true;
  bool wifiRadioEnabled = true;
  uint16_t airportLabelKm = 50;
  uint16_t adsbRefreshSec = 5;
};

class SettingsStore {
 public:
  void begin();
  AppSettings load();
  void save(const AppSettings &settings);
  void reset();

 private:
  Preferences prefs_;
};
