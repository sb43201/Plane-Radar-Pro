#include "display.h"

#include <math.h>
#include <WiFi.h>

#include "config.h"
#include "radar.h"

namespace {
bool inRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
  return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

String altText(int32_t alt) {
  if (alt == INT32_MIN) return "---";
  return String(alt) + " ft";
}

String speedText(float gs) {
  if (isnan(gs)) return "---";
  return String(gs, 0) + " kt";
}

String headingText(float track) {
  if (isnan(track)) return "---";
  return String(track, 0) + " deg";
}

String safeFlight(const Aircraft &a) {
  return a.flight.length() ? a.flight : a.hex;
}

String aircraftSubtitle(const Aircraft &a) {
  return a.type.length() ? a.type : (a.category.length() ? a.category : String("---"));
}

float angleDiff(float a, float b) {
  float diff = fabsf(a - b);
  while (diff >= 360.0f) diff -= 360.0f;
  return diff > 180.0f ? 360.0f - diff : diff;
}

const char *uiActionName(UIAction action) {
  switch (action) {
    case UIAction::None:
      return "None";
    case UIAction::ShowRadar:
      return "ShowRadar";
    case UIAction::ShowList:
      return "ShowList";
    case UIAction::ShowAirportList:
      return "ShowAirportList";
    case UIAction::ShowSettings:
      return "ShowSettings";
    case UIAction::ShowWiFiSettings:
      return "ShowWiFiSettings";
    case UIAction::ShowDetail:
      return "ShowDetail";
    case UIAction::ShowAirportDetail:
      return "ShowAirportDetail";
    case UIAction::CenterOnAirport:
      return "CenterOnAirport";
    case UIAction::StartTouchCalibration:
      return "StartTouchCalibration";
    case UIAction::RangeNext:
      return "RangeNext";
    case UIAction::ToggleTheme:
      return "ToggleTheme";
    case UIAction::ToggleGpsLogging:
      return "ToggleGpsLogging";
    case UIAction::ToggleRadarMode:
      return "ToggleRadarMode";
    case UIAction::ToggleAirportOverlay:
      return "ToggleAirportOverlay";
    case UIAction::AirportLabelNext:
      return "AirportLabelNext";
    case UIAction::RefreshRateNext:
      return "RefreshRateNext";
    case UIAction::LatPlus:
      return "LatPlus";
    case UIAction::LatMinus:
      return "LatMinus";
    case UIAction::LonPlus:
      return "LonPlus";
    case UIAction::LonMinus:
      return "LonMinus";
    case UIAction::SaveSettings:
      return "SaveSettings";
    case UIAction::RebootDevice:
      return "RebootDevice";
    case UIAction::SelectWifiNetwork:
      return "SelectWifiNetwork";
    case UIAction::WifiAddPortal:
      return "WifiAddPortal";
    case UIAction::WifiDelete:
      return "WifiDelete";
    case UIAction::WifiMoveUp:
      return "WifiMoveUp";
    case UIAction::WifiMoveDown:
      return "WifiMoveDown";
    case UIAction::WifiToggle:
      return "WifiToggle";
    case UIAction::WifiExport:
      return "WifiExport";
    case UIAction::WifiImport:
      return "WifiImport";
    case UIAction::ResetWiFiHold:
      return "ResetWiFiHold";
  }
  return "Unknown";
}

void drawEdgeMarker(TFT_eSPI &tft, int16_t cx, int16_t cy, int16_t radius, float bearingDeg, float distanceKm,
                    uint16_t bgColor) {
  const float angle = bearingDeg * PI / 180.0f;
  const int16_t x = cx + roundf(sinf(angle) * (radius - 2));
  const int16_t y = cy - roundf(cosf(angle) * (radius - 2));
  const int16_t tailX = cx + roundf(sinf(angle) * (radius - 12));
  const int16_t tailY = cy - roundf(cosf(angle) * (radius - 12));
  tft.fillCircle(x, y, 4, TFT_RED);
  tft.drawLine(tailX, tailY, x, y, TFT_RED);
  tft.setTextFont(1);
  tft.setTextColor(TFT_RED, bgColor);
  const int16_t labelX = x < cx ? x + 6 : x - 28;
  const int16_t labelY = y < cy ? y + 5 : y - 13;
  tft.drawString(String(distanceKm, 0) + "km", labelX, labelY);
}

}  // namespace

void DisplayUI::begin(const AppSettings &settings) {
  pinMode(Config::LCD_BL, OUTPUT);
  digitalWrite(Config::LCD_BL, HIGH);
  tft_.init();
  tft_.setRotation(settings.displayRotation);
  tft_.setTextDatum(TL_DATUM);
  tft_.setTextFont(2);
  tft_.fillScreen(TFT_BLACK);
  Serial.printf("[display] initialized %dx%d rotation=%u\n", tft_.width(), tft_.height(), settings.displayRotation);
}

void DisplayUI::showSplash() {
  tft_.fillScreen(TFT_NAVY);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(TFT_WHITE, TFT_NAVY);
  tft_.setTextFont(4);
  tft_.drawString(Config::APP_NAME, tft_.width() / 2, tft_.height() / 2 - 24);
  tft_.setTextFont(2);
  tft_.drawString(Config::APP_SUBTITLE, tft_.width() / 2, tft_.height() / 2 + 18);
  tft_.setTextDatum(TL_DATUM);
  delay(1200);
  dirty_ = true;
}

void DisplayUI::invalidate() {
  dirty_ = true;
}

uint16_t DisplayUI::bg(const AppSettings &settings) const {
  return settings.nightMode ? TFT_BLACK : TFT_WHITE;
}

uint16_t DisplayUI::fg(const AppSettings &settings) const {
  return settings.nightMode ? TFT_WHITE : TFT_BLACK;
}

