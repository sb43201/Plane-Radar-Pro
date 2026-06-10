#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>
#include <vector>

#include "adsb.h"
#include "config.h"
#include "display.h"
#include "settings.h"
#include "touch.h"

SettingsStore settingsStore;
AppSettings settings;
DisplayUI display;
TouchInput touch;
ADSBClient adsb;
std::vector<Aircraft> aircraft;

ScreenId currentScreen = ScreenId::Radar;
String selectedHex;
String wifiStatus = "boot";
String lastUpdateText = "No update";
uint32_t lastAdsbMs = 0;
uint32_t lastReconnectMs = 0;
uint32_t lastClockMs = 0;
bool timeConfigured = false;

String localTimeText() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 5)) return "--:--:--";
  char buffer[16];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  return String(buffer);
}

const Aircraft *selectedAircraft() {
  for (const Aircraft &a : aircraft) {
    if (a.hex == selectedHex) return &a;
  }
  return nullptr;
}

void configureTimeIfNeeded() {
  if (timeConfigured || WiFi.status() != WL_CONNECTED) return;
  configTzTime("EST5EDT,M3.2.0,M11.1.0", "pool.ntp.org", "time.nist.gov");
  timeConfigured = true;
  Serial.println("[time] NTP configured");
}

void startWiFi() {
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setDebugOutput(true);
  wm.setConnectTimeout(20);
  wm.setConfigPortalTimeout(180);
  display.drawRadar(settings, aircraft, "setup", localTimeText(), "WiFi setup", true);
  Serial.println("[wifi] starting WiFiManager");
  bool ok = wm.autoConnect(Config::WIFI_AP_NAME);
  wifiStatus = ok ? "online" : "offline";
  Serial.printf("[wifi] %s ip=%s\n", ok ? "connected" : "not connected", WiFi.localIP().toString().c_str());
  configureTimeIfNeeded();
}

void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiStatus = "online";
    configureTimeIfNeeded();
    return;
  }

  wifiStatus = "offline";
  const uint32_t now = millis();
  if (now - lastReconnectMs < Config::WIFI_RECONNECT_MS) return;
  lastReconnectMs = now;
  Serial.println("[wifi] reconnecting");
  WiFi.disconnect();
  WiFi.reconnect();
}

void refreshAdsbIfDue(bool force = false) {
  const uint32_t now = millis();
  if (!force && now - lastAdsbMs < Config::ADSB_REFRESH_MS) return;
  lastAdsbMs = now;

  if (WiFi.status() != WL_CONNECTED) {
    lastUpdateText = "Waiting WiFi";
    display.invalidate();
    return;
  }

  const bool ok = adsb.fetch(settings.homeLat, settings.homeLon, settings.rangeKm, aircraft);
  if (ok) {
    lastUpdateText = aircraft.empty() ? "No aircraft" : "Updated " + localTimeText();
  } else {
    lastUpdateText = adsb.lastError();
  }
  display.invalidate();
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
    case UIAction::ShowSettings:
      currentScreen = ScreenId::Settings;
      break;
    case UIAction::ShowDetail:
      selectedHex = event.aircraftHex;
      currentScreen = ScreenId::Detail;
      break;
    case UIAction::RangeNext:
      cycleRange();
      break;
    case UIAction::ToggleTheme:
      settings.nightMode = !settings.nightMode;
      settingsStore.save(settings);
      Serial.printf("[settings] nightMode=%s\n", settings.nightMode ? "true" : "false");
      break;
    case UIAction::LatPlus:
      settings.homeLat = constrain(settings.homeLat + 0.01f, -90.0f, 90.0f);
      break;
    case UIAction::LatMinus:
      settings.homeLat = constrain(settings.homeLat - 0.01f, -90.0f, 90.0f);
      break;
    case UIAction::LonPlus:
      settings.homeLon += 0.01f;
      if (settings.homeLon > 180.0f) settings.homeLon = -180.0f;
      break;
    case UIAction::LonMinus:
      settings.homeLon -= 0.01f;
      if (settings.homeLon < -180.0f) settings.homeLon = 180.0f;
      break;
    case UIAction::SaveSettings:
      settingsStore.save(settings);
      lastAdsbMs = 0;
      Serial.printf("[settings] saved home=(%.5f, %.5f)\n", settings.homeLat, settings.homeLon);
      break;
  }
  display.invalidate();
}

void drawCurrentScreen(bool force = false) {
  const String timeText = localTimeText();
  switch (currentScreen) {
    case ScreenId::Radar:
      display.drawRadar(settings, aircraft, wifiStatus, timeText, lastUpdateText, force);
      break;
    case ScreenId::AircraftList:
      display.drawAircraftList(settings, aircraft, force);
      break;
    case ScreenId::Detail:
      display.drawAircraftDetail(settings, selectedAircraft(), force);
      break;
    case ScreenId::Settings:
      display.drawSettings(settings, force);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("[boot] Plane Radar Pro starting");

  settingsStore.begin();
  settings = settingsStore.load();
  aircraft.reserve(Config::MAX_AIRCRAFT);

  display.begin(settings);
  touch.begin(settings);
  display.showSplash();

  startWiFi();
  refreshAdsbIfDue(true);
  drawCurrentScreen(true);
}

void loop() {
  maintainWiFi();
  refreshAdsbIfDue();

  TouchPoint point = touch.read(settings);
  UIEvent event = display.handleTouch(point, currentScreen, settings, aircraft);
  handleUiEvent(event);

  const uint32_t now = millis();
  if (now - lastClockMs >= Config::UI_CLOCK_MS) {
    lastClockMs = now;
    drawCurrentScreen(true);
  } else {
    drawCurrentScreen();
  }
}
