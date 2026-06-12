#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>
#include <vector>

#include "adsb.h"
#include "airport_manager.h"
#include "config.h"
#include "display.h"
#include "gps.h"
#include "radar.h"
#include "settings.h"
#include "touch.h"

SettingsStore settingsStore;
AppSettings settings;
DisplayUI display;
TouchInput touch;
ADSBClient adsb;
GPSModule gps;
AirportManager airportManager;
std::vector<Aircraft> aircraft;

ScreenId currentScreen = ScreenId::Radar;
String selectedHex;
String selectedAirportCode;
String wifiStatus = "boot";
String lastUpdateText = "No update";
uint32_t lastAdsbMs = 0;
uint32_t lastReconnectMs = 0;
uint32_t lastClockMs = 0;
uint32_t lastBatteryMs = 0;
uint32_t lastGpsLogMs = 0;
bool timeConfigured = false;
bool wifiPortalSaved = false;
bool filesystemReady = false;
uint32_t resetWiFiHoldStartMs = 0;
uint32_t resetWiFiLastTouchMs = 0;
String gpsStatus = "No GPS";
String gpsCompassStatus = "";
String batteryStatus = "Bat --";
String alertStatus = "";
RawTouchPoint calibrationPoints[4];
uint8_t calibrationStep = 0;
bool calibrationWaitingForRelease = false;
bool airportCenterActive = false;
bool startupCalibrationMode = false;

void updateAircraftAlerts();
void processGpsLogging();
void updateAirports(bool force = false);
void continueStartupAfterCalibration();
void startTouchCalibration(bool startupMode);

void markWifiPortalSaved() {
  wifiPortalSaved = true;
  Serial.println("[wifi] credentials saved from captive portal");
}

String localTimeText() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 5)) return "--:--:--";
  char buffer[16];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  return String(buffer);
}

String localDateTimeText() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 5)) return "";
  char buffer[24];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

const Aircraft *selectedAircraft() {
  for (const Aircraft &a : aircraft) {
    if (a.hex == selectedHex) return &a;
  }
  return nullptr;
}

String aircraftName(const Aircraft &a) {
  return a.flight.length() ? a.flight : a.hex;
}