uint16_t DisplayUI::muted(const AppSettings &settings) const {
  return settings.nightMode ? TFT_DARKGREY : 0x7BEF;
}

uint16_t DisplayUI::panel(const AppSettings &settings) const {
  return settings.nightMode ? 0x1082 : 0xEF7D;
}

uint16_t DisplayUI::accent(const AppSettings &settings) const {
  return settings.nightMode ? TFT_CYAN : TFT_BLUE;
}

uint16_t DisplayUI::altitudeColor(int32_t altFt) const {
  if (altFt == INT32_MIN || altFt < 5000) return TFT_RED;
  if (altFt <= 20000) return TFT_YELLOW;
  return TFT_GREEN;
}

void DisplayUI::header(const AppSettings &settings, const String &title, const String &rightText) {
  tft_.fillRect(0, 0, tft_.width(), 32, settings.nightMode ? 0x0841 : 0xD69A);
  tft_.setTextColor(fg(settings), settings.nightMode ? 0x0841 : 0xD69A);
  tft_.setTextFont(2);
  tft_.drawString(title, 8, 8);
  tft_.setTextDatum(TR_DATUM);
  tft_.drawString(rightText, tft_.width() - 8, 8);
  tft_.setTextDatum(TL_DATUM);
}

void DisplayUI::button(int16_t x, int16_t y, int16_t w, int16_t h, const String &label, uint16_t fill,
                       uint16_t text) {
  tft_.fillRoundRect(x, y, w, h, 6, fill);
  tft_.drawRoundRect(x, y, w, h, 6, TFT_DARKGREY);
  tft_.setTextColor(text, fill);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextFont(2);
  tft_.drawString(label, x + w / 2, y + h / 2 - 1);
  tft_.setTextDatum(TL_DATUM);
}

void DisplayUI::drawAircraftIcon(int16_t x, int16_t y, float heading, uint16_t color, bool selected) {
  const float angle = (isnan(heading) ? 0 : heading) * PI / 180.0f;
  const int16_t noseX = x + roundf(sinf(angle) * 10);
  const int16_t noseY = y - roundf(cosf(angle) * 10);
  const int16_t leftX = x + roundf(sinf(angle + 2.45f) * 8);
  const int16_t leftY = y - roundf(cosf(angle + 2.45f) * 8);
  const int16_t rightX = x + roundf(sinf(angle - 2.45f) * 8);
  const int16_t rightY = y - roundf(cosf(angle - 2.45f) * 8);
  tft_.fillTriangle(noseX, noseY, leftX, leftY, rightX, rightY, color);
  tft_.drawTriangle(noseX, noseY, leftX, leftY, rightX, rightY, TFT_BLACK);
  if (selected) tft_.drawCircle(x, y, 13, TFT_WHITE);
}

void DisplayUI::drawBottomNav(const AppSettings &settings, ScreenId active) {
  const int16_t y = tft_.height() - 38;
  const int16_t gap = 4;
  const int16_t w = (tft_.width() - gap * 6) / 5;
  tft_.fillRect(0, y, tft_.width(), 38, panel(settings));
  const bool compact = w < 56;
  button(gap, y + 5, w, 28, compact ? "Rad" : "Radar",
         active == ScreenId::Radar ? accent(settings) : muted(settings), TFT_WHITE);
  button(gap * 2 + w, y + 5, w, 28, "AC", active == ScreenId::AircraftList ? accent(settings) : muted(settings),
         TFT_WHITE);
  const bool airportActive = active == ScreenId::AirportList || active == ScreenId::AirportDetail;
  button(gap * 3 + w * 2, y + 5, w, 28, "APT", airportActive ? accent(settings) : panel(settings),
         airportActive ? TFT_WHITE : fg(settings));
  button(gap * 4 + w * 3, y + 5, w, 28, compact ? "Rng" : "Range", panel(settings), fg(settings));
  button(gap * 5 + w * 4, y + 5, w, 28, compact ? "Set" : "Setup",
         active == ScreenId::Settings ? accent(settings) : muted(settings),
         TFT_WHITE);
}

