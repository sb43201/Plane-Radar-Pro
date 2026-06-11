#include "display.h"

#include <math.h>

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

struct AirportOverlay {
  const char *id;
  float lat;
  float lon;
};

constexpr AirportOverlay AIRPORTS[] = {
    {"IND", 39.7173f, -86.2944f},
    {"HUF", 39.4515f, -87.3076f},
    {"MQJ", 39.8435f, -85.8971f},
};
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
  tft_.fillRect(0, y, tft_.width(), 38, panel(settings));
  button(6, y + 5, 82, 28, "Radar", active == ScreenId::Radar ? accent(settings) : muted(settings), TFT_WHITE);
  button(96, y + 5, 82, 28, "List", active == ScreenId::AircraftList ? accent(settings) : muted(settings),
         TFT_WHITE);
  button(186, y + 5, 92, 28, "Range", panel(settings), fg(settings));
  button(286, y + 5, 92, 28, settings.nightMode ? "Day" : "Night", panel(settings), fg(settings));
  button(386, y + 5, 88, 28, "Setup", active == ScreenId::Settings ? accent(settings) : muted(settings),
         TFT_WHITE);
}

void DisplayUI::drawRadar(const AppSettings &settings, const std::vector<Aircraft> &aircraft,
                          const String &wifiStatus, const String &gpsStatus, const String &batteryStatus,
                          const String &timeText, const String &lastUpdateText, const String &alertText, bool force) {
  if (!dirty_ && !force) return;
  dirty_ = false;
  tft_.fillScreen(bg(settings));
  header(settings, Config::APP_NAME, timeText + "  " + batteryStatus);

  const int16_t cx = tft_.width() / 2;
  const int16_t cy = 170;
  const int16_t radius = 118;
  uint16_t grid = settings.nightMode ? 0x03EF : 0x9CF3;
  tft_.drawCircle(cx, cy, radius, grid);
  tft_.drawCircle(cx, cy, radius * 2 / 3, grid);
  tft_.drawCircle(cx, cy, radius / 3, grid);
  tft_.drawLine(cx - radius, cy, cx + radius, cy, grid);
  tft_.drawLine(cx, cy - radius, cx, cy + radius, grid);

  tft_.setTextColor(muted(settings), bg(settings));
  tft_.setTextFont(2);
  tft_.setTextDatum(MC_DATUM);
  tft_.drawString("N", cx, cy - radius - 13);
  tft_.drawString("S", cx, cy + radius + 12);
  tft_.drawString("W", cx - radius - 12, cy);
  tft_.drawString("E", cx + radius + 12, cy);
  tft_.setTextDatum(TL_DATUM);

  for (const AirportOverlay &airport : AIRPORTS) {
    RadarPoint p = Radar::project(settings.homeLat, settings.homeLon, airport.lat, airport.lon, settings.rangeKm, cx,
                                  cy, radius);
    if (!p.visible) continue;
    tft_.fillCircle(p.x, p.y, 3, settings.nightMode ? TFT_MAGENTA : TFT_PURPLE);
    tft_.setTextColor(settings.nightMode ? TFT_MAGENTA : TFT_PURPLE, bg(settings));
    tft_.drawString(airport.id, p.x + 5, p.y - 7);
  }

  for (const Aircraft &a : aircraft) {
    for (uint8_t i = 1; i < a.trailCount; ++i) {
      RadarPoint p1 = Radar::project(settings.homeLat, settings.homeLon, a.trail[i - 1].lat, a.trail[i - 1].lon,
                                     settings.rangeKm, cx, cy, radius);
      RadarPoint p2 = Radar::project(settings.homeLat, settings.homeLon, a.trail[i].lat, a.trail[i].lon,
                                     settings.rangeKm, cx, cy, radius);
      if (p1.visible && p2.visible) tft_.drawLine(p1.x, p1.y, p2.x, p2.y, muted(settings));
    }
  }

  for (const Aircraft &a : aircraft) {
    RadarPoint p = Radar::project(settings.homeLat, settings.homeLon, a.lat, a.lon, settings.rangeKm, cx, cy, radius);
    if (!p.visible) continue;
    drawAircraftIcon(p.x, p.y, a.track, altitudeColor(a.altBaro), selectedHex_ == a.hex);
    if (a.type.length()) {
      tft_.setTextColor(muted(settings), bg(settings));
      tft_.drawString(a.type, p.x + 9, p.y + 7);
    }
  }

  tft_.fillRect(0, 32, tft_.width(), 28, bg(settings));
  tft_.setTextColor(fg(settings), bg(settings));
  tft_.setTextFont(2);
  tft_.drawString("AC: " + String(aircraft.size()), 8, 40);
  tft_.drawString("WiFi: " + wifiStatus, 78, 40);
  tft_.drawString("GPS: " + gpsStatus, 220, 40);
  tft_.drawString(String(settings.rangeKm) + " km", 340, 40);
  tft_.setTextDatum(TR_DATUM);
  tft_.drawString(lastUpdateText, tft_.width() - 8, 40);
  tft_.setTextDatum(TL_DATUM);

  if (alertText.length()) {
    const uint16_t fill = settings.nightMode ? 0xA000 : TFT_RED;
    tft_.fillRoundRect(10, 62, tft_.width() - 20, 24, 5, fill);
    tft_.setTextColor(TFT_WHITE, fill);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString(alertText, tft_.width() / 2, 74);
    tft_.setTextDatum(TL_DATUM);
  }

  drawBottomNav(settings, ScreenId::Radar);
}

