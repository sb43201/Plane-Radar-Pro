#include "gps.h"

#include "config.h"

namespace {
HardwareSerial gpsSerial(2);
}

void GPSModule::begin() {
  if (!Config::GPS_ENABLED) return;
  gpsSerial.begin(Config::GPS_BAUD, SERIAL_8N1, Config::GPS_RX, Config::GPS_TX);
  Serial.printf("[gps] optional GPS enabled on RX=%d TX=%d baud=%lu\n", Config::GPS_RX, Config::GPS_TX,
                Config::GPS_BAUD);
}

void GPSModule::update() {
  if (!Config::GPS_ENABLED) return;
  while (gpsSerial.available() > 0) {
    const char c = gpsSerial.read();
    gps_.encode(c);
    lastDataMs_ = millis();
  }

  const uint32_t now = millis();
  if (now - lastDebugMs_ >= 5000) {
    const uint32_t chars = gps_.charsProcessed();
    const uint32_t deltaChars = chars - lastCharsProcessed_;
    lastCharsProcessed_ = chars;
    lastDebugMs_ = now;

    if (deltaChars > 0) {
      Serial.printf("[gps] rx=%lu chars/5s total=%lu valid=%lu failed=%lu sats=%lu fix=%s locAge=%lu ms\n",
                    deltaChars, chars, gps_.passedChecksum(), gps_.failedChecksum(), satellites(),
                    hasFix() ? "yes" : "no",
                    gps_.location.isValid() ? gps_.location.age() : 0UL);
    } else {
      Serial.printf("[gps] no serial data on RX=%d; check GPS TX -> ESP32 GPIO%d and common GND\n", Config::GPS_RX,
                    Config::GPS_RX);
    }
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

bool GPSModule::hasCourse() const {
  return hasFix() && gps_.course.isValid() && gps_.course.age() <= Config::GPS_FIX_MAX_AGE_MS;
}

float GPSModule::courseDeg() {
  return hasCourse() ? gps_.course.deg() : NAN;
}

float GPSModule::speedKmph() {
  return hasFix() && gps_.speed.isValid() ? gps_.speed.kmph() : NAN;
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