void DisplayUI::drawRadar(const AppSettings &settings, const std::vector<Aircraft> &aircraft,
                          const String &wifiStatus, const String &gpsStatus, const String &gpsCompass,
                          const String &batteryStatus, const String &timeText, const String &lastUpdateText,
                          const String &alertText, const std::vector<Airport> &airports,
                          const String &airportStatus, bool force) {
  if (!dirty_ && !force) return;
  dirty_ = false;
  const uint16_t scopeBg = settings.scopeMode ? TFT_BLACK : bg(settings);
  const uint16_t grid = settings.scopeMode ? 0x07E0 : accent(settings);
  const uint16_t gridDim = settings.scopeMode ? 0x03A0 : muted(settings);
  const uint16_t text = settings.scopeMode ? 0xB7FF : fg(settings);
  const uint16_t dimText = settings.scopeMode ? 0x7BEF : muted(settings);
  const uint16_t airportColor = settings.scopeMode ? TFT_MAGENTA : TFT_ORANGE;
  const int16_t navY = tft_.height() - 38;
  tft_.startWrite();
  if (force) {
    tft_.fillScreen(scopeBg);
  } else {
    tft_.fillRect(0, 0, tft_.width(), navY, scopeBg);
  }

  const int16_t cx = tft_.width() / 2;
  const int16_t cy = 190;
  const int16_t radius = min((int16_t)132, (int16_t)((tft_.width() - 42) / 2));

  tft_.drawCircle(cx, cy, radius + 5, settings.scopeMode ? 0x39E7 : gridDim);
  tft_.drawCircle(cx, cy, radius + 2, settings.scopeMode ? 0x18E3 : panel(settings));
  for (uint8_t ring = 1; ring <= 4; ++ring) {
    tft_.drawCircle(cx, cy, radius * ring / 4, gridDim);
  }
  tft_.drawCircle(cx, cy, radius, grid);

  for (uint16_t deg = 0; deg < 360; deg += 45) {
    const float a = deg * PI / 180.0f;
    const int16_t x = cx + roundf(sinf(a) * radius);
    const int16_t y = cy - roundf(cosf(a) * radius);
    tft_.drawLine(cx, cy, x, y, deg % 90 == 0 ? grid : gridDim);
  }

  tft_.setTextDatum(MC_DATUM);
  tft_.setTextFont(2);
  tft_.setTextColor(text, scopeBg);
  tft_.drawString("N", cx, cy - radius - 13);
  tft_.drawString("S", cx, cy + radius + 13);
  tft_.drawString("W", cx - radius - 14, cy);
  tft_.drawString("E", cx + radius + 14, cy);
  tft_.setTextFont(1);
  tft_.setTextColor(grid, scopeBg);
  tft_.drawString(String(settings.rangeKm) + " km", cx + radius - 28, cy + 10);
  tft_.setTextDatum(TL_DATUM);

  tft_.setTextFont(1);
  tft_.setTextColor(text, scopeBg);
  tft_.drawString(settings.scopeMode ? "SCOPE" : "RADAR", 6, 4);
  tft_.drawString("Tracked " + String(aircraft.size()), 58, 4);
  tft_.drawString("WiFi " + wifiStatus, 6, 16);
  tft_.drawString("GPS " + gpsStatus, 6, 28);
  tft_.setTextDatum(TR_DATUM);
  tft_.drawString(timeText, tft_.width() - 6, 4);
  tft_.drawString(batteryStatus, tft_.width() - 6, 16);
  tft_.setTextDatum(TL_DATUM);
  tft_.setTextColor(dimText, scopeBg);
  tft_.drawString(lastUpdateText, 6, 40);

  if (settings.airportOverlay) {
    for (const Airport &airport : airports) {
      if (airport.distanceKm > settings.rangeKm) continue;
      RadarPoint p = Radar::project(settings.homeLat, settings.homeLon, airport.lat, airport.lon, settings.rangeKm, cx,
                                    cy, radius);
      if (!p.visible) continue;
      tft_.fillCircle(p.x, p.y, 3, TFT_BLUE);
      tft_.drawCircle(p.x, p.y, 4, airportColor);
      if (airport.distanceKm <= settings.airportLabelKm) {
        tft_.setTextColor(airportColor, scopeBg);
        tft_.setTextFont(1);
        tft_.drawString(AirportManager::displayCode(airport), p.x + 5, p.y - 7);
      }
    }
  }
  if ((airports.empty() || !settings.airportOverlay) && airportStatus.length()) {
    tft_.setTextColor(dimText, scopeBg);
    tft_.setTextFont(1);
    tft_.drawString(airportStatus, 6, 52);
  }

  for (const Aircraft &a : aircraft) {
    for (uint8_t i = 1; i < a.trailCount; ++i) {
      RadarPoint p1 = Radar::project(settings.homeLat, settings.homeLon, a.trail[i - 1].lat, a.trail[i - 1].lon,
                                     settings.rangeKm, cx, cy, radius);
      RadarPoint p2 = Radar::project(settings.homeLat, settings.homeLon, a.trail[i].lat, a.trail[i].lon,
                                     settings.rangeKm, cx, cy, radius);
      if (p1.visible && p2.visible) tft_.drawLine(p1.x, p1.y, p2.x, p2.y, gridDim);
    }
  }

  for (const Aircraft &a : aircraft) {
    RadarPoint p = Radar::project(settings.homeLat, settings.homeLon, a.lat, a.lon, settings.rangeKm, cx, cy, radius);
    if (!p.visible) {
      if (p.distanceKm <= settings.rangeKm * Config::EDGE_MARKER_RANGE_MULTIPLIER) {
        drawEdgeMarker(tft_, cx, cy, radius, p.bearingDeg, p.distanceKm, scopeBg);
      }
      continue;
    }
    drawAircraftIcon(p.x, p.y, a.track, altitudeColor(a.altBaro), selectedHex_ == a.hex);
    const int16_t labelX = p.x < cx ? p.x + 10 : p.x - 58;
    const int16_t labelY = p.y - 16;
    tft_.setTextFont(1);
    tft_.setTextColor(altitudeColor(a.altBaro), scopeBg);
    tft_.drawString(safeFlight(a), labelX, labelY);
    tft_.drawString(aircraftSubtitle(a), labelX, labelY + 9);
    tft_.drawString(altText(a.altBaro), labelX, labelY + 18);
  }

  if (alertText.length()) {
    const uint16_t fill = 0xA000;
    tft_.fillRoundRect(46, 52, tft_.width() - 92, 20, 4, fill);
    tft_.setTextColor(TFT_WHITE, fill);
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextFont(1);
    tft_.drawString(alertText, tft_.width() / 2, 62);
    tft_.setTextDatum(TL_DATUM);
  }

  if (gpsCompass.length()) {
    const int16_t compassY = alertText.length() ? 76 : 52;
    const int16_t compassX = tft_.width() - 92;
    const uint16_t compassFill = settings.scopeMode ? 0x0841 : panel(settings);
    tft_.fillRoundRect(compassX, compassY, 86, 20, 4, compassFill);
    tft_.setTextColor(text, compassFill);
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextFont(1);
    tft_.drawString(gpsCompass, compassX + 43, compassY + 10);
    tft_.setTextDatum(TL_DATUM);
  }

  String nearestAirportText = airportStatus;
  if (settings.airportOverlay && !airports.empty()) {
    const Airport &nearestAirport = airports.front();
    nearestAirportText = "NEAREST " + AirportManager::displayCode(nearestAirport) + "  " +
                         String(nearestAirport.distanceKm, 1) + " km";
  }

  const int16_t statsY = cy + radius + 18;
  const uint16_t statFill = settings.scopeMode ? 0x0841 : panel(settings);
  tft_.fillRoundRect(10, statsY, tft_.width() - 20, 72, 6, statFill);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextFont(4);
  tft_.setTextColor(text, statFill);
  tft_.drawString(String(settings.rangeKm) + " km", tft_.width() / 2, statsY + 18);
  tft_.setTextFont(2);
  tft_.setTextColor(dimText, statFill);
  tft_.drawString("SCAN RADIUS   " + String(aircraft.size()) + " AIRCRAFT", tft_.width() / 2, statsY + 43);
  if (nearestAirportText.length()) {
    tft_.setTextFont(1);
    tft_.drawString(nearestAirportText, tft_.width() / 2, statsY + 62);
  }
  tft_.setTextDatum(TL_DATUM);

  if (force) drawBottomNav(settings, ScreenId::Radar);
  tft_.endWrite();
}

