#pragma once

#include <Arduino.h>
#include <vector>

#include "config.h"

struct TrailPoint {
  float lat = 0;
  float lon = 0;
  uint32_t seenAtMs = 0;
};

struct Aircraft {
  String flight;
  String hex;
  float lat = NAN;
  float lon = NAN;
  int32_t altBaro = INT32_MIN;
  float groundSpeed = NAN;
  float track = NAN;
  float seen = NAN;
  String category;
  uint32_t updatedAtMs = 0;
  TrailPoint trail[Config::TRAIL_POINTS];
  uint8_t trailCount = 0;
};

class ADSBClient {
 public:
  bool fetch(float homeLat, float homeLon, uint16_t rangeKm, std::vector<Aircraft> &aircraft);
  const String &lastError() const { return lastError_; }
  uint32_t lastHttpCode() const { return lastHttpCode_; }

 private:
  String lastError_;
  uint32_t lastHttpCode_ = 0;

  void mergeAircraft(std::vector<Aircraft> &aircraft, Aircraft &incoming);
  static void pushTrail(Aircraft &aircraft, float lat, float lon);
};
