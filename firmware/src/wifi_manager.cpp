#include "wifi_manager.h"

#include <Preferences.h>
#include <WiFi.h>

#include "gift_config.h"
#include "secrets.h"

namespace {
// The hotspot name is set in gift_config.h.
constexpr const char *SETUP_NETWORK = GIFT_SETUP_NETWORK;
constexpr char PREFERENCES_NAMESPACE[] = "wifi";
constexpr uint32_t RETRY_MS = 10000;
// In setup mode each attempt briefly disturbs the hotspot, so try less often.
constexpr uint32_t SETUP_RETRY_MS = 45000;
}  // namespace

void WifiManager::begin() {
  dnsLock_ = xSemaphoreCreateMutex();
  ssid_ = WIFI_SSID;
  password_ = WIFI_PASSWORD;
  Preferences preferences;
  if (preferences.begin(PREFERENCES_NAMESPACE, true)) {
    const String savedSsid = preferences.getString("ssid", "");
    if (savedSsid.length() > 0) {
      ssid_ = savedSsid;
      password_ = preferences.getString("password", "");
      Serial.println("Wi-Fi: using the network saved from the setup page");
    }
    preferences.end();
  }
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  connect();
}

void WifiManager::connect() {
  attempted_ = true;
  lastAttempt_ = millis();
  if (ssid_.length() == 0) return;
  Serial.println("Connecting to Wi-Fi");
  WiFi.begin(ssid_.c_str(), password_.c_str());
}

void WifiManager::update() {
  if (WiFi.status() == WL_CONNECTED) {
    everConnected_ = true;
    // Setup that opened because Wi-Fi was missing closes itself when Wi-Fi
    // returns. Setup opened by hand while connected stays until dismissed.
    if (setupMode_ && leaveSetupWhenConnected_ && restartAt_ == 0) {
      Serial.println("Wi-Fi connected; leaving setup mode");
      stopSetup();
    }
    return;
  }
  const uint32_t interval = setupMode_ ? SETUP_RETRY_MS : RETRY_MS;
  if (attempted_ && millis() - lastAttempt_ < interval) return;
  connect();
}

bool WifiManager::connected() const { return WiFi.status() == WL_CONNECTED; }

const char *WifiManager::setupNetworkName() const { return SETUP_NETWORK; }

void WifiManager::startSetup() {
  if (setupMode_) return;
  leaveSetupWhenConnected_ = WiFi.status() != WL_CONNECTED;
  Serial.printf("Wi-Fi setup mode: join \"%s\" and open http://192.168.4.1/\n", SETUP_NETWORK);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(SETUP_NETWORK);
  delay(100);  // let the access point come up before reading its address
  xSemaphoreTake(dnsLock_, portMAX_DELAY);
  // Answer every name with our own address so phones open the setup page by themselves.
  dns_.setErrorReplyCode(DNSReplyCode::NoError);
  dnsRunning_ = dns_.start(53, "*", WiFi.softAPIP());
  xSemaphoreGive(dnsLock_);
  setupMode_ = true;
}

void WifiManager::stopSetup() {
  if (!setupMode_) return;
  setupMode_ = false;
  xSemaphoreTake(dnsLock_, portMAX_DELAY);
  if (dnsRunning_) dns_.stop();
  dnsRunning_ = false;
  xSemaphoreGive(dnsLock_);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
}

void WifiManager::processDns() {
  if (!dnsRunning_ || dnsLock_ == nullptr) return;
  if (xSemaphoreTake(dnsLock_, 0) != pdTRUE) return;
  if (dnsRunning_) dns_.processNextRequest();
  xSemaphoreGive(dnsLock_);
}

bool WifiManager::saveCredentials(const String &ssid, const String &password) {
  // Wi-Fi limits: names up to 32 bytes; passwords empty (open network) or 8-63 characters.
  if (ssid.length() == 0 || ssid.length() > 32) return false;
  if (password.length() != 0 && (password.length() < 8 || password.length() > 63)) return false;
  Preferences preferences;
  if (!preferences.begin(PREFERENCES_NAMESPACE, false)) return false;
  const bool saved = preferences.putString("ssid", ssid) > 0;
  if (password.length() > 0) preferences.putString("password", password);
  else preferences.remove("password");
  preferences.end();
  if (!saved) return false;
  Serial.println("Wi-Fi: new network saved; restarting shortly");
  restartAt_ = millis() + 2500;
  if (restartAt_ == 0) restartAt_ = 1;
  return true;
}

bool WifiManager::restartDue() const {
  return restartAt_ != 0 && static_cast<int32_t>(millis() - restartAt_) >= 0;
}