void DisplayUI::drawAirportList(const AppSettings &settings, const std::vector<Airport> &airports, bool force) {
  if (!dirty_ && !force) return;
  dirty_ = false;
  tft_.fillScreen(bg(settings));
  header(settings, "Airports", "<= " + String(settings.rangeKm) + " km");
  if (airports.empty()) {
    tft_.setTextColor(muted(settings), bg(settings));
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString("No airport database", tft_.width() / 2, tft_.height() / 2);
    tft_.setTextDatum(TL_DATUM);
  }
  uint8_t row = 0;
  uint8_t inRangeCount = 0;
  const uint8_t maxRows = 9;
  for (const Airport &airport : airports) {
    if (airport.distanceKm > settings.rangeKm) continue;
    inRangeCount++;
    if (row >= maxRows) continue;
    const int16_t y = 42 + row * 42;
    tft_.fillRoundRect(8, y, tft_.width() - 16, 36, 5, panel(settings));
    tft_.setTextColor(TFT_BLUE, panel(settings));
    tft_.drawString(AirportManager::displayCode(airport), 16, y + 3);
    tft_.setTextColor(fg(settings), panel(settings));
    String name = airport.name;
    if (name.length() > 20) name = name.substring(0, 20);
    tft_.drawString(name, 62, y + 3);
    tft_.setTextColor(muted(settings), panel(settings));
    tft_.drawString(String(airport.distanceKm, 1) + " km", 62, y + 19);
    tft_.drawString(String(airport.bearingDeg, 0) + " deg", 144, y + 19);
    row++;
  }
  if (!airports.empty() && inRangeCount == 0) {
    tft_.setTextColor(muted(settings), bg(settings));
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString("No airports inside " + String(settings.rangeKm) + " km", tft_.width() / 2, tft_.height() / 2);
    tft_.setTextDatum(TL_DATUM);
  } else if (inRangeCount > maxRows) {
    tft_.setTextColor(muted(settings), bg(settings));
    tft_.setTextDatum(TR_DATUM);
    tft_.drawString("showing " + String(maxRows) + "/" + String(inRangeCount), tft_.width() - 10, 424);
    tft_.setTextDatum(TL_DATUM);
  }
  drawBottomNav(settings, ScreenId::AirportList);
}

void DisplayUI::drawAirportDetail(const AppSettings &settings, const Airport *airport,
                                  const std::vector<Aircraft> &aircraft, bool force) {
  if (!dirty_ && !force) return;
  dirty_ = false;
  tft_.fillScreen(bg(settings));
  header(settings, "Airport Hub", airport ? AirportManager::displayCode(*airport) : "Missing");
  if (!airport) {
    tft_.setTextColor(muted(settings), bg(settings));
    tft_.drawString("Airport is no longer loaded.", 18, 72);
    drawBottomNav(settings, ScreenId::AirportDetail);
    return;
  }

  uint16_t arrivals = 0;
  uint16_t departures = 0;
  int nearestIndex[3] = {-1, -1, -1};
  float nearestDistance[3] = {99999.0f, 99999.0f, 99999.0f};

  for (size_t i = 0; i < aircraft.size(); ++i) {
    const Aircraft &a = aircraft[i];
    if (isnan(a.lat) || isnan(a.lon)) continue;
    const float d = Radar::distanceKm(airport->lat, airport->lon, a.lat, a.lon);
    if (d <= settings.rangeKm && !isnan(a.track)) {
      const float toAirport = Radar::bearingDeg(a.lat, a.lon, airport->lat, airport->lon);
      const float fromAirport = Radar::bearingDeg(airport->lat, airport->lon, a.lat, a.lon);
      if (angleDiff(a.track, toAirport) <= 55.0f) arrivals++;
      else if (angleDiff(a.track, fromAirport) <= 55.0f) departures++;
    }

    for (uint8_t slot = 0; slot < 3; ++slot) {
      if (d < nearestDistance[slot]) {
        for (int8_t move = 2; move > slot; --move) {
          nearestDistance[move] = nearestDistance[move - 1];
          nearestIndex[move] = nearestIndex[move - 1];
        }
        nearestDistance[slot] = d;
        nearestIndex[slot] = (int)i;
        break;
      }
    }
  }

  const String code = AirportManager::displayCode(*airport);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(accent(settings), bg(settings));
  tft_.setTextFont(4);
  tft_.drawString(code, tft_.width() / 2, 58);
  tft_.setTextFont(2);
  tft_.setTextColor(fg(settings), bg(settings));
  tft_.drawString("Arrivals: " + String(arrivals) + "   Departures: " + String(departures), tft_.width() / 2, 92);
  tft_.setTextDatum(TL_DATUM);

  button(24, 112, tft_.width() - 48, 30, "Center Radar Here", accent(settings), TFT_WHITE);

  tft_.setTextColor(fg(settings), bg(settings));
  tft_.setTextFont(2);
  tft_.drawString("Nearest", 16, 154);
  for (uint8_t row = 0; row < 3; ++row) {
    const int16_t y = 178 + row * 34;
    tft_.fillRoundRect(10, y, tft_.width() - 20, 28, 5, panel(settings));
    tft_.setTextColor(fg(settings), panel(settings));
    if (nearestIndex[row] >= 0) {
      const Aircraft &a = aircraft[nearestIndex[row]];
      tft_.drawString(safeFlight(a), 18, y + 6);
      tft_.setTextDatum(TR_DATUM);
      tft_.drawString(String(nearestDistance[row], 1) + " km", tft_.width() - 18, y + 6);
      tft_.setTextDatum(TL_DATUM);
    } else {
      tft_.setTextColor(muted(settings), panel(settings));
      tft_.drawString("---", 18, y + 6);
    }
  }

  String name = airport->name;
  if (name.length() > 28) name = name.substring(0, 28);
  String rows[] = {
      name,
      "Type: " + airport->type,
      "From home: " + String(airport->distanceKm, 1) + " km @ " + String(airport->bearingDeg, 0) + " deg",
      "Lat/Lon: " + String(airport->lat, 5) + ", " + String(airport->lon, 5),
  };
  for (uint8_t i = 0; i < 4; ++i) {
    const int16_t y = 292 + i * 32;
    tft_.fillRoundRect(10, y, tft_.width() - 20, 26, 5, panel(settings));
    tft_.setTextColor(fg(settings), panel(settings));
    tft_.drawString(rows[i], 18, y + 5);
  }
  drawBottomNav(settings, ScreenId::AirportDetail);
}

