#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"

struct AppSettings {
  float homeLat = Config::DEFAULT_HOME_LAT;
  float homeLon = Config::DEFAULT_HOME_LON;
  uint16_t rangeKm = Config::DEFAULT_RANGE_KM;
  bool nightMode = false;
  uint8_t displayRotation = Config::DEFAULT_ROTATION;
  int touchMinX = Config::TOUCH_MIN_X;
  int touchMaxX = Config::TOUCH_MAX_X;
  int touchMinY = Config::TOUCH_MIN_Y;
  int touchMaxY = Config::TOUCH_MAX_Y;
  bool gpsLogging = false;
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
