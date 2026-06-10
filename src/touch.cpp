#include "touch.h"

#include "config.h"

TouchInput::TouchInput()
    : touchSpi_(HSPI),
      touch_(Config::TOUCH_CS_PIN) {}

void TouchInput::begin(const AppSettings &) {
  touchSpi_.begin(Config::TOUCH_SCK, Config::TOUCH_MISO, Config::TOUCH_MOSI, Config::TOUCH_CS_PIN);
  touch_.begin(touchSpi_);
  touch_.setRotation(1);
  Serial.println("[touch] XPT2046 initialized");
}

TouchPoint TouchInput::read(const AppSettings &settings) {
  TouchPoint point;
  if (!touch_.touched()) return point;

  const uint32_t now = millis();
  if (now - lastTouchMs_ < 160) return point;
  lastTouchMs_ = now;

  TS_Point raw = touch_.getPoint();
  int16_t x = map(raw.x, settings.touchMinX, settings.touchMaxX, 0, Config::SCREEN_W - 1);
  int16_t y = map(raw.y, settings.touchMinY, settings.touchMaxY, 0, Config::SCREEN_H - 1);
  point.x = constrain(x, 0, Config::SCREEN_W - 1);
  point.y = constrain(y, 0, Config::SCREEN_H - 1);
  point.touched = true;
  Serial.printf("[touch] raw=(%d,%d,%d) mapped=(%d,%d)\n", raw.x, raw.y, raw.z, point.x, point.y);
  return point;
}