void DisplayUI::drawAircraftList(const AppSettings &settings, const std::vector<Aircraft> &aircraft,
                                 const String &lastUpdateText, bool force) {
  if (!dirty_ && !force) return;
  dirty_ = false;
  tft_.fillScreen(bg(settings));
  header(settings, "Nearest Aircraft", String(aircraft.size()) + " tracked");

  const int16_t summaryY = 40;
  tft_.fillRoundRect(8, summaryY, tft_.width() - 16, 58, 6, panel(settings));
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextFont(4);
  tft_.setTextColor(accent(settings), panel(settings));
  tft_.drawString(String(settings.rangeKm) + " km", tft_.width() / 2, summaryY + 18);
  tft_.setTextFont(2);
  tft_.setTextColor(fg(settings), panel(settings));
  tft_.drawString("SCAN RADIUS   " + String(aircraft.size()) + " AIRCRAFT", tft_.width() / 2, summaryY + 42);
  tft_.setTextDatum(TL_DATUM);
  tft_.setTextFont(1);
  tft_.setTextColor(muted(settings), bg(settings));
  tft_.drawString(lastUpdateText, 12, 104);

  auto order = Radar::nearestOrder(aircraft, settings.homeLat, settings.homeLon, 6);
  if (order.empty()) {
    tft_.setTextColor(muted(settings), bg(settings));
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString("No aircraft in range", tft_.width() / 2, tft_.height() / 2 + 24);
    tft_.setTextDatum(TL_DATUM);
  }
  const uint8_t maxRows = min((size_t)5, order.size());
  for (uint8_t row = 0; row < maxRows; ++row) {
    const Aircraft &a = aircraft[order[row]];
    const int16_t y = 118 + row * 48;
    tft_.fillRoundRect(8, y, tft_.width() - 16, 44, 5, panel(settings));
    tft_.fillCircle(22, y + 15, 6, altitudeColor(a.altBaro));
    tft_.setTextColor(fg(settings), panel(settings));
    tft_.drawString(safeFlight(a), 38, y + 4);
    tft_.setTextColor(muted(settings), panel(settings));
    tft_.drawString(aircraftSubtitle(a), 38, y + 18);
    tft_.setTextColor(fg(settings), panel(settings));
    const float d = Radar::distanceKm(settings.homeLat, settings.homeLon, a.lat, a.lon);
    tft_.drawString(String(d, 1) + " km", 150, y + 4);
    tft_.drawString(altText(a.altBaro), 218, y + 4);
    tft_.setTextColor(muted(settings), panel(settings));
    tft_.drawString(speedText(a.groundSpeed), 150, y + 22);
  }
  drawBottomNav(settings, ScreenId::AircraftList);
}

void DisplayUI::drawAircraftDetail(const AppSettings &settings, const Aircraft *aircraft, bool force) {
  if (!dirty_ && !force) return;
  dirty_ = false;
  tft_.fillScreen(bg(settings));
  header(settings, "Aircraft Detail", aircraft ? aircraft->hex : "Missing");

  if (!aircraft) {
    tft_.setTextColor(muted(settings), bg(settings));
    tft_.drawString("Aircraft is no longer in the latest feed.", 24, 80);
    drawBottomNav(settings, ScreenId::Detail);
    return;
  }

  const Aircraft &a = *aircraft;
  tft_.setTextColor(fg(settings), bg(settings));
  tft_.setTextFont(4);
  tft_.drawString(safeFlight(a), 18, 52);
  drawAircraftIcon(tft_.width() - 36, 68, a.track, altitudeColor(a.altBaro), false);

  tft_.setTextFont(2);
  const float dist = Radar::distanceKm(settings.homeLat, settings.homeLon, a.lat, a.lon);
  const float bearing = Radar::bearingDeg(settings.homeLat, settings.homeLon, a.lat, a.lon);
  String rows[] = {
      "Hex: " + a.hex,
      "Altitude: " + altText(a.altBaro),
      "Ground speed: " + speedText(a.groundSpeed),
      "Track: " + headingText(a.track),
      "Type: " + aircraftSubtitle(a),
      "Distance: " + String(dist, 1) + " km",
      "Bearing: " + String(bearing, 0) + " deg",
      "Seen: " + (isnan(a.seen) ? String("---") : String(a.seen, 1) + " s"),
      "Category: " + (a.category.length() ? a.category : String("---"))};

  for (uint8_t i = 0; i < 9; ++i) {
    tft_.fillRoundRect(18, 100 + i * 34, tft_.width() - 36, 28, 5, panel(settings));
    tft_.setTextColor(fg(settings), panel(settings));
    tft_.drawString(rows[i], 26, 106 + i * 34);
  }
  drawBottomNav(settings, ScreenId::Detail);
}

