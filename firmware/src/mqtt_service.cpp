#include "mqtt_service.h"

#include <WiFi.h>

#include "animation_transfer.h"
#include "app_config.h"
#include "secrets.h"

namespace {
constexpr uint32_t MQTT_RETRY_MIN_MS = 5000;
constexpr uint32_t MQTT_RETRY_MAX_MS = 60000;
const char MQTT_ROOT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
)EOF";
}

MqttService *MqttService::instance_ = nullptr;

void MqttService::begin(MessageReceivedCallback callback) {
  callback_ = callback;
  instance_ = this;
  const uint64_t chipId = ESP.getEfuseMac();
  char clientId[32];
  snprintf(clientId, sizeof(clientId), "walkie-ESP32-%04X%08X",
           static_cast<uint16_t>(chipId >> 32), static_cast<uint32_t>(chipId));
  clientId_ = clientId;
  tlsClient_.setCACert(MQTT_ROOT_CA);
  mqtt_.setServer(MQTT_HOST, MQTT_PORT);
  mqtt_.setCallback(callbackThunk);
  // Must hold one whole animation chunk plus the MQTT topic and headers.
  mqtt_.setBufferSize(AnimationTransfer::MAX_CHUNK_DATA + 1024);
  mqtt_.setKeepAlive(30);
  mqtt_.setSocketTimeout(3);
  Serial.print("MQTT client ID: ");
  Serial.println(clientId_);
}

void MqttService::update() {
  if (connecting_) return;
  if (WiFi.status() != WL_CONNECTED || mqtt_.connected() ||
      (lastAttempt_ != 0 && millis() - lastAttempt_ < retryInterval_)) {
    return;
  }
  lastAttempt_ = millis() | 1;
  connecting_ = true;
  if (xTaskCreatePinnedToCore(connectTask, "mqtt_connect", 10240, this, 1, nullptr, 0) != pdPASS) {
    connecting_ = false;
    Serial.println("MQTT error: connect task creation failed");
  }
}

void MqttService::connectTask(void *context) {
  MqttService *service = static_cast<MqttService *>(context);
  service->connect();
  service->connecting_ = false;
  vTaskDelete(nullptr);
}

void MqttService::connect() {
  Serial.printf("Connecting to MQTT broker %s:%u\n", MQTT_HOST, MQTT_PORT);
  // Last will: if the display drops off without saying goodbye, the broker
  // replaces the retained status with this, so the control page shows "offline".
  if (mqtt_.connect(clientId_.c_str(), MQTT_USERNAME, MQTT_PASSWORD, MQTT_STATUS_TOPIC, 0, true,
                    "{\"type\":\"device\",\"online\":false}")) {
    Serial.println("MQTT connected");
    justConnected_ = true;
    // Runs after every (re)connect: the broker forgets subscriptions of a
    // clean session, so both topics are subscribed again each time.
    for (const char *topic : {MQTT_TOPIC, MQTT_MESSAGES_TOPIC}) {
      if (mqtt_.subscribe(topic)) Serial.printf("Subscribed to %s\n", topic);
      else Serial.printf("MQTT subscription to %s failed\n", topic);
    }
    retryInterval_ = MQTT_RETRY_MIN_MS;
  } else {
    // Wait longer after each failure, up to a minute, so a broken network is not hammered.
    retryInterval_ = min(MQTT_RETRY_MAX_MS, retryInterval_ * 2);
    Serial.printf("MQTT connection error, state: %d; next try in %u s\n", mqtt_.state(),
                  static_cast<unsigned int>(retryInterval_ / 1000));
  }
}

void MqttService::loop() {
  if (!connecting_ && mqtt_.connected()) mqtt_.loop();
}

bool MqttService::connected() { return !connecting_ && mqtt_.connected(); }

bool MqttService::publish(const char *topic, const char *payload, bool retained) {
  return !connecting_ && mqtt_.connected() && mqtt_.publish(topic, payload, retained);
}

bool MqttService::consumeConnected() {
  if (connecting_ || !justConnected_) return false;
  justConnected_ = false;
  return true;
}

void MqttService::callbackThunk(char *topic, byte *payload, unsigned int length) {
  if (instance_ == nullptr) return;
  // Animation chunks arrive many times a second; logging each one would only
  // slow the transfer down.
  if (!AnimationTransfer::isTransferMessage(payload, length)) {
    Serial.print("Incoming topic: ");
    Serial.println(topic);
    Serial.printf("Incoming message (%u bytes)\n", length);
  }
  if (instance_->callback_ != nullptr) instance_->callback_(topic, payload, length);
}