String compassPoint(float deg) {
  if (isnan(deg)) return "---";
  static const char *POINTS[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  const uint8_t index = (uint8_t)floorf(fmodf(deg + 22.5f, 360.0f) / 45.0f);
  return POINTS[index];
}

void configureTimeIfNeeded() {
  if (timeConfigured || WiFi.status() != WL_CONNECTED) return;
  configTzTime("EST5EDT,M3.2.0,M11.1.0", "pool.ntp.org", "time.nist.gov");
  timeConfigured = true;
  Serial.println("[time] NTP configured");
}

void updateBatteryStatus(bool force = false) {
  const uint32_t now = millis();
  if (!force && now - lastBatteryMs < Config::BATTERY_REFRESH_MS) return;
  lastBatteryMs = now;

  uint32_t millivolts = 0;
  for (uint16_t i = 0; i < Config::BATTERY_ADC_SAMPLES; ++i) {
    millivolts += analogReadMilliVolts(Config::BATTERY_ADC_PIN);
  }
  millivolts /= Config::BATTERY_ADC_SAMPLES;

  const float batteryVolts = (millivolts / 1000.0f) * Config::BATTERY_ADC_DIVIDER;
  String newStatus = "Bat " + String(batteryVolts, 2) + "V";
  if (newStatus != batteryStatus) {
    batteryStatus = newStatus;
    display.invalidate();
    Serial.printf("[battery] adc=%lu mV battery=%.2f V\n", millivolts, batteryVolts);
  }
}

const Airport *selectedAirport() {
  return airportManager.findByCode(selectedAirportCode);
}

void initializeGpsLog() {
  filesystemReady = SPIFFS.begin(true);
  if (!filesystemReady) {
    Serial.println("[gps-log] SPIFFS mount failed; GPS logging disabled until reboot");
    return;
  }

  if (!SPIFFS.exists(Config::GPS_LOG_PATH)) {
    File file = SPIFFS.open(Config::GPS_LOG_PATH, FILE_WRITE);
    if (!file) {
      Serial.println("[gps-log] could not create log file");
      return;
    }
    file.println("millis,local_time,lat,lon,speed_kmph,course_deg,satellites");
    file.close();
  }
  Serial.printf("[gps-log] ready path=%s size=%u bytes\n", Config::GPS_LOG_PATH,
                (unsigned)SPIFFS.open(Config::GPS_LOG_PATH, FILE_READ).size());
}

void rotateGpsLogIfNeeded() {
  if (!filesystemReady || !SPIFFS.exists(Config::GPS_LOG_PATH)) return;
  File file = SPIFFS.open(Config::GPS_LOG_PATH, FILE_READ);
  if (!file) return;
  const size_t size = file.size();
  file.close();
  if (size < Config::GPS_LOG_MAX_BYTES) return;

  Serial.printf("[gps-log] rotating log at %u bytes\n", (unsigned)size);
  SPIFFS.remove(Config::GPS_LOG_PATH);
  File fresh = SPIFFS.open(Config::GPS_LOG_PATH, FILE_WRITE);
  if (fresh) {
    fresh.println("millis,local_time,lat,lon,speed_kmph,course_deg,satellites");
    fresh.close();
  }
}

void appendGpsLogLine() {
  if (!filesystemReady || !gps.hasFix()) return;
  rotateGpsLogIfNeeded();

  File file = SPIFFS.open(Config::GPS_LOG_PATH, FILE_APPEND);
  if (!file) {
    Serial.println("[gps-log] append failed");
    return;
  }

  const float speed = gps.speedKmph();
  const float course = gps.courseDeg();
  file.print(millis());
  file.print(',');
  file.print(localDateTimeText());
  file.print(',');
  file.print(gps.latitude(), 6);
  file.print(',');
  file.print(gps.longitude(), 6);
  file.print(',');
  file.print(isnan(speed) ? String("") : String(speed, 1));
  file.print(',');
  file.print(isnan(course) ? String("") : String(course, 1));
  file.print(',');
  file.println(gps.satellites());
  file.close();
  Serial.printf("[gps-log] wrote %.6f,%.6f\n", gps.latitude(), gps.longitude());
}

void processGpsLogging() {
  if (!settings.gpsLogging) return;
  const uint32_t now = millis();
  if (lastGpsLogMs != 0 && now - lastGpsLogMs < Config::GPS_LOG_INTERVAL_MS) return;
  lastGpsLogMs = now;
  appendGpsLogLine();
}

void startWiFi() {
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setDebugOutput(true);
  wm.setConnectTimeout(60);
  wm.setConnectRetries(3);
  wm.setSaveConfigCallback(markWifiPortalSaved);

  const String savedSsid = wm.getWiFiSSID(true);
  if (savedSsid.length()) {
    wifiStatus = "Searching";
    Serial.printf("[wifi] trying saved hotspot: %s\n", savedSsid.c_str());
    display.drawWiFiSetup(settings, savedSsid, "WiFi: Searching");
    WiFi.begin();
    const uint32_t startMs = millis();
    uint32_t lastDrawMs = 0;
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < Config::WIFI_CONNECT_TIMEOUT_MS) {
      if (millis() - lastDrawMs >= 1000) {
        lastDrawMs = millis();
        display.drawWiFiSetup(settings, savedSsid, "Trying saved hotspot");
        Serial.printf("[wifi] connecting to %s, status=%d\n", savedSsid.c_str(), WiFi.status());
      }
      delay(50);
    }
    if (WiFi.status() == WL_CONNECTED) {
      wifiStatus = "Connected";
      Serial.printf("[wifi] connected ip=%s\n", WiFi.localIP().toString().c_str());
      configureTimeIfNeeded();
      return;
    }
  }

  wifiStatus = "Setup Mode";
  display.drawWiFiSetup(settings, savedSsid, "WiFi: Setup Mode");
  Serial.println("[wifi] starting WiFiManager setup portal");
  bool ok = wm.autoConnect(Config::WIFI_AP_NAME);
  wifiStatus = ok ? "Connected" : "Searching";
  Serial.printf("[wifi] %s ip=%s\n", ok ? "connected" : "not connected", WiFi.localIP().toString().c_str());
  if (ok && wifiPortalSaved) {
    Serial.println("[wifi] portal saved credentials; rebooting into radar mode");
    delay(500);
    ESP.restart();
  }
  configureTimeIfNeeded();
}