void DisplayUI::drawWiFiSetup(const AppSettings &settings, const String &savedSsid, const String &status) {
  dirty_ = false;
  tft_.fillScreen(bg(settings));
  header(settings, "WiFi: Setup Mode", status);

  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(fg(settings), bg(settings));
  tft_.setTextFont(4);
  tft_.drawString("Connect phone to WiFi:", tft_.width() / 2, 78);
  tft_.setTextColor(accent(settings), bg(settings));
  tft_.drawString(Config::WIFI_AP_NAME, tft_.width() / 2, 120);
  tft_.setTextFont(2);
  tft_.setTextColor(fg(settings), bg(settings));
  tft_.drawString("Then open: " + String(Config::WIFI_SETUP_URL), tft_.width() / 2, 162);
  if (savedSsid.length()) {
    tft_.setTextColor(muted(settings), bg(settings));
    tft_.drawString("Saved hotspot: " + savedSsid, tft_.width() / 2, 204);
  }
  tft_.setTextColor(muted(settings), bg(settings));
  tft_.drawString("Select your phone hotspot and save the password.", tft_.width() / 2, 250);
  tft_.setTextDatum(TL_DATUM);
}

void DisplayUI::drawTouchCalibration(const AppSettings &settings, uint8_t step, bool complete) {
  dirty_ = false;
  tft_.fillScreen(bg(settings));
  header(settings, "Touch Calibration", complete ? "Saved" : "Tap target");

  tft_.setTextDatum(MC_DATUM);
  tft_.setTextFont(2);
  tft_.setTextColor(fg(settings), bg(settings));
  if (complete) {
    tft_.drawString("Calibration saved", tft_.width() / 2, 120);
    tft_.setTextColor(muted(settings), bg(settings));
    tft_.drawString("Returning to setup...", tft_.width() / 2, 160);
    tft_.setTextDatum(TL_DATUM);
    return;
  }

  const int16_t margin = 28;
  const int16_t right = (int16_t)(tft_.width() - margin);
  const int16_t bottom = (int16_t)(tft_.height() - margin);
  const int16_t xs[] = {margin, right, right, margin};
  const int16_t ys[] = {(int16_t)(margin + 32), (int16_t)(margin + 32), bottom, bottom};
  const uint8_t index = step < 4 ? step : 3;
  tft_.drawString("Tap and release each crosshair", tft_.width() / 2, tft_.height() / 2 - 12);
  tft_.setTextColor(muted(settings), bg(settings));
  tft_.drawString("Point " + String(index + 1) + " of 4", tft_.width() / 2, tft_.height() / 2 + 18);

  tft_.drawCircle(xs[index], ys[index], 16, accent(settings));
  tft_.drawLine(xs[index] - 22, ys[index], xs[index] + 22, ys[index], accent(settings));
  tft_.drawLine(xs[index], ys[index] - 22, xs[index], ys[index] + 22, accent(settings));
  tft_.setTextDatum(TL_DATUM);
}

void DisplayUI::drawSettings(const AppSettings &settings, const String &wifiStatus, bool force) {
  if (!dirty_ && !force) return;
  dirty_ = false;
  const uint16_t settingsBg = settings.nightMode ? 0x0841 : bg(settings);
  const uint16_t settingsPanel = settings.nightMode ? 0x2104 : panel(settings);
  const uint16_t settingsText = settings.nightMode ? TFT_WHITE : fg(settings);
  const uint16_t settingsMuted = settings.nightMode ? 0xBDF7 : muted(settings);
  tft_.fillScreen(settingsBg);
  String right = "WiFi " + wifiStatus;
  if (right.length() > 20) right = right.substring(0, 20);
  header(settings, "Settings", right);
  tft_.setTextColor(settingsText, settingsBg);
  tft_.setTextFont(2);
  tft_.drawString("Latitude", 16, 48);
  tft_.drawString(String(settings.homeLat, 5), 110, 48);
  button(226, 42, 36, 28, "-", settingsPanel, settingsText);
  button(270, 42, 36, 28, "+", accent(settings), TFT_WHITE);

  tft_.drawString("Longitude", 16, 88);
  tft_.drawString(String(settings.homeLon, 5), 110, 88);
  button(226, 82, 36, 28, "-", settingsPanel, settingsText);
  button(270, 82, 36, 28, "+", accent(settings), TFT_WHITE);

  tft_.drawString("Range", 16, 128);
  tft_.drawString(String(settings.rangeKm) + " km", 110, 128);
  button(206, 122, 100, 28, "Change", accent(settings), TFT_WHITE);

  tft_.drawString("Theme", 16, 168);
  button(110, 162, 86, 28, settings.nightMode ? "Night" : "Day", accent(settings), TFT_WHITE);
  button(206, 162, 100, 28, settings.scopeMode ? "Scope" : "Radar", accent(settings), TFT_WHITE);

  tft_.drawString("GPS Log", 16, 208);
  button(110, 202, 86, 28, settings.gpsLogging ? "Log On" : "Log Off",
         settings.gpsLogging ? TFT_GREEN : settingsPanel, settings.gpsLogging ? TFT_BLACK : settingsText);
  button(206, 202, 100, 28, "Cal Touch", accent(settings), TFT_WHITE);

  tft_.drawString("Airports", 16, 248);
  button(110, 242, 86, 28, settings.airportOverlay ? "Apt On" : "Apt Off",
         settings.airportOverlay ? TFT_GREEN : settingsPanel, settings.airportOverlay ? TFT_BLACK : settingsText);
  button(206, 242, 100, 28, String(settings.airportLabelKm) + "km", accent(settings), TFT_WHITE);

  tft_.setTextColor(settingsText, settingsBg);
  tft_.setTextFont(2);
  tft_.drawString("Refresh", 16, 288);
  button(206, 282, 100, 28, String(settings.adsbRefreshSec) + " sec", accent(settings), TFT_WHITE);

  button(10, 334, 72, 30, "Reset", TFT_RED, TFT_WHITE);
  button(88, 334, 68, 30, "WiFi", settingsPanel, settingsText);
  button(162, 334, 74, 30, "Reboot", settingsPanel, settingsText);
  button(242, 334, 68, 30, "Save", TFT_GREEN, TFT_BLACK);
  drawBottomNav(settings, ScreenId::Settings);
}

