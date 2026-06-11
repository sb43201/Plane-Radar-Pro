#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#include "settings.h"

struct TouchPoint {
  int16_t x = 0;
  int16_t y = 0;
  bool touched = false;
};

struct RawTouchPoint {
  int16_t x = 0;
  int16_t y = 0;
  int16_t z = 0;
  bool touched = false;
};

class TouchInput {
 public:
  TouchInput();
  void begin(const AppSettings &settings);
  TouchPoint read(const AppSettings &settings);
  RawTouchPoint readRaw();
  bool isTouched();

 private:
  SPIClass touchSpi_;
  XPT2046_Touchscreen touch_;
  uint32_t lastTouchMs_ = 0;
};