void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiStatus = "Connected";
    configureTimeIfNeeded();
    return;
  }

  wifiStatus = "Searching";
  if (lastUpdateText != "WiFi lost") {
    lastUpdateText = "WiFi lost";
    display.invalidate();
  }
  const uint32_t now = millis();
  if (now - lastReconnectMs < Config::WIFI_RECONNECT_MS) return;
  lastReconnectMs = now;
  Serial.println("[wifi] reconnecting");
  WiFi.reconnect();
}

void refreshAdsbIfDue(bool force = false) {
  const uint32_t now = millis();
  if (!force && now - lastAdsbMs < Config::ADSB_REFRESH_MS) return;
  lastAdsbMs = now;

  if (WiFi.status() != WL_CONNECTED) {
    lastUpdateText = "WiFi lost";
    display.invalidate();
    return;
  }

  const bool ok = adsb.fetch(settings.homeLat, settings.homeLon, settings.rangeKm, aircraft);
  if (ok) {
    lastUpdateText = aircraft.empty() ? "No aircraft" : "Updated " + localTimeText();
    updateAircraftAlerts();
  } else {
    lastUpdateText = adsb.lastError();
  }
  display.invalidate();
}

void updateAircraftAlerts() {
  String nearestAlert = "";
  String altitudeAlert = "";
  float nearestDistance = Config::ALERT_DISTANCE_KM + 1.0f;
  int32_t lowestAltitude = INT32_MAX;

  for (const Aircraft &a : aircraft) {
    if (!isnan(a.lat) && !isnan(a.lon)) {
      const float distance = Radar::distanceKm(settings.homeLat, settings.homeLon, a.lat, a.lon);
      if (distance <= Config::ALERT_DISTANCE_KM && distance < nearestDistance) {
        nearestDistance = distance;
        nearestAlert = aircraftName(a) + " " + String(distance, 1) + " km";
      }
    }

    if (a.altBaro != INT32_MIN && a.altBaro < Config::ALERT_LOW_ALT_FT && a.altBaro < lowestAltitude) {
      lowestAltitude = a.altBaro;
      altitudeAlert = aircraftName(a) + " " + String(a.altBaro) + " ft";
    }
  }

  String newAlert = "";
  if (nearestAlert.length()) {
    newAlert = "Close: " + nearestAlert;
  } else if (altitudeAlert.length()) {
    newAlert = "Low alt: " + altitudeAlert;
  }

  if (newAlert != alertStatus) {
    alertStatus = newAlert;
    display.invalidate();
    if (alertStatus.length()) Serial.printf("[alert] %s\n", alertStatus.c_str());
  }
}

void updateGpsPosition() {
  gps.update();
  const String newStatus = gps.statusText();
  if (newStatus != gpsStatus) {
    gpsStatus = newStatus;
    display.invalidate();
    Serial.printf("[gps] status=%s\n", gpsStatus.c_str());
  }

  String newCompass = "";
  if (gps.hasCourse()) {
    const float course = gps.courseDeg();
    newCompass = compassPoint(course) + " " + String(course, 0) + " deg";
  }
  if (newCompass != gpsCompassStatus) {
    gpsCompassStatus = newCompass;
    display.invalidate();
    if (gpsCompassStatus.length()) Serial.printf("[gps] compass=%s\n", gpsCompassStatus.c_str());
  }

  if (!gps.hasFix() || airportCenterActive) return;

  const float lat = gps.latitude();
  const float lon = gps.longitude();
  if (isnan(lat) || isnan(lon)) return;

  if (fabs(settings.homeLat - lat) > 0.00005f || fabs(settings.homeLon - lon) > 0.00005f) {
    settings.homeLat = lat;
    settings.homeLon = lon;
    lastAdsbMs = 0;
    updateAirports(true);
    display.invalidate();
    Serial.printf("[gps] using GPS home position %.6f, %.6f\n", settings.homeLat, settings.homeLon);
  }
}