void DisplayUI::drawWiFiSettings(const AppSettings &settings, const WiFiManagerExt &wifi, bool force) {
  if (!dirty_ && !force) return;
  dirty_ = false;
  tft_.fillScreen(bg(settings));
  header(settings, "WiFi Settings", wifi.statusText());

  const auto &networks = wifi.networks();
  if (networks.empty()) {
    tft_.setTextColor(muted(settings), bg(settings));
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString("No saved networks", tft_.width() / 2, 96);
    tft_.setTextDatum(TL_DATUM);
  }

  const uint8_t rows = min((size_t)4, networks.size());
  for (uint8_t row = 0; row < rows; ++row) {
    const WifiCredential &cred = networks[row];
    const int16_t y = 42 + row * 40;
    const bool selected = wifi.selectedIndex() == row;
    const uint16_t fill = selected ? accent(settings) : panel(settings);
    const uint16_t primary = selected ? TFT_WHITE : fg(settings);
    tft_.fillRoundRect(8, y, tft_.width() - 16, 34, 5, fill);
    tft_.setTextColor(primary, fill);
    tft_.setTextFont(2);
    String label = String(row + 1) + ". " + cred.ssid;
    if (label.length() > 22) label = label.substring(0, 22);
    tft_.drawString(label, 16, y + 3);
    tft_.setTextColor(selected ? TFT_WHITE : muted(settings), fill);
    String meta = cred.enabled ? "enabled" : "disabled";
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == cred.ssid) meta += "  connected";
    tft_.drawString(meta, 34, y + 18);
  }

  tft_.setTextColor(fg(settings), bg(settings));
  tft_.setTextFont(2);
  tft_.drawString("SSID: " + (wifi.currentSsid().length() ? wifi.currentSsid() : String("---")), 12, 210);
  tft_.drawString("IP: " + wifi.ipText(), 12, 232);
  tft_.drawString("RSSI: " + String(wifi.rssi()) + " dBm", 12, 254);

  button(10, 282, 70, 28, "Add", accent(settings), TFT_WHITE);
  button(86, 282, 70, 28, "Delete", TFT_RED, TFT_WHITE);
  button(162, 282, 70, 28, "On/Off", panel(settings), fg(settings));
  button(238, 282, 34, 28, "Up", panel(settings), fg(settings));
  button(276, 282, 34, 28, "Dn", panel(settings), fg(settings));

  button(10, 322, 90, 28, "Export", panel(settings), fg(settings));
  button(108, 322, 90, 28, "Import", panel(settings), fg(settings));
  button(206, 322, 104, 28, "Reset WiFi", TFT_RED, TFT_WHITE);
  drawBottomNav(settings, ScreenId::WiFiSettings);
}

const Aircraft *DisplayUI::findAircraft(const std::vector<Aircraft> &aircraft, const String &hex) const {
  for (const Aircraft &a : aircraft) {
    if (a.hex == hex) return &a;
  }
  return nullptr;
}

String DisplayUI::hitAircraft(int16_t x, int16_t y, const AppSettings &settings,
                              const std::vector<Aircraft> &aircraft) {
  const int16_t cx = tft_.width() / 2;
  const int16_t cy = 190;
  const int16_t radius = min((int16_t)132, (int16_t)((tft_.width() - 42) / 2));
  for (const Aircraft &a : aircraft) {
    RadarPoint p = Radar::project(settings.homeLat, settings.homeLon, a.lat, a.lon, settings.rangeKm, cx, cy, radius);
    if (!p.visible) continue;
    if (abs(p.x - x) <= 18 && abs(p.y - y) <= 18) return a.hex;
  }
  return "";
}

String DisplayUI::hitAirportRow(int16_t x, int16_t y, const AppSettings &settings,
                                const std::vector<Airport> &airports) {
  uint8_t row = 0;
  for (const Airport &airport : airports) {
    if (airport.distanceKm > settings.rangeKm) continue;
    if (row >= 9) break;
    if (inRect(x, y, 8, 42 + row * 42, tft_.width() - 16, 36)) return AirportManager::displayCode(airport);
    row++;
  }
  return "";
}

int DisplayUI::hitWifiRow(int16_t x, int16_t y, const WiFiManagerExt &wifi) {
  const uint8_t rows = min((size_t)4, wifi.networks().size());
  for (uint8_t row = 0; row < rows; ++row) {
    if (inRect(x, y, 8, 42 + row * 40, tft_.width() - 16, 34)) return row;
  }
  return -1;
}

UIEvent DisplayUI::handleTouch(const TouchPoint &point, ScreenId screen, const AppSettings &settings,
                               const std::vector<Aircraft> &aircraft, const std::vector<Airport> &airports) {
  static WiFiManagerExt emptyWifi;
  return handleTouch(point, screen, settings, aircraft, airports, emptyWifi);
}