void DisplayUI::drawAircraftList(const AppSettings &settings, const std::vector<Aircraft> &aircraft, bool force) {
  if (!dirty_ && !force) return;
  dirty_ = false;
  tft_.fillScreen(bg(settings));
  header(settings, "Nearest Aircraft", String(aircraft.size()) + " tracked");
  auto order = Radar::nearestOrder(aircraft, settings.homeLat, settings.homeLon, 6);
  if (order.empty()) {
    tft_.setTextColor(muted(settings), bg(settings));
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString("No aircraft in range", tft_.width() / 2, tft_.height() / 2);
    tft_.setTextDatum(TL_DATUM);
  }
  for (uint8_t row = 0; row < order.size(); ++row) {
    const Aircraft &a = aircraft[order[row]];
    const int16_t y = 42 + row * 38;
    tft_.fillRoundRect(8, y, tft_.width() - 16, 32, 5, panel(settings));
    tft_.fillCircle(22, y + 16, 6, altitudeColor(a.altBaro));
    tft_.setTextColor(fg(settings), panel(settings));
    tft_.drawString(safeFlight(a), 38, y + 4);
    tft_.setTextColor(muted(settings), panel(settings));
    tft_.drawString(aircraftSubtitle(a), 38, y + 18);
    tft_.setTextColor(fg(settings), panel(settings));
    const float d = Radar::distanceKm(settings.homeLat, settings.homeLon, a.lat, a.lon);
    tft_.drawString(String(d, 1) + " km", 150, y + 4);
    tft_.drawString(altText(a.altBaro), 230, y + 4);
    tft_.drawString(speedText(a.groundSpeed), 340, y + 4);
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
  drawAircraftIcon(430, 68, a.track, altitudeColor(a.altBaro), false);

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
    tft_.fillRoundRect(18 + (i % 2) * 225, 100 + (i / 2) * 36, 210, 28, 5, panel(settings));
    tft_.setTextColor(fg(settings), panel(settings));
    tft_.drawString(rows[i], 26 + (i % 2) * 225, 106 + (i / 2) * 36);
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

void DisplayUI::drawSettings(const AppSettings &settings, bool force) {
  if (!dirty_ && !force) return;
  dirty_ = false;
  tft_.fillScreen(bg(settings));
  header(settings, "Home Settings", settings.nightMode ? "Night" : "Day");
  tft_.setTextColor(fg(settings), bg(settings));
  tft_.setTextFont(2);
  tft_.drawString("Latitude", 34, 58);
  tft_.drawString(String(settings.homeLat, 5), 170, 58);
  button(328, 50, 56, 30, "-", panel(settings), fg(settings));
  button(394, 50, 56, 30, "+", accent(settings), TFT_WHITE);

  tft_.drawString("Longitude", 34, 106);
  tft_.drawString(String(settings.homeLon, 5), 170, 106);
  button(328, 98, 56, 30, "-", panel(settings), fg(settings));
  button(394, 98, 56, 30, "+", accent(settings), TFT_WHITE);

  tft_.drawString("Radar range", 34, 154);
  tft_.drawString(String(settings.rangeKm) + " km", 170, 154);
  button(328, 146, 122, 30, "Change", accent(settings), TFT_WHITE);

  tft_.drawString("Theme", 34, 202);
  button(170, 194, 122, 30, settings.nightMode ? "Night" : "Day", accent(settings), TFT_WHITE);
  button(328, 194, 122, 30, "Cal Touch", accent(settings), TFT_WHITE);
  button(34, 242, 122, 30, "Reset WiFi", TFT_RED, TFT_WHITE);
  tft_.setTextColor(muted(settings), bg(settings));
  tft_.drawString("Hold 3 sec", 170, 250);
  button(328, 242, 122, 30, "Save", TFT_GREEN, TFT_BLACK);
  drawBottomNav(settings, ScreenId::Settings);
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
  const int16_t cy = 170;
  const int16_t radius = 118;
  for (const Aircraft &a : aircraft) {
    RadarPoint p = Radar::project(settings.homeLat, settings.homeLon, a.lat, a.lon, settings.rangeKm, cx, cy, radius);
    if (!p.visible) continue;
    if (abs(p.x - x) <= 18 && abs(p.y - y) <= 18) return a.hex;
  }
  return "";
}

UIEvent DisplayUI::handleTouch(const TouchPoint &point, ScreenId screen, const AppSettings &settings,
                               const std::vector<Aircraft> &aircraft) {
  UIEvent event;
  if (!point.touched) return event;

  const int16_t navY = tft_.height() - 38;
  if (inRect(point.x, point.y, 6, navY + 5, 82, 28)) event.action = UIAction::ShowRadar;
  else if (inRect(point.x, point.y, 96, navY + 5, 82, 28)) event.action = UIAction::ShowList;
  else if (inRect(point.x, point.y, 186, navY + 5, 92, 28)) event.action = UIAction::RangeNext;
  else if (inRect(point.x, point.y, 286, navY + 5, 92, 28)) event.action = UIAction::ToggleTheme;
  else if (inRect(point.x, point.y, 386, navY + 5, 88, 28)) event.action = UIAction::ShowSettings;

  if (event.action != UIAction::None) {
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
    for (uint8_t row = 0; row < order.size(); ++row) {
      if (inRect(point.x, point.y, 8, 42 + row * 38, tft_.width() - 16, 32)) {
        event.aircraftHex = aircraft[order[row]].hex;
        selectedHex_ = event.aircraftHex;
        event.action = UIAction::ShowDetail;
        dirty_ = true;
        break;
      }
    }
  } else if (screen == ScreenId::Settings) {
    if (inRect(point.x, point.y, 328, 50, 56, 30)) event.action = UIAction::LatMinus;
    else if (inRect(point.x, point.y, 394, 50, 56, 30)) event.action = UIAction::LatPlus;
    else if (inRect(point.x, point.y, 328, 98, 56, 30)) event.action = UIAction::LonMinus;
    else if (inRect(point.x, point.y, 394, 98, 56, 30)) event.action = UIAction::LonPlus;
    else if (inRect(point.x, point.y, 328, 146, 122, 30)) event.action = UIAction::RangeNext;
    else if (inRect(point.x, point.y, 170, 194, 122, 30)) event.action = UIAction::ToggleTheme;
    else if (inRect(point.x, point.y, 328, 194, 122, 30)) event.action = UIAction::StartTouchCalibration;
    else if (inRect(point.x, point.y, 34, 242, 122, 30)) event.action = UIAction::ResetWiFiHold;
    else if (inRect(point.x, point.y, 328, 242, 122, 30)) event.action = UIAction::SaveSettings;
    if (event.action != UIAction::None) dirty_ = true;
  }
  return event;
}
