#pragma once

#include <Arduino.h>
#include <vector>

struct Airport {
  String ident;
  String name;
  String type;
  String gpsCode;
  String iataCode;
  String localCode;
  double lat = NAN;
  double lon = NAN;
  float distanceKm = NAN;
  float bearingDeg = NAN;
};

class AirportManager {
 public:
  bool begin();
  bool loadNearby(double homeLat, double homeLon);
  void refreshCalculations(double homeLat, double homeLon);
  bool refreshIfDue(double homeLat, double homeLon, bool force = false);
  const std::vector<Airport> &airports() const { return airports_; }
  const Airport *findByCode(const String &code) const;
  bool findInDatabaseByCode(const String &code, Airport &airport) const;
  String statusText() const;
  bool hasWarning() const { return warning_.length() > 0; }
  const String &warning() const { return warning_; }
  size_t parsedCount() const { return parsedCount_; }
  size_t nearbyCount() const { return airports_.size(); }

  static String displayCode(const Airport &airport);

 private:
  std::vector<Airport> airports_;
  String warning_;
  bool sdReady_ = false;
  size_t parsedCount_ = 0;
  uint32_t lastRefreshMs_ = 0;
  double lastLat_ = NAN;
  double lastLon_ = NAN;

  bool parseCsvLine(const char *line, Airport &airport) const;
  static String csvField(const char *line, uint8_t targetIndex);
  void sortAndTrim();
};
