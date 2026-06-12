#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <vector>

struct WifiCredential {
  String ssid;
  String password;
  int priority = 0;
  bool enabled = true;
};

class WiFiManagerExt {
 public:
  static constexpr size_t MAX_NETWORKS = 10;

  void begin();
  void load();
  void save();
  bool connectSaved(uint32_t timeoutMs = 30000, uint8_t retries = 2);
  void startReconnect();
  bool processReconnect(uint32_t timeoutMs = 30000, uint8_t retries = 2);
  bool addOrUpdate(const String &ssid, const String &password, bool enabled = true);
  bool remove(size_t index);
  bool moveUp(size_t index);
  bool moveDown(size_t index);
  bool toggleEnabled(size_t index);
  void clear();
  bool exportToSd();
  bool importFromSd();

  const std::vector<WifiCredential> &networks() const { return networks_; }
  size_t selectedIndex() const { return selectedIndex_; }
  void setSelectedIndex(size_t index);
  int currentIndex() const { return currentIndex_; }
  String currentSsid() const;
  String statusText() const;
  String ipText() const;
  int rssi() const;
  uint32_t connectedForMs() const;
  uint32_t lastReconnectMs() const { return lastReconnectMs_; }

 private:
  Preferences prefs_;
  std::vector<WifiCredential> networks_;
  size_t selectedIndex_ = 0;
  int currentIndex_ = -1;
  int reconnectIndex_ = 0;
  uint8_t reconnectTry_ = 0;
  bool reconnectActive_ = false;
  uint32_t attemptStartMs_ = 0;
  uint32_t connectedAtMs_ = 0;
  uint32_t lastReconnectMs_ = 0;

  void sortByPriority();
  void normalizePriorities();
  int nextEnabledIndex(int start) const;
  bool waitForConnection(uint32_t timeoutMs);
};