void updateAirports(bool force) {
  if (airportManager.refreshIfDue(settings.homeLat, settings.homeLon, force)) {
    display.invalidate();
  }
}

void cycleRange() {
  size_t index = 0;
  for (size_t i = 0; i < Config::RANGE_OPTION_COUNT; ++i) {
    if (settings.rangeKm == Config::RANGE_OPTIONS[i]) {
      index = i;
      break;
    }
  }
  settings.rangeKm = Config::RANGE_OPTIONS[(index + 1) % Config::RANGE_OPTION_COUNT];
  settingsStore.save(settings);
  lastAdsbMs = 0;
  Serial.printf("[settings] range=%u km\n", settings.rangeKm);
}

void handleUiEvent(const UIEvent &event) {
  switch (event.action) {
    case UIAction::None:
      return;
    case UIAction::ShowRadar:
      currentScreen = ScreenId::Radar;
      break;
    case UIAction::ShowList:
      currentScreen = ScreenId::AircraftList;
      break;
    case UIAction::ShowAirportList:
      currentScreen = ScreenId::AirportList;
      break;
    case UIAction::ShowSettings:
      currentScreen = ScreenId::Settings;
      break;
    case UIAction::ShowDetail:
      selectedHex = event.aircraftHex;
      currentScreen = ScreenId::Detail;
      break;
    case UIAction::ShowAirportDetail:
      selectedAirportCode = event.airportCode;
      currentScreen = ScreenId::AirportDetail;
      break;
    case UIAction::CenterOnAirport: {
      if (event.airportCode.length()) selectedAirportCode = event.airportCode;
      const Airport *airport = selectedAirport();
      if (airport) {
        const String airportCode = AirportManager::displayCode(*airport);
        settings.homeLat = airport->lat;
        settings.homeLon = airport->lon;
        lastAdsbMs = 0;
        updateAirports(true);
        refreshAdsbIfDue(true);
        airportCenterActive = true;
        currentScreen = ScreenId::Radar;
        Serial.printf("[airport] radar centered on %s %.6f, %.6f\n", airportCode.c_str(), settings.homeLat,
                      settings.homeLon);
      }
      break;
    }
    case UIAction::StartTouchCalibration:
      startTouchCalibration(false);
      return;
    case UIAction::RangeNext:
      cycleRange();
      break;
    case UIAction::ToggleTheme:
      settings.nightMode = !settings.nightMode;
      settingsStore.save(settings);
      Serial.printf("[settings] nightMode=%s\n", settings.nightMode ? "true" : "false");
      break;
    case UIAction::ToggleGpsLogging:
      settings.gpsLogging = !settings.gpsLogging;
      settingsStore.save(settings);
      lastGpsLogMs = 0;
      Serial.printf("[settings] gpsLogging=%s\n", settings.gpsLogging ? "true" : "false");
      break;
    case UIAction::ToggleRadarMode:
      settings.scopeMode = !settings.scopeMode;
      settingsStore.save(settings);
      Serial.printf("[settings] radarMode=%s\n", settings.scopeMode ? "scope" : "radar");
      break;
    case UIAction::ToggleAirportOverlay:
      settings.airportOverlay = !settings.airportOverlay;
      settingsStore.save(settings);
      Serial.printf("[settings] airportOverlay=%s\n", settings.airportOverlay ? "true" : "false");
      break;
    case UIAction::AirportLabelNext: {
      size_t index = 0;
      for (size_t i = 0; i < Config::AIRPORT_LABEL_OPTION_COUNT; ++i) {
        if (settings.airportLabelKm == Config::AIRPORT_LABEL_OPTIONS[i]) {
          index = i;
          break;
        }
      }
      settings.airportLabelKm = Config::AIRPORT_LABEL_OPTIONS[(index + 1) % Config::AIRPORT_LABEL_OPTION_COUNT];
      settingsStore.save(settings);
      Serial.printf("[settings] airportLabelKm=%u\n", settings.airportLabelKm);
      break;
    }
    case UIAction::LatPlus:
      airportCenterActive = false;
      settings.homeLat = constrain(settings.homeLat + 0.01f, -90.0f, 90.0f);
      break;
    case UIAction::LatMinus:
      airportCenterActive = false;
      settings.homeLat = constrain(settings.homeLat - 0.01f, -90.0f, 90.0f);
      break;
    case UIAction::LonPlus:
      airportCenterActive = false;
      settings.homeLon += 0.01f;
      if (settings.homeLon > 180.0f) settings.homeLon = -180.0f;
      break;
    case UIAction::LonMinus:
      airportCenterActive = false;
      settings.homeLon -= 0.01f;
      if (settings.homeLon < -180.0f) settings.homeLon = 180.0f;
      break;
    case UIAction::SaveSettings:
      settingsStore.save(settings);
      lastAdsbMs = 0;
      Serial.printf("[settings] saved home=(%.5f, %.5f)\n", settings.homeLat, settings.homeLon);
      break;
    case UIAction::ResetWiFiHold:
      break;
  }
  display.invalidate();
}