UIEvent DisplayUI::handleTouch(const TouchPoint &point, ScreenId screen, const AppSettings &settings,
                               const std::vector<Aircraft> &aircraft, const std::vector<Airport> &airports,
                               const WiFiManagerExt &wifi) {
  UIEvent event;
  if (!point.touched) return event;

  const int16_t navY = tft_.height() - 38;
  if (point.y >= navY - 10) {
    const int16_t w = tft_.width();
    if (point.x < w * 22 / 100) event.action = UIAction::ShowRadar;
    else if (point.x < w * 42 / 100) event.action = UIAction::ShowList;
    else if (point.x < w * 62 / 100) event.action = UIAction::ShowAirportList;
    else if (point.x < w * 85 / 100) event.action = UIAction::RangeNext;
    else event.action = UIAction::ShowSettings;
  }

  if (event.action != UIAction::None) {
    Serial.printf("[ui] bottom nav x=%d y=%d action=%s\n", point.x, point.y, uiActionName(event.action));
    dirty_ = true;
    return event;
  }

  if (screen == ScreenId::Radar) {
    event.aircraftHex = hitAircraft(point.x, point.y, settings, aircraft);
    if (event.aircraftHex.length()) {
      selectedHex_ = event.aircraftHex;
      event.action = UIAction::ShowDetail;
      dirty_ = true;
    }
  } else if (screen == ScreenId::AircraftList) {
    auto order = Radar::nearestOrder(aircraft, settings.homeLat, settings.homeLon, 6);
    const uint8_t maxRows = min((size_t)5, order.size());
    for (uint8_t row = 0; row < maxRows; ++row) {
      if (inRect(point.x, point.y, 8, 118 + row * 48, tft_.width() - 16, 44)) {
        event.aircraftHex = aircraft[order[row]].hex;
        selectedHex_ = event.aircraftHex;
        event.action = UIAction::ShowDetail;
        dirty_ = true;
        break;
      }
    }
  } else if (screen == ScreenId::AirportList) {
    event.airportCode = hitAirportRow(point.x, point.y, settings, airports);
    if (event.airportCode.length()) {
      selectedAirportCode_ = event.airportCode;
      event.action = UIAction::ShowAirportDetail;
      dirty_ = true;
    }
  } else if (screen == ScreenId::AirportDetail) {
    if (inRect(point.x, point.y, 24, 112, tft_.width() - 48, 30) ||
        inRect(point.x, point.y, 0, 34, tft_.width(), 70)) {
      event.airportCode = selectedAirportCode_;
      event.action = UIAction::CenterOnAirport;
      dirty_ = true;
    }
  } else if (screen == ScreenId::WiFiSettings) {
    int row = hitWifiRow(point.x, point.y, wifi);
    if (row >= 0) {
      event.wifiIndex = row;
      event.action = UIAction::SelectWifiNetwork;
    } else if (inRect(point.x, point.y, 10, 282, 70, 28)) {
      event.action = UIAction::WifiAddPortal;
    } else if (inRect(point.x, point.y, 86, 282, 70, 28)) {
      event.action = UIAction::WifiDelete;
    } else if (inRect(point.x, point.y, 162, 282, 70, 28)) {
      event.action = UIAction::WifiToggle;
    } else if (inRect(point.x, point.y, 238, 282, 34, 28)) {
      event.action = UIAction::WifiMoveUp;
    } else if (inRect(point.x, point.y, 276, 282, 34, 28)) {
      event.action = UIAction::WifiMoveDown;
    } else if (inRect(point.x, point.y, 10, 322, 90, 28)) {
      event.action = UIAction::WifiExport;
    } else if (inRect(point.x, point.y, 108, 322, 90, 28)) {
      event.action = UIAction::WifiImport;
    } else if (inRect(point.x, point.y, 206, 322, 104, 28)) {
      event.action = UIAction::ResetWiFiHold;
    }
    if (event.action != UIAction::None) dirty_ = true;
  } else if (screen == ScreenId::Settings) {
    if (inRect(point.x, point.y, 226, 42, 36, 28)) event.action = UIAction::LatMinus;
    else if (inRect(point.x, point.y, 270, 42, 36, 28)) event.action = UIAction::LatPlus;
    else if (inRect(point.x, point.y, 226, 82, 36, 28)) event.action = UIAction::LonMinus;
    else if (inRect(point.x, point.y, 270, 82, 36, 28)) event.action = UIAction::LonPlus;
    else if (inRect(point.x, point.y, 206, 122, 100, 28)) event.action = UIAction::RangeNext;
    else if (inRect(point.x, point.y, 110, 162, 86, 28)) event.action = UIAction::ToggleTheme;
    else if (inRect(point.x, point.y, 206, 162, 100, 28)) event.action = UIAction::ToggleRadarMode;
    else if (inRect(point.x, point.y, 110, 202, 86, 28)) event.action = UIAction::ToggleGpsLogging;
    else if (inRect(point.x, point.y, 206, 202, 100, 28)) event.action = UIAction::StartTouchCalibration;
    else if (inRect(point.x, point.y, 110, 242, 86, 28)) event.action = UIAction::ToggleAirportOverlay;
    else if (inRect(point.x, point.y, 206, 242, 100, 28)) event.action = UIAction::AirportLabelNext;
    else if (inRect(point.x, point.y, 206, 282, 100, 28)) event.action = UIAction::RefreshRateNext;
    else if (inRect(point.x, point.y, 10, 334, 72, 30)) event.action = UIAction::ResetWiFiHold;
    else if (inRect(point.x, point.y, 88, 334, 68, 30)) event.action = UIAction::ShowWiFiSettings;
    else if (inRect(point.x, point.y, 162, 334, 74, 30)) event.action = UIAction::RebootDevice;
    else if (inRect(point.x, point.y, 242, 334, 68, 30)) event.action = UIAction::SaveSettings;
    if (event.action != UIAction::None) dirty_ = true;
  }
  return event;
}
