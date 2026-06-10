#pragma once

#include <Arduino.h>
#include <vector>

#include "adsb.h"

struct RadarPoint {
  int16_t x = 0;
  int16_t y = 0;
  float distanceKm = 0;
  float bearingDeg = 0;
  bool visible = false;
};

namespace Radar {
float distanceKm(float lat1, float lon1, float lat2, float lon2);
float bearingDeg(float lat1, float lon1, float lat2, float lon2);
RadarPoint project(float homeLat, float homeLon, float lat, float lon, uint16_t rangeKm,
                   int16_t centerX, int16_t centerY, int16_t radiusPx);
std::vector<uint16_t> nearestOrder(const std::vector<Aircraft> &aircraft, float homeLat, float homeLon,
                                   size_t maxCount);
}  // namespace Radar