void finishTouchCalibration() {
  const int leftX = ((int)calibrationPoints[0].x + calibrationPoints[3].x) / 2;
  const int rightX = ((int)calibrationPoints[1].x + calibrationPoints[2].x) / 2;
  const int topY = ((int)calibrationPoints[0].y + calibrationPoints[1].y) / 2;
  const int bottomY = ((int)calibrationPoints[2].y + calibrationPoints[3].y) / 2;

  if (abs(rightX - leftX) < 300 || abs(bottomY - topY) < 300) {
    Serial.println("[touch-cal] rejected calibration: points too close");
    if (startupCalibrationMode) {
      startTouchCalibration(true);
      return;
    }
    currentScreen = ScreenId::Settings;
    display.invalidate();
    return;
  }

  const float targetLeft = 28.0f;
  const float targetRight = Config::SCREEN_W - 28.0f;
  const float targetTop = 60.0f;
  const float targetBottom = Config::SCREEN_H - 28.0f;
  const float xScale = (rightX - leftX) / (targetRight - targetLeft);
  const float yScale = (bottomY - topY) / (targetBottom - targetTop);

  settings.touchMinX = lroundf(leftX - xScale * targetLeft);
  settings.touchMaxX = lroundf(leftX + xScale * ((Config::SCREEN_W - 1) - targetLeft));
  settings.touchMinY = lroundf(topY - yScale * targetTop);
  settings.touchMaxY = lroundf(topY + yScale * ((Config::SCREEN_H - 1) - targetTop));
  settings.touchCalibrated = true;
  settingsStore.save(settings);
  Serial.printf("[touch-cal] saved x=(%d,%d) y=(%d,%d)\n", settings.touchMinX, settings.touchMaxX,
                settings.touchMinY, settings.touchMaxY);
  display.drawTouchCalibration(settings, calibrationStep, true);
  delay(900);
  if (startupCalibrationMode) {
    startupCalibrationMode = false;
    continueStartupAfterCalibration();
    return;
  }
  currentScreen = ScreenId::Settings;
  display.invalidate();
}

void processTouchCalibration() {
  if (currentScreen != ScreenId::TouchCalibration) return;

  if (calibrationWaitingForRelease) {
    if (!touch.isTouched()) {
      calibrationWaitingForRelease = false;
      if (calibrationStep >= 4) {
        finishTouchCalibration();
      } else {
        display.drawTouchCalibration(settings, calibrationStep, false);
      }
    }
    return;
  }

  RawTouchPoint raw = touch.readRaw();
  if (!raw.touched) return;

  calibrationPoints[calibrationStep] = raw;
  Serial.printf("[touch-cal] point %u raw=(%d,%d,%d)\n", calibrationStep + 1, raw.x, raw.y, raw.z);
  calibrationStep++;
  calibrationWaitingForRelease = true;
}

void resetWiFiAndRestart() {
  Serial.println("[wifi] reset requested; clearing credentials and restarting");
  display.drawWiFiSetup(settings, "", "Clearing WiFi");
  WiFiManager wm;
  wm.resetSettings();
  WiFi.disconnect(true, true);
  delay(500);
  ESP.restart();
}

