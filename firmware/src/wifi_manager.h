#pragma once

#include <Arduino.h>
#include <DNSServer.h>

// Owns the Wi-Fi connection. The network name and password come from flash if
// they were set through the setup page, otherwise from secrets.h.
//
// Setup mode: the display opens its own open hotspot (GIFT_SETUP_NETWORK in gift_config.h) with
// a page at http://192.168.4.1/ where a new network can be entered. It keeps
// trying the known network in the background and leaves setup mode by itself
// if that connects.
class WifiManager {
 public:
  void begin();
  void update();

  bool connected() const;
  bool everConnected() const { return everConnected_; }
  bool setupMode() const { return setupMode_; }
  void startSetup();
  void stopSetup();
  const char *setupNetworkName() const;
  String networkName() const { return ssid_; }

  // The next two are called from the web server task.
  bool saveCredentials(const String &ssid, const String &password);
  void processDns();

  // True shortly after new credentials were saved: time to restart.
  bool restartDue() const;

 private:
  void connect();

  String ssid_;
  String password_;
  DNSServer dns_;
  SemaphoreHandle_t dnsLock_ = nullptr;
  volatile bool setupMode_ = false;
  volatile bool dnsRunning_ = false;
  volatile uint32_t restartAt_ = 0;
  bool everConnected_ = false;
  bool leaveSetupWhenConnected_ = false;
  bool attempted_ = false;
  uint32_t lastAttempt_ = 0;
};
