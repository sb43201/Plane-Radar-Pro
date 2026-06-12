#include "wifi_manager_ext.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <WiFi.h>
#include <algorithm>

#include "config.h"

namespace {
constexpr const char *WIFI_CONFIG_PATH = "/wifi_config.json";

String keyFor(const char *prefix, size_t index) {
  return String(prefix) + String(index);
}

bool validCred(const WifiCredential &cred) {
  return cred.ssid.length() > 0;
}
}  // namespace

void WiFiManagerExt::begin() {
  prefs_.begin("wifi-ext", false);
  load();
}

void WiFiManagerExt::load() {
  networks_.clear();
  const uint8_t count = prefs_.getUChar("count", 0);
  for (uint8_t i = 0; i < count && networks_.size() < MAX_NETWORKS; ++i) {
    WifiCredential cred;
    cred.ssid = prefs_.getString(keyFor("s", i).c_str(), "");
    cred.password = prefs_.getString(keyFor("p", i).c_str(), "");
    cred.priority = prefs_.getInt(keyFor("r", i).c_str(), i + 1);
    cred.enabled = prefs_.getBool(keyFor("e", i).c_str(), true);
    if (validCred(cred)) networks_.push_back(cred);
  }
  sortByPriority();
  normalizePriorities();
  Serial.printf("[wifi-ext] Loaded %u WiFi networks\n", (unsigned)networks_.size());
}

void WiFiManagerExt::save() {
  prefs_.clear();
  normalizePriorities();
  prefs_.putUChar("count", networks_.size());
  for (size_t i = 0; i < networks_.size(); ++i) {
    prefs_.putString(keyFor("s", i).c_str(), networks_[i].ssid);
    prefs_.putString(keyFor("p", i).c_str(), networks_[i].password);
    prefs_.putInt(keyFor("r", i).c_str(), networks_[i].priority);
    prefs_.putBool(keyFor("e", i).c_str(), networks_[i].enabled);
  }
}

