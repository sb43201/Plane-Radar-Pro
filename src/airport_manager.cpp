#include "airport_manager.h"

#include <SD.h>
#include <SPI.h>
#include <algorithm>

#include "config.h"
#include "radar.h"

namespace {
SPIClass sdSpi(VSPI);

bool validPosition(double lat, double lon) {
  return !isnan(lat) && !isnan(lon) && lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;
}

bool blankLine(const char *line) {
  while (*line) {
    if (*line != ' ' && *line != '\t' && *line != '\r' && *line != '\n') return false;
    line++;
  }
  return true;
}
}  // namespace

bool AirportManager::begin() {
  sdSpi.begin(Config::SD_SCK, Config::SD_MISO, Config::SD_MOSI, Config::SD_CS);
  sdReady_ = SD.begin(Config::SD_CS, sdSpi, 16000000);
  if (!sdReady_) {
    warning_ = "SD missing";
    Serial.println("[airport] SD card not available");
    return false;
  }
  if (!SD.exists(Config::AIRPORT_CSV_PATH)) {
    warning_ = "airports.csv missing";
    Serial.printf("[airport] %s not found on SD\n", Config::AIRPORT_CSV_PATH);
    return false;
  }
  warning_ = "";
  Serial.println("[airport] SD ready");
  return true;
}

bool AirportManager::loadNearby(double homeLat, double homeLon) {
  airports_.clear();
  parsedCount_ = 0;
  if (!sdReady_ && !begin()) return false;
  if (!validPosition(homeLat, homeLon)) {
    warning_ = "Airport home invalid";
    Serial.println("[airport] invalid home position");
    return false;
  }

  File file = SD.open(Config::AIRPORT_CSV_PATH, FILE_READ);
  if (!file) {
    warning_ = "airports.csv open failed";
    Serial.println("[airport] airports.csv open failed");
    return false;
  }

  Serial.printf("[airport] Loaded airports.csv size=%u bytes\n", (unsigned)file.size());
  bool firstLine = true;
  char line[512];
  while (file.available()) {
    const size_t len = file.readBytesUntil('\n', line, sizeof(line) - 1);
    line[len] = '\0';
    if (len == sizeof(line) - 1) {
      while (file.available() && file.read() != '\n') yield();
      Serial.println("[airport] skipped overlong CSV line");
      continue;
    }
    if (blankLine(line)) continue;
    if (firstLine) {
      firstLine = false;
      if (strncmp(line, "ident,", 6) == 0) continue;
    }

    Airport airport;
    if (!parseCsvLine(line, airport)) continue;
    parsedCount_++;
    if (parsedCount_ % 1000 == 0) {
      Serial.printf("[airport] parsed=%u nearby=%u\n", (unsigned)parsedCount_, (unsigned)airports_.size());
      yield();
    }

    airport.distanceKm = Radar::distanceKm(homeLat, homeLon, airport.lat, airport.lon);
    if (airport.distanceKm > Config::AIRPORT_LOAD_RADIUS_KM) continue;
    airport.bearingDeg = Radar::bearingDeg(homeLat, homeLon, airport.lat, airport.lon);
    airports_.push_back(airport);
    if (airports_.size() > Config::AIRPORT_MAX_RETAINED * 2) sortAndTrim();
  }
  file.close();

  sortAndTrim();
  lastLat_ = homeLat;
  lastLon_ = homeLon;
  lastRefreshMs_ = millis();
  warning_ = "";

  Serial.printf("[airport] Parsed airport count=%u\n", (unsigned)parsedCount_);
  Serial.printf("[airport] Nearby airport count=%u\n", (unsigned)airports_.size());
  if (!airports_.empty()) {
    const Airport &nearest = airports_.front();
    Serial.printf("[airport] Nearest airport=%s %.1f km\n", displayCode(nearest).c_str(), nearest.distanceKm);
  }
  return true;
}

void AirportManager::refreshCalculations(double homeLat, double homeLon) {
  if (!validPosition(homeLat, homeLon)) return;
  for (Airport &airport : airports_) {
    airport.distanceKm = Radar::distanceKm(homeLat, homeLon, airport.lat, airport.lon);
    airport.bearingDeg = Radar::bearingDeg(homeLat, homeLon, airport.lat, airport.lon);
  }
  sortAndTrim();
  lastLat_ = homeLat;
  lastLon_ = homeLon;
  lastRefreshMs_ = millis();
}

bool AirportManager::refreshIfDue(double homeLat, double homeLon, bool force) {
  if (airports_.empty()) return false;
  const uint32_t now = millis();
  const bool timedOut = now - lastRefreshMs_ >= Config::AIRPORT_REFRESH_MS;
  const bool moved = validPosition(lastLat_, lastLon_) &&
                     Radar::distanceKm(lastLat_, lastLon_, homeLat, homeLon) >= Config::AIRPORT_RELOAD_MOVE_KM;
  if (!force && !timedOut && !moved) return false;
  refreshCalculations(homeLat, homeLon);
  Serial.printf("[airport] refreshed calculations home=(%.5f, %.5f)\n", homeLat, homeLon);
  return true;
}

const Airport *AirportManager::findByCode(const String &code) const {
  for (const Airport &airport : airports_) {
    if (displayCode(airport) == code) return &airport;
  }
  return nullptr;
}

String AirportManager::statusText() const {
  if (warning_.length()) return warning_;
  return String(airports_.size()) + " airports";
}

String AirportManager::displayCode(const Airport &airport) {
  if (airport.iataCode.length()) return airport.iataCode;
  if (airport.gpsCode.length()) return airport.gpsCode;
  if (airport.localCode.length()) return airport.localCode;
  return airport.ident;
}

bool AirportManager::parseCsvLine(const char *line, Airport &airport) const {
  airport.ident = csvField(line, 0);
  airport.type = csvField(line, 1);
  airport.name = csvField(line, 2);
  airport.lat = csvField(line, 3).toDouble();
  airport.lon = csvField(line, 4).toDouble();
  airport.gpsCode = csvField(line, 10);
  airport.iataCode = csvField(line, 11);
  airport.localCode = csvField(line, 12);
  return airport.ident.length() && validPosition(airport.lat, airport.lon);
}

String AirportManager::csvField(const char *line, uint8_t targetIndex) {
  String field;
  field.reserve(32);
  uint8_t index = 0;
  bool quoted = false;
  for (size_t i = 0; line[i] != '\0'; ++i) {
    const char c = line[i];
    if (c == '\r' || c == '\n') break;
    if (c == '"') {
      quoted = !quoted;
      continue;
    }
    if (c == ',' && !quoted) {
      if (index == targetIndex) {
        field.trim();
        return field;
      }
      field = "";
      index++;
      continue;
    }
    if (index == targetIndex) field += c;
  }
  if (index == targetIndex) {
    field.trim();
    return field;
  }
  return "";
}

void AirportManager::sortAndTrim() {
  std::sort(airports_.begin(), airports_.end(), [](const Airport &a, const Airport &b) {
    return a.distanceKm < b.distanceKm;
  });
  if (airports_.size() > Config::AIRPORT_MAX_RETAINED) airports_.resize(Config::AIRPORT_MAX_RETAINED);
}
