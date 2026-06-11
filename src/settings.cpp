#include "settings.h"

void SettingsStore::begin() {
  prefs_.begin("plane-radar", false);
}

AppSettings SettingsStore::load() {
  AppSettings s;
  s.homeLat = prefs_.getFloat("homeLat", Config::DEFAULT_HOME_LAT);
  s.homeLon = prefs_.getFloat("homeLon", Config::DEFAULT_HOME_LON);
  s.rangeKm = prefs_.getUShort("rangeKm", Config::DEFAULT_RANGE_KM);
  s.nightMode = prefs_.getBool("night", false);
  s.displayRotation = prefs_.getUChar("rotationP", Config::DEFAULT_ROTATION);
  s.touchMinX = prefs_.getInt("tMinX", Config::TOUCH_MIN_X);
  s.touchMaxX = prefs_.getInt("tMaxX", Config::TOUCH_MAX_X);
  s.touchMinY = prefs_.getInt("tMinY", Config::TOUCH_MIN_Y);
  s.touchMaxY = prefs_.getInt("tMaxY", Config::TOUCH_MAX_Y);
  s.gpsLogging = prefs_.getBool("gpsLog", false);
  s.scopeMode = prefs_.getBool("scope", true);
  s.airportOverlay = prefs_.getBool("aptOv", true);
  s.airportLabelKm = prefs_.getUShort("aptLbl", 50);

  bool rangeOk = false;
  for (size_t i = 0; i < Config::RANGE_OPTION_COUNT; ++i) {
    if (s.rangeKm == Config::RANGE_OPTIONS[i]) {
      rangeOk = true;
      break;
    }
  }
  if (!rangeOk) s.rangeKm = Config::DEFAULT_RANGE_KM;
  bool labelOk = false;
  for (size_t i = 0; i < Config::AIRPORT_LABEL_OPTION_COUNT; ++i) {
    if (s.airportLabelKm == Config::AIRPORT_LABEL_OPTIONS[i]) {
      labelOk = true;
      break;
    }
  }
  if (!labelOk) s.airportLabelKm = 50;
  if (s.displayRotation > 3) s.displayRotation = Config::DEFAULT_ROTATION;
  return s;
}

void SettingsStore::save(const AppSettings &s) {
  prefs_.putFloat("homeLat", s.homeLat);
  prefs_.putFloat("homeLon", s.homeLon);
  prefs_.putUShort("rangeKm", s.rangeKm);
  prefs_.putBool("night", s.nightMode);
  prefs_.putUChar("rotationP", s.displayRotation);
  prefs_.putInt("tMinX", s.touchMinX);
  prefs_.putInt("tMaxX", s.touchMaxX);
  prefs_.putInt("tMinY", s.touchMinY);
  prefs_.putInt("tMaxY", s.touchMaxY);
  prefs_.putBool("gpsLog", s.gpsLogging);
  prefs_.putBool("scope", s.scopeMode);
  prefs_.putBool("aptOv", s.airportOverlay);
  prefs_.putUShort("aptLbl", s.airportLabelKm);
}

void SettingsStore::reset() {
  prefs_.clear();
}