bool WiFiManagerExt::connectSaved(uint32_t timeoutMs, uint8_t retries) {
  sortByPriority();
  normalizePriorities();
  currentIndex_ = -1;
  if (networks_.empty()) {
    Serial.println("[wifi-ext] no saved networks");
    return false;
  }

  for (size_t i = 0; i < networks_.size(); ++i) {
    WifiCredential &cred = networks_[i];
    if (!cred.enabled) continue;
    for (uint8_t attempt = 0; attempt < retries; ++attempt) {
      Serial.printf("[wifi-ext] Trying %s attempt %u/%u\n", cred.ssid.c_str(), attempt + 1, retries);
      WiFi.disconnect(false, false);
      delay(100);
      WiFi.begin(cred.ssid.c_str(), cred.password.c_str());
      if (waitForConnection(timeoutMs)) {
        currentIndex_ = (int)i;
        connectedAtMs_ = millis();
        lastReconnectMs_ = connectedAtMs_;
        Serial.println("[wifi-ext] Connected");
        Serial.printf("[wifi-ext] IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
      }
    }
  }
  Serial.println("[wifi-ext] all saved networks failed");
  return false;
}

void WiFiManagerExt::startReconnect() {
  reconnectActive_ = true;
  reconnectIndex_ = currentIndex_ >= 0 ? currentIndex_ : 0;
  reconnectTry_ = 0;
  attemptStartMs_ = 0;
}

bool WiFiManagerExt::processReconnect(uint32_t timeoutMs, uint8_t retries) {
  if (WiFi.status() == WL_CONNECTED) {
    reconnectActive_ = false;
    connectedAtMs_ = connectedAtMs_ ? connectedAtMs_ : millis();
    return true;
  }
  if (!reconnectActive_) startReconnect();
  if (networks_.empty()) return false;

  const uint32_t now = millis();
  if (attemptStartMs_ == 0 || now - attemptStartMs_ >= timeoutMs) {
    if (attemptStartMs_ != 0) {
      reconnectTry_++;
      if (reconnectTry_ >= retries) {
        reconnectTry_ = 0;
        reconnectIndex_ = nextEnabledIndex(reconnectIndex_ + 1);
      }
    }
    if (reconnectIndex_ < 0) return false;
    WifiCredential &cred = networks_[reconnectIndex_];
    Serial.printf("[wifi-ext] reconnect trying %s attempt %u/%u\n", cred.ssid.c_str(), reconnectTry_ + 1, retries);
    WiFi.disconnect(false, false);
    WiFi.begin(cred.ssid.c_str(), cred.password.c_str());
    attemptStartMs_ = now;
  }

  if (WiFi.status() == WL_CONNECTED) {
    currentIndex_ = reconnectIndex_;
    reconnectActive_ = false;
    connectedAtMs_ = millis();
    lastReconnectMs_ = connectedAtMs_;
    Serial.printf("[wifi-ext] reconnected to %s ip=%s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    return true;
  }
  return false;
}

bool WiFiManagerExt::addOrUpdate(const String &ssid, const String &password, bool enabled) {
  String cleanSsid = ssid;
  cleanSsid.trim();
  if (cleanSsid.isEmpty()) return false;
  for (WifiCredential &cred : networks_) {
    if (cred.ssid == cleanSsid) {
      cred.password = password;
      cred.enabled = enabled;
      save();
      Serial.printf("[wifi-ext] updated network %s\n", cleanSsid.c_str());
      return true;
    }
  }
  if (networks_.size() >= MAX_NETWORKS) return false;
  WifiCredential cred;
  cred.ssid = cleanSsid;
  cred.password = password;
  cred.enabled = enabled;
  cred.priority = networks_.size() + 1;
  networks_.push_back(cred);
  save();
  Serial.printf("[wifi-ext] added network %s\n", cleanSsid.c_str());
  return true;
}

bool WiFiManagerExt::remove(size_t index) {
  if (index >= networks_.size()) return false;
  Serial.printf("[wifi-ext] deleted network %s\n", networks_[index].ssid.c_str());
  networks_.erase(networks_.begin() + index);
  if (selectedIndex_ >= networks_.size()) selectedIndex_ = networks_.empty() ? 0 : networks_.size() - 1;
  save();
  return true;
}

bool WiFiManagerExt::moveUp(size_t index) {
  if (index == 0 || index >= networks_.size()) return false;
  std::swap(networks_[index - 1], networks_[index]);
  selectedIndex_ = index - 1;
  save();
  return true;
}

bool WiFiManagerExt::moveDown(size_t index) {
  if (index + 1 >= networks_.size()) return false;
  std::swap(networks_[index], networks_[index + 1]);
  selectedIndex_ = index + 1;
  save();
  return true;
}

bool WiFiManagerExt::toggleEnabled(size_t index) {
  if (index >= networks_.size()) return false;
  networks_[index].enabled = !networks_[index].enabled;
  save();
  return true;
}

void WiFiManagerExt::clear() {
  networks_.clear();
  selectedIndex_ = 0;
  currentIndex_ = -1;
  save();
}

bool WiFiManagerExt::exportToSd() {
  JsonDocument doc;
  JsonArray array = doc["networks"].to<JsonArray>();
  for (const WifiCredential &cred : networks_) {
    JsonObject obj = array.add<JsonObject>();
    obj["ssid"] = cred.ssid;
    obj["password"] = cred.password;
    obj["priority"] = cred.priority;
    obj["enabled"] = cred.enabled;
  }
  File file = SD.open(WIFI_CONFIG_PATH, FILE_WRITE);
  if (!file) {
    Serial.println("[wifi-ext] export failed: could not open SD file");
    return false;
  }
  serializeJsonPretty(doc, file);
  file.close();
  Serial.printf("[wifi-ext] exported WiFi settings to %s\n", WIFI_CONFIG_PATH);
  return true;
}

bool WiFiManagerExt::importFromSd() {
  File file = SD.open(WIFI_CONFIG_PATH, FILE_READ);
  if (!file) {
    Serial.println("[wifi-ext] import failed: file missing");
    return false;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    Serial.printf("[wifi-ext] import failed: %s\n", err.c_str());
    return false;
  }
  JsonArray array = doc["networks"].as<JsonArray>();
  if (array.isNull()) return false;
  networks_.clear();
  for (JsonObject obj : array) {
    if (networks_.size() >= MAX_NETWORKS) break;
    WifiCredential cred;
    cred.ssid = obj["ssid"] | "";
    cred.password = obj["password"] | "";
    cred.priority = obj["priority"] | (int)(networks_.size() + 1);
    cred.enabled = obj["enabled"] | true;
    if (validCred(cred)) networks_.push_back(cred);
  }
  sortByPriority();
  save();
  Serial.printf("[wifi-ext] imported %u WiFi networks\n", (unsigned)networks_.size());
  return true;
}

void WiFiManagerExt::setSelectedIndex(size_t index) {
  if (index < networks_.size()) selectedIndex_ = index;
}

String WiFiManagerExt::currentSsid() const {
  if (WiFi.status() != WL_CONNECTED) return "";
  return WiFi.SSID();
}

String WiFiManagerExt::statusText() const {
  if (WiFi.status() == WL_CONNECTED) return "Connected";
  return reconnectActive_ ? "Searching" : "Offline";
}

String WiFiManagerExt::ipText() const {
  if (WiFi.status() != WL_CONNECTED) return "---";
  return WiFi.localIP().toString();
}

int WiFiManagerExt::rssi() const {
  if (WiFi.status() != WL_CONNECTED) return 0;
  return WiFi.RSSI();
}

uint32_t WiFiManagerExt::connectedForMs() const {
  if (WiFi.status() != WL_CONNECTED || connectedAtMs_ == 0) return 0;
  return millis() - connectedAtMs_;
}

void WiFiManagerExt::sortByPriority() {
  std::sort(networks_.begin(), networks_.end(), [](const WifiCredential &a, const WifiCredential &b) {
    return a.priority < b.priority;
  });
}

void WiFiManagerExt::normalizePriorities() {
  for (size_t i = 0; i < networks_.size(); ++i) networks_[i].priority = i + 1;
}

int WiFiManagerExt::nextEnabledIndex(int start) const {
  if (networks_.empty()) return -1;
  for (size_t offset = 0; offset < networks_.size(); ++offset) {
    const int index = (start + offset) % networks_.size();
    if (networks_[index].enabled) return index;
  }
  return -1;
}

bool WiFiManagerExt::waitForConnection(uint32_t timeoutMs) {
  const uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < timeoutMs) {
    delay(50);
    yield();
  }
  return WiFi.status() == WL_CONNECTED;
}
