#include "gps.h"

#include "config.h"

namespace {
HardwareSerial gpsSerial(2);
}

void GPSModule::begin() {
  if (!Config::GPS_ENABLED) return;
  gpsSerial.begin(Config::GPS_BAUD, SERIAL_8N1, Config::GPS_RX, Config::GPS_TX);
  Serial.printf("[gps] optional GPS enabled on RX=%u TX=%u baud=%lu\n", Config::GPS_RX, Config::GPS_TX,
                Config::GPS_BAUD);
}

void GPSModule::update() {
  if (!Config::GPS_ENABLED) return;
  while (gpsSerial.available() > 0) {
    const char c = gpsSerial.read();
    gps_.encode(c);
    lastDataMs_ = millis();
  }
}

bool GPSModule::hasData() const {
  return Config::GPS_ENABLED && lastDataMs_ > 0 && millis() - lastDataMs_ <= Config::GPS_FIX_MAX_AGE_MS;
}

bool GPSModule::hasFix() const {
  return hasData() && gps_.location.isValid() && gps_.location.age() <= Config::GPS_FIX_MAX_AGE_MS;
}

float GPSModule::latitude() {
  return hasFix() ? gps_.location.lat() : NAN;
}

float GPSModule::longitude() {
  return hasFix() ? gps_.location.lng() : NAN;
}

uint32_t GPSModule::satellites() {
  return gps_.satellites.isValid() ? gps_.satellites.value() : 0;
}

String GPSModule::statusText() {
  if (!Config::GPS_ENABLED) return "Off";
  if (hasFix()) return "Fix " + String(satellites()) + " sat";
  if (hasData()) return "No fix";
  return "No GPS";
}
