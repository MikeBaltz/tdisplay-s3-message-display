#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

using MessageReceivedCallback = void (*)(const char *topic, const uint8_t *payload, size_t length);

class MqttService {
 public:
  void begin(MessageReceivedCallback callback);
  void update();
  void loop();
  bool connected();
  // Best effort: returns false when offline or the broker refuses the publish.
  bool publish(const char *topic, const char *payload, bool retained = false);
  // True once after each successful (re)connection.
  bool consumeConnected();

 private:
  void connect();
  static void connectTask(void *context);
  static void callbackThunk(char *topic, byte *payload, unsigned int length);
  static MqttService *instance_;
  WiFiClientSecure tlsClient_;
  PubSubClient mqtt_{tlsClient_};
  MessageReceivedCallback callback_ = nullptr;
  String clientId_;
  uint32_t lastAttempt_ = 0;
  // The TLS handshake can take many seconds when the network is bad, so it
  // runs in its own task. While connecting_ is true only that task touches
  // the client; the main loop leaves it alone.
  volatile bool connecting_ = false;
  volatile bool justConnected_ = false;
  volatile uint32_t retryInterval_ = 5000;
};
