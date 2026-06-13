#pragma once

#include <Arduino.h>
#include <TinyGPSPlus.h>

class GPSModule {
 public:
  void begin();
  void update();
  bool hasFix() const;
  bool hasData() const;
  float latitude();
  float longitude();
  bool hasCourse() const;
  float courseDeg();
  float speedKmph();
  uint32_t satellites();
  String statusText();

 private:
  TinyGPSPlus gps_;
  uint32_t lastDataMs_ = 0;
  uint32_t lastDebugMs_ = 0;
  uint32_t lastCharsProcessed_ = 0;
};
