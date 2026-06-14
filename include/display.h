#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <vector>

#include "adsb.h"
#include "airport_manager.h"
#include "settings.h"
#include "touch.h"
#include "wifi_manager_ext.h"

enum class ScreenId : uint8_t {
  Radar,
  AircraftList,
  AirportList,
  Detail,
  AirportDetail,
  Settings,
  WiFiSettings,
  TouchCalibration
};

enum class UIAction : uint8_t {
  None,
  ShowRadar,
  ShowList,
  ShowAirportList,
  ShowSettings,
  ShowWiFiSettings,
  ShowDetail,
  ShowAirportDetail,
  CenterOnAirport,
  StartTouchCalibration,
  RangeNext,
  ToggleTheme,
  ToggleGpsLogging,
  ToggleRadarMode,
  ToggleAirportOverlay,
  ToggleWiFiRadio,
  AirportLabelNext,
  RefreshRateNext,
  CenterModeNext,
  LatPlus,
  LatMinus,
  LonPlus,
  LonMinus,
  SaveSettings,
  RebootDevice,
  SelectWifiNetwork,
  WifiAddPortal,
  WifiDelete,
  WifiMoveUp,
  WifiMoveDown,
  WifiToggle,
  WifiExport,
  WifiImport,
  ResetWiFiHold
};

struct UIEvent {
  UIAction action = UIAction::None;
  String aircraftHex;
  String airportCode;
  size_t wifiIndex = 0;
};

class DisplayUI {
 public:
  void begin(const AppSettings &settings);
  void showSplash();
  void drawRadar(const AppSettings &settings, const std::vector<Aircraft> &aircraft, const String &wifiStatus,
                 const String &gpsStatus, const String &gpsCompass, const String &batteryStatus, const String &timeText,
                 const String &lastUpdateText, const String &alertText, const std::vector<Airport> &airports,
                 const String &airportStatus, bool force = false);
  void drawAircraftList(const AppSettings &settings, const std::vector<Aircraft> &aircraft,
                        const String &lastUpdateText, bool force = false);
  void drawAircraftDetail(const AppSettings &settings, const Aircraft *aircraft, bool force = false);
  void drawAirportList(const AppSettings &settings, const std::vector<Airport> &airports, bool force = false);
  void drawAirportDetail(const AppSettings &settings, const Airport *airport, const std::vector<Aircraft> &aircraft,
                         bool force = false);
  void drawSettings(const AppSettings &settings, const String &wifiStatus, bool force = false);
  void drawWiFiSettings(const AppSettings &settings, const WiFiManagerExt &wifi, bool force = false);
  void drawTouchCalibration(const AppSettings &settings, uint8_t step, bool complete = false);
  void drawWiFiSetup(const AppSettings &settings, const String &savedSsid, const String &status);
  UIEvent handleTouch(const TouchPoint &point, ScreenId screen, const AppSettings &settings,
                      const std::vector<Aircraft> &aircraft, const std::vector<Airport> &airports);
  UIEvent handleTouch(const TouchPoint &point, ScreenId screen, const AppSettings &settings,
                      const std::vector<Aircraft> &aircraft, const std::vector<Airport> &airports,
                      const WiFiManagerExt &wifi);
  void invalidate();

 private:
  TFT_eSPI tft_;
  bool dirty_ = true;
  String selectedHex_;
  String selectedAirportCode_;
  uint8_t aircraftListOffset_ = 0;
  uint8_t airportListOffset_ = 0;

  uint16_t bg(const AppSettings &settings) const;
  uint16_t fg(const AppSettings &settings) const;
  uint16_t muted(const AppSettings &settings) const;
  uint16_t panel(const AppSettings &settings) const;
  uint16_t accent(const AppSettings &settings) const;
  uint16_t altitudeColor(int32_t altFt) const;

  void header(const AppSettings &settings, const String &title, const String &rightText);
  void button(int16_t x, int16_t y, int16_t w, int16_t h, const String &label, uint16_t fill, uint16_t text);
  void drawAircraftIcon(int16_t x, int16_t y, float heading, uint16_t color, uint16_t outlineColor, bool selected);
  void drawBottomNav(const AppSettings &settings, ScreenId active);
  const Aircraft *findAircraft(const std::vector<Aircraft> &aircraft, const String &hex) const;
  String hitAircraft(int16_t x, int16_t y, const AppSettings &settings, const std::vector<Aircraft> &aircraft);
  String hitAirportRow(int16_t x, int16_t y, const AppSettings &settings, const std::vector<Airport> &airports);
  uint8_t airportInRangeCount(const AppSettings &settings, const std::vector<Airport> &airports) const;
  int hitWifiRow(int16_t x, int16_t y, const WiFiManagerExt &wifi);
};
