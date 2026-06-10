#include "radar.h"

#include <algorithm>
#include <math.h>

namespace {
constexpr float EARTH_RADIUS_KM = 6371.0f;

float degToRad(float deg) {
  return deg * PI / 180.0f;
}

float radToDeg(float rad) {
  return rad * 180.0f / PI;
}
}  // namespace

float Radar::distanceKm(float lat1, float lon1, float lat2, float lon2) {
  const float dLat = degToRad(lat2 - lat1);
  const float dLon = degToRad(lon2 - lon1);
  const float a = sinf(dLat / 2) * sinf(dLat / 2) +
                  cosf(degToRad(lat1)) * cosf(degToRad(lat2)) * sinf(dLon / 2) * sinf(dLon / 2);
  const float c = 2 * atan2f(sqrtf(a), sqrtf(1 - a));
  return EARTH_RADIUS_KM * c;
}

float Radar::bearingDeg(float lat1, float lon1, float lat2, float lon2) {
  const float p1 = degToRad(lat1);
  const float p2 = degToRad(lat2);
  const float dLon = degToRad(lon2 - lon1);
  const float y = sinf(dLon) * cosf(p2);
  const float x = cosf(p1) * sinf(p2) - sinf(p1) * cosf(p2) * cosf(dLon);
  float bearing = fmodf(radToDeg(atan2f(y, x)) + 360.0f, 360.0f);
  return bearing;
}

RadarPoint Radar::project(float homeLat, float homeLon, float lat, float lon, uint16_t rangeKm,
                          int16_t centerX, int16_t centerY, int16_t radiusPx) {
  RadarPoint p;
  if (isnan(lat) || isnan(lon) || rangeKm == 0) return p;
  p.distanceKm = distanceKm(homeLat, homeLon, lat, lon);
  p.bearingDeg = bearingDeg(homeLat, homeLon, lat, lon);
  p.visible = p.distanceKm <= rangeKm;
  const float radial = (p.distanceKm / rangeKm) * radiusPx;
  const float angle = degToRad(p.bearingDeg);
  p.x = centerX + (int16_t)roundf(sinf(angle) * radial);
  p.y = centerY - (int16_t)roundf(cosf(angle) * radial);
  return p;
}

std::vector<uint16_t> Radar::nearestOrder(const std::vector<Aircraft> &aircraft, float homeLat, float homeLon,
                                          size_t maxCount) {
  std::vector<uint16_t> order;
  order.reserve(min(maxCount, aircraft.size()));
  for (uint16_t i = 0; i < aircraft.size(); ++i) {
    if (!isnan(aircraft[i].lat) && !isnan(aircraft[i].lon)) order.push_back(i);
  }
  std::sort(order.begin(), order.end(), [&](uint16_t a, uint16_t b) {
    return distanceKm(homeLat, homeLon, aircraft[a].lat, aircraft[a].lon) <
           distanceKm(homeLat, homeLon, aircraft[b].lat, aircraft[b].lon);
  });
  if (order.size() > maxCount) order.resize(maxCount);
  return order;
}