void processResetWiFiHold(const UIEvent &event) {
  const uint32_t now = millis();
  if (event.action == UIAction::ResetWiFiHold) {
    if (resetWiFiHoldStartMs == 0) {
      resetWiFiHoldStartMs = now;
      Serial.println("[wifi] reset hold started");
    }
    resetWiFiLastTouchMs = now;
    lastUpdateText = "Hold Reset WiFi";
  } else if (!touch.isTouched() || (resetWiFiLastTouchMs > 0 && now - resetWiFiLastTouchMs > 600)) {
    resetWiFiHoldStartMs = 0;
    resetWiFiLastTouchMs = 0;
  }

  if (resetWiFiHoldStartMs > 0 && now - resetWiFiHoldStartMs >= Config::WIFI_RESET_HOLD_MS) {
    resetWiFiAndRestart();
  }
}

void drawCurrentScreen(bool force = false) {
  const String timeText = localTimeText();
  switch (currentScreen) {
    case ScreenId::Radar:
      display.drawRadar(settings, aircraft, wifiStatus, gpsStatus, gpsCompassStatus, batteryStatus, timeText, lastUpdateText,
                        alertStatus, airportManager.airports(), airportManager.statusText(), force);
      break;
    case ScreenId::AircraftList:
      display.drawAircraftList(settings, aircraft, force);
      break;
    case ScreenId::AirportList:
      display.drawAirportList(settings, airportManager.airports(), force);
      break;
    case ScreenId::Detail:
      display.drawAircraftDetail(settings, selectedAircraft(), force);
      break;
    case ScreenId::AirportDetail:
      display.drawAirportDetail(settings, selectedAirport(), aircraft, force);
      break;
    case ScreenId::Settings:
      display.drawSettings(settings, force);
      break;
    case ScreenId::TouchCalibration:
      display.drawTouchCalibration(settings, calibrationStep, false);
      break;
  }
}

void startTouchCalibration(bool startupMode) {
  startupCalibrationMode = startupMode;
  currentScreen = ScreenId::TouchCalibration;
  calibrationStep = 0;
  calibrationWaitingForRelease = false;
  display.drawTouchCalibration(settings, calibrationStep, false);
  Serial.printf("[touch-cal] calibration started%s\n", startupMode ? " at first boot" : "");
}

void continueStartupAfterCalibration() {
  startWiFi();
  refreshAdsbIfDue(true);
  currentScreen = ScreenId::Radar;
  drawCurrentScreen(true);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("[boot] Plane Radar Pro starting");
  analogReadResolution(12);
  analogSetPinAttenuation(Config::BATTERY_ADC_PIN, ADC_11db);

  settingsStore.begin();
  settings = settingsStore.load();
  initializeGpsLog();
  aircraft.reserve(Config::MAX_AIRCRAFT);

  display.begin(settings);
  airportManager.begin();
  airportManager.loadNearby(settings.homeLat, settings.homeLon);
  touch.begin(settings);
  gps.begin();
  updateBatteryStatus(true);
  display.showSplash();

  if (!settings.touchCalibrated) {
    startTouchCalibration(true);
    return;
  }

  continueStartupAfterCalibration();
}

void loop() {
  processTouchCalibration();
  if (currentScreen == ScreenId::TouchCalibration) {
    return;
  }

  updateGpsPosition();
  updateAirports();
  processGpsLogging();
  updateBatteryStatus();
  maintainWiFi();
  refreshAdsbIfDue();

  TouchPoint point = touch.read(settings);
  UIEvent event = display.handleTouch(point, currentScreen, settings, aircraft, airportManager.airports());
  processResetWiFiHold(event);
  handleUiEvent(event);
  if (event.action != UIAction::None) {
    drawCurrentScreen(true);
    return;
  }

  const uint32_t now = millis();
  if (now - lastClockMs >= Config::UI_CLOCK_MS) {
    lastClockMs = now;
    drawCurrentScreen(true);
  } else {
    drawCurrentScreen();
  }
}
