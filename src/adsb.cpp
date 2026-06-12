#include "adsb.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <algorithm>

namespace {
String cleanFlight(const char *flight) {
  if (!flight) return "";
  String value(flight);
  value.trim();
  return value;
}

bool hasPosition(const Aircraft &a) {
  return !isnan(a.lat) && !isnan(a.lon);
}

void addAircraftFilter(JsonVariant variant) {
  variant["hex"] = true;
  variant["flight"] = true;
  variant["t"] = true;
  variant["type"] = true;
  variant["lat"] = true;
  variant["lon"] = true;
  variant["alt_baro"] = true;
  variant["gs"] = true;
  variant["track"] = true;
  variant["seen"] = true;
  variant["category"] = true;
}
}  // namespace

bool ADSBClient::fetch(float homeLat, float homeLon, uint16_t rangeKm, std::vector<Aircraft> &aircraft) {
  lastError_ = "";
  if (WiFi.status() != WL_CONNECTED) {
    lastError_ = "WiFi offline";
    Serial.println("[adsb] fetch skipped: WiFi offline");
    return false;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  HTTPClient http;
  String url = "https://opendata.adsb.fi/api/v3/lat/" + String(homeLat, 6) + "/lon/" + String(homeLon, 6) +
               "/dist/" + String(rangeKm);
  Serial.printf("[adsb] GET %s\n", url.c_str());
  http.setConnectTimeout(3500);
  http.setTimeout(4500);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!http.begin(secureClient, url)) {
    lastError_ = "HTTP begin failed";
    Serial.println("[adsb] HTTP begin failed");
    return false;
  }

  int code = http.GET();
  lastHttpCode_ = code;
  if (code != HTTP_CODE_OK) {
    lastError_ = "HTTP " + String(code);
    Serial.printf("[adsb] request failed: %s\n", lastError_.c_str());
    http.end();
    return false;
  }

  JsonDocument doc;
  JsonDocument filter;
  addAircraftFilter(filter["aircraft"][0]);
  addAircraftFilter(filter["ac"][0]);
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    lastError_ = "JSON " + String(err.c_str());
    Serial.printf("[adsb] parse failed: %s\n", err.c_str());
    return false;
  }

  JsonArray array;
  if (doc["aircraft"].is<JsonArray>()) {
    array = doc["aircraft"].as<JsonArray>();
  } else if (doc["ac"].is<JsonArray>()) {
    array = doc["ac"].as<JsonArray>();
  } else {
    lastError_ = "No aircraft array";
    Serial.println("[adsb] no aircraft array in response");
    aircraft.clear();
    return true;
  }

  std::vector<String> seenHexes;
  seenHexes.reserve(min((size_t)array.size(), Config::MAX_AIRCRAFT));
  size_t parsed = 0;

  for (JsonObject obj : array) {
    if (parsed >= Config::MAX_AIRCRAFT) break;
    Aircraft incoming;
    incoming.hex = obj["hex"] | "";
    incoming.hex.toUpperCase();
    if (incoming.hex.isEmpty()) continue;

    incoming.flight = cleanFlight(obj["flight"] | "");
    incoming.type = cleanFlight(obj["t"] | "");
    if (incoming.type.isEmpty()) incoming.type = cleanFlight(obj["type"] | "");
    incoming.type.toUpperCase();
    incoming.lat = obj["lat"].is<float>() ? obj["lat"].as<float>() : NAN;
    incoming.lon = obj["lon"].is<float>() ? obj["lon"].as<float>() : NAN;
    incoming.altBaro = obj["alt_baro"].is<int>() ? obj["alt_baro"].as<int>() : INT32_MIN;
    incoming.groundSpeed = obj["gs"].is<float>() ? obj["gs"].as<float>() : NAN;
    incoming.track = obj["track"].is<float>() ? obj["track"].as<float>() : NAN;
    incoming.seen = obj["seen"].is<float>() ? obj["seen"].as<float>() : NAN;
    incoming.category = obj["category"] | "";
    incoming.updatedAtMs = millis();

    mergeAircraft(aircraft, incoming);
    seenHexes.push_back(incoming.hex);
    parsed++;
  }

  aircraft.erase(std::remove_if(aircraft.begin(), aircraft.end(), [&](const Aircraft &a) {
                   return std::find(seenHexes.begin(), seenHexes.end(), a.hex) == seenHexes.end();
                 }),
                 aircraft.end());

  Serial.printf("[adsb] parsed=%u active=%u\n", (unsigned)parsed, (unsigned)aircraft.size());
  return true;
}

void ADSBClient::mergeAircraft(std::vector<Aircraft> &aircraft, Aircraft &incoming) {
  auto it = std::find_if(aircraft.begin(), aircraft.end(), [&](const Aircraft &a) { return a.hex == incoming.hex; });
  if (it == aircraft.end()) {
    if (hasPosition(incoming)) pushTrail(incoming, incoming.lat, incoming.lon);
    aircraft.push_back(incoming);
    return;
  }

  TrailPoint existingTrail[Config::TRAIL_POINTS];
  const uint8_t existingCount = it->trailCount;
  for (uint8_t i = 0; i < existingCount; ++i) existingTrail[i] = it->trail[i];
  *it = incoming;
  it->trailCount = existingCount;
  for (uint8_t i = 0; i < existingCount; ++i) it->trail[i] = existingTrail[i];
  if (hasPosition(*it)) pushTrail(*it, it->lat, it->lon);
}

void ADSBClient::pushTrail(Aircraft &aircraft, float lat, float lon) {
  if (aircraft.trailCount > 0) {
    TrailPoint &last = aircraft.trail[aircraft.trailCount - 1];
    if (fabs(last.lat - lat) < 0.0001f && fabs(last.lon - lon) < 0.0001f) return;
  }
  if (aircraft.trailCount < Config::TRAIL_POINTS) {
    TrailPoint point;
    point.lat = lat;
    point.lon = lon;
    point.seenAtMs = millis();
    aircraft.trail[aircraft.trailCount++] = point;
    return;
  }
  for (uint8_t i = 1; i < Config::TRAIL_POINTS; ++i) aircraft.trail[i - 1] = aircraft.trail[i];
  TrailPoint point;
  point.lat = lat;
  point.lon = lon;
  point.seenAtMs = millis();
  aircraft.trail[Config::TRAIL_POINTS - 1] = point;
}
