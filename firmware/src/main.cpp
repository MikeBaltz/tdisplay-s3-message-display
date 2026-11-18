#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include <esp_heap_caps.h>

#include "animation_transfer.h"
#include "app_config.h"
#include "backlight.h"
#include "button_handler.h"
#include "gif_messages.h"
#include "gif_player.h"
#include "gift_config.h"
#include "messages.h"
#include "mqtt_service.h"
#include "settings.h"
#include "storage.h"
#include "time_service.h"
#include "ui.h"
#include "web_server.h"
#include "wifi_manager.h"
#include "weather_service.h"

namespace {
TFT_eSPI display;
ButtonHandler buttons;
MessageStore messages;
MqttService mqtt;
TimeService timeService;
WeatherService weather;
Ui ui;
GifPlayer gifPlayer;
GifMessageStore gifMessages;
WebServerService webServer;
AnimationTransfer animationTransfer;
AnimationTransfer firmwareTransfer("TDF1", AnimationTransfer::Kind::Firmware);
Backlight backlight;
Storage storage;
WifiManager wifiManager;
SettingsStore settings;
uint32_t bootStartedAt = 0;
uint32_t colonyCount = 0;      // messages ever received: colonies on the Culture screen
uint32_t lastActivityAt = 0;   // last button press, for the idle Culture screen
bool cultureIdle = false;      // the Culture screen was opened because nobody pressed anything

// A received firmware update waiting for the upper button (see
// holdFirmwareForConfirmation below).
struct PendingFirmware {
  uint8_t *image = nullptr;  // owned while waiting; nullptr when nothing waits
  size_t size = 0;
  uint32_t id = 0;
  uint32_t since = 0;
};
PendingFirmware pendingFirmware;

bool firmwarePending() { return pendingFirmware.image != nullptr; }

// Gift state kept in flash (NVS) apart from the settings: "preloaded" = the
// gift messages have been added to the inbox, "colonies" = the Culture count.
constexpr char GIFT_NAMESPACE[] = "gift";

bool giftFlag(const char *key) {
  Preferences preferences;
  if (!preferences.begin(GIFT_NAMESPACE, true)) return false;  // nothing saved yet
  const bool value = preferences.getBool(key, false);
  preferences.end();
  return value;
}

void setGiftFlag(const char *key) {
  Preferences preferences;
  if (!preferences.begin(GIFT_NAMESPACE, false)) return;
  preferences.putBool(key, true);
  preferences.end();
}

// Adds the messages from gift_config.h, oldest first, as unread messages, with
// the sender's name and no time.
void preloadGiftMessages() {
  static_assert(sizeof(GIFT_MESSAGES) / sizeof(GIFT_MESSAGES[0]) <= MessageStore::MAX_MESSAGES,
                "gift_config.h: at most 10 preloaded messages");
  for (const GiftMessage &gift : GIFT_MESSAGES) messages.add(gift.text, "", gift.sender);
  setGiftFlag("preloaded");
  Serial.printf("Gift: preloaded %u messages\n",
                static_cast<unsigned int>(sizeof(GIFT_MESSAGES) / sizeof(GIFT_MESSAGES[0])));
}

// The colony count lives in flash next to the gift flags. A device that never
// counted starts from the messages it already holds.
void loadColonies() {
  Preferences preferences;
  constexpr uint32_t UNSET = 0xFFFFFFFF;
  uint32_t saved = UNSET;
  if (preferences.begin(GIFT_NAMESPACE, true)) {
    saved = preferences.getUInt("colonies", UNSET);
    preferences.end();
  }
  colonyCount = saved != UNSET ? saved : messages.count() + gifMessages.unreadCount();
  if (saved == UNSET && preferences.begin(GIFT_NAMESPACE, false)) {
    preferences.putUInt("colonies", colonyCount);
    preferences.end();
  }
  ui.setColonies(colonyCount);
}

void addColony() {
  ++colonyCount;
  Preferences preferences;
  if (preferences.begin(GIFT_NAMESPACE, false)) {
    preferences.putUInt("colonies", colonyCount);
    preferences.end();
  }
  ui.setColonies(colonyCount);
}

// After CULTURE_IDLE_MINUTES without a press, show the living dish.
void updateIdleCulture() {
  const uint32_t now = millis();
  if (CULTURE_IDLE_MINUTES == 0 || cultureIdle) return;
  // Not while something else has the screen, or someone is watching a GIF.
  if (ui.booting() || wifiManager.setupMode() || firmwarePending() ||
      ui.screen() == Screen::Culture || ui.screen() == Screen::Animations) {
    lastActivityAt = now;
    return;
  }
  if (now - lastActivityAt < CULTURE_IDLE_MINUTES * 60000UL) return;
  cultureIdle = true;
  ui.openScreen(Screen::Culture);
}

// Serial command for testing the gift (115200 baud, end with Enter):
//   gift reset - forget the preloaded flag, the colony count and the saved
//                inbox, then restart: the preloaded messages come back.
//                Wi-Fi, settings and the saved animation are kept.
void handleSerialCommands() {
  static String line;
  while (Serial.available() > 0) {
    const char received = static_cast<char>(Serial.read());
    if (received != '\n' && received != '\r') {
      if (line.length() < 32) line += received;
      continue;
    }
    line.trim();
    if (line == "gift reset") {
      Preferences preferences;
      if (preferences.begin(GIFT_NAMESPACE, false)) {
        preferences.remove("preloaded");
        preferences.remove("colonies");
        preferences.end();
      }
      storage.removeSavedMessages();
      Serial.println("Gift: first-start state cleared; restarting");
      delay(200);
      ESP.restart();
    } else if (line.length() > 0) {
      Serial.println("Commands: gift reset");
    }
    line = "";
  }
}

// If the main loop ever stops for this long, the watchdog restarts the device.
constexpr uint32_t WATCHDOG_TIMEOUT_S = 60;
// Longest text message kept, in bytes. The Messages screen shows four lines.
constexpr size_t MAX_MESSAGE_BYTES = 800;

void reportWiFiStatus() {
  static wl_status_t previousStatus = WL_NO_SSID_AVAIL;
  const wl_status_t currentStatus = WiFi.status();
  if (currentStatus == previousStatus) return;
  previousStatus = currentStatus;
  if (currentStatus == WL_CONNECTED) {
    Serial.println("Wi-Fi connected");
    Serial.print("Local IP: ");
    Serial.println(WiFi.localIP());
    // Restart mDNS on every (re)connect so the name follows the new address.
    MDNS.end();
    if (MDNS.begin(MDNS_HOSTNAME)) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("mDNS started: http://%s.local/\n", MDNS_HOSTNAME);
    } else {
      Serial.println("mDNS start failed; use the IP address instead");
    }
    ui.invalidate();
  } else {
    Serial.printf("Wi-Fi disconnected (status %d)\n", currentStatus);
  }
}

// Pushes the current settings into the parts of the firmware that use them.
void applySettings() {
  const Settings &current = settings.get();
  static float appliedLatitude = NAN;
  static float appliedLongitude = NAN;
  timeService.setUse24Hour(current.clock24Hour);
  ui.configure(current.showWeather, current.showMessages, current.showAnimations, current.city);
  if (current.latitude != appliedLatitude || current.longitude != appliedLongitude) {
    appliedLatitude = current.latitude;
    appliedLongitude = current.longitude;
    weather.setLocation(current.latitude, current.longitude);
  }
}

// Publishes what the control page shows about the device. Retained, so the
// page sees the latest report as soon as it connects.
void publishDeviceStatus() {
  StaticJsonDocument<1024> document;
  document["type"] = "device";
  document["online"] = true;
  document["version"] = FIRMWARE_VERSION;
  document["time"] = timeService.synced() ? static_cast<uint32_t>(time(nullptr)) : 0;
  document["uptime"] = millis() / 1000;
  document["freeHeap"] = ESP.getFreeHeap();
  document["freePsram"] = ESP.getFreePsram();
  document["network"] = wifiManager.networkName();
  document["signal"] = WiFi.RSSI();
  document["ip"] = WiFi.localIP().toString();
  document["animationBytes"] = gifPlayer.size();
  document["messages"] = messages.count();
  document["unread"] = messages.unreadCount();
  settings.toJson(document.createNestedObject("settings"));
  char payload[1024];
  const size_t length = serializeJson(document, payload, sizeof(payload));
  if (length == 0 || length >= sizeof(payload) || !mqtt.publish(MQTT_STATUS_TOPIC, payload, true)) {
    Serial.println("Device status could not be published");
  }
}

void reportTransfer(const char *type, uint32_t id, bool ok, const char *error, size_t bytes) {
  char status[176];
  if (ok) {
    snprintf(status, sizeof(status), "{\"type\":\"%s\",\"id\":%u,\"ok\":true,\"bytes\":%u}", type,
             static_cast<unsigned int>(id), static_cast<unsigned int>(bytes));
  } else {
    snprintf(status, sizeof(status), "{\"type\":\"%s\",\"id\":%u,\"ok\":false,\"error\":\"%s\"}", type,
             static_cast<unsigned int>(id), error);
  }
  if (!mqtt.publish(MQTT_STATUS_TOPIC, status)) {
    Serial.println("Transfer status could not be published");
  }
}

// Writes a received firmware image to the spare application slot and restarts
// into it. The image was already checked against its CRC-32 during transfer.
void installFirmware(uint8_t *image, size_t size, uint32_t id) {
  Serial.printf("Firmware update: installing %u bytes\n", static_cast<unsigned int>(size));
  ui.showNotice("Updating", "Installing new firmware", "Keep the display plugged in", false);
  const char *error = nullptr;
  if (!Update.begin(size)) {
    error = "Not enough space for the update";
  } else {
    constexpr size_t BLOCK = 65536;
    for (size_t offset = 0; offset < size && error == nullptr; offset += BLOCK) {
      const size_t length = min(BLOCK, size - offset);
      if (Update.write(image + offset, length) != length) error = "Writing the update failed";
      esp_task_wdt_reset();
    }
    if (error == nullptr && !Update.end(true)) error = "The update image was rejected";
    if (error != nullptr) Update.abort();
  }
  heap_caps_free(image);
  if (error != nullptr) {
    Serial.printf("Firmware update failed: %s (%s)\n", error, Update.errorString());
    reportTransfer("firmware", id, false, error, 0);
    ui.invalidate();
    return;
  }
  Serial.println("Firmware update installed; restarting");
  reportTransfer("firmware", id, true, "", size);
  ui.showNotice("Updating", "Update installed", "Restarting", false);
  // Give the confirmation a moment to leave before the connection drops.
  delay(1200);
  ESP.restart();
}

// Returns true when the payload was a piece of an animation transfer.
bool handleAnimationTransfer(const uint8_t *payload, size_t length) {
  const AnimationTransfer::Result result = animationTransfer.handle(payload, length);
  if (result == AnimationTransfer::Result::NotTransfer) return false;
  if (result == AnimationTransfer::Result::Failed) {
    reportTransfer("animation", animationTransfer.transferId(), false, animationTransfer.lastError(), 0);
  } else if (result == AnimationTransfer::Result::Completed) {
    size_t size = 0;
    uint8_t *buffer = animationTransfer.takeBuffer(size);
    // MQTT callbacks run on the main loop, the same thread as the GIF decoder,
    // so the player can swap buffers safely here.
    if (buffer != nullptr && gifPlayer.acceptUploadedBuffer(buffer, size)) {
      gifMessages.add("remote-upload", timeService.messageTime());
      addColony();
      Serial.println("Remote animation applied as latest animation");
      reportTransfer("animation", animationTransfer.transferId(), true, "", size);
    } else {
      if (buffer != nullptr) heap_caps_free(buffer);
      Serial.println("Remote animation rejected by player");
      reportTransfer("animation", animationTransfer.transferId(), false, "Display rejected the GIF", 0);
    }
  }
  return true;
}

// A received firmware image is never installed straight away: it waits in
// PSRAM until someone presses the upper button on the display. The lower
// button, or the time running out, throws it away.
constexpr uint32_t FIRMWARE_CONFIRM_MS = 60000;

// Takes the image out of the waiting slot, so it can only be used or freed once.
uint8_t *takePendingFirmware(size_t &size, uint32_t &id) {
  uint8_t *image = pendingFirmware.image;
  size = pendingFirmware.size;
  id = pendingFirmware.id;
  pendingFirmware = PendingFirmware();
  return image;
}

void cancelPendingFirmware(const char *reason) {
  size_t size = 0;
  uint32_t id = 0;
  uint8_t *image = takePendingFirmware(size, id);
  if (image == nullptr) return;
  heap_caps_free(image);
  Serial.printf("Firmware update %u not installed: %s\n", static_cast<unsigned int>(id), reason);
  reportTransfer("firmware", id, false, "Not confirmed on the display", 0);
  ui.invalidate();
}

void confirmPendingFirmware() {
  size_t size = 0;
  uint32_t id = 0;
  uint8_t *image = takePendingFirmware(size, id);
  if (image != nullptr) installFirmware(image, size, id);  // frees the image
}

void holdFirmwareForConfirmation(uint8_t *image, size_t size, uint32_t id) {
  pendingFirmware.image = image;
  pendingFirmware.size = size;
  pendingFirmware.id = id;
  pendingFirmware.since = millis();
  backlight.wake();
  Serial.printf("Firmware update %u received (%u bytes); waiting for the upper button\n",
                static_cast<unsigned int>(id), static_cast<unsigned int>(size));
  char status[80];
  snprintf(status, sizeof(status), "{\"type\":\"firmware\",\"id\":%u,\"pending\":true}",
           static_cast<unsigned int>(id));
  if (!mqtt.publish(MQTT_STATUS_TOPIC, status)) Serial.println("Transfer status could not be published");
}

// Called from loop() while an update waits: draws the question with the
// seconds left and ends the wait when the time is up.
void updatePendingFirmware() {
  const uint32_t elapsed = millis() - pendingFirmware.since;
  if (elapsed >= FIRMWARE_CONFIRM_MS) {
    cancelPendingFirmware("timed out");
    return;
  }
  char countdown[12];
  snprintf(countdown, sizeof(countdown), "%u s",
           static_cast<unsigned int>((FIRMWARE_CONFIRM_MS - elapsed + 999) / 1000));
  ui.showNotice("Software update", "Press the upper button to install", "Lower button cancels",
                false, countdown);
}

// Returns true when the payload was a piece of a firmware update.
bool handleFirmwareTransfer(const uint8_t *payload, size_t length) {
  if (firmwarePending()) {
    // One update at a time: refuse a new one (reported once, on its START
    // message) and drop its chunks without storing them.
    if (length < 9 || memcmp(payload, "TDF1", 4) != 0) return false;
    if (payload[4] == 0x01) {
      const uint32_t id = static_cast<uint32_t>(payload[5]) | static_cast<uint32_t>(payload[6]) << 8 |
                          static_cast<uint32_t>(payload[7]) << 16 |
                          static_cast<uint32_t>(payload[8]) << 24;
      reportTransfer("firmware", id, false, "Another update is waiting", 0);
    }
    return true;
  }
  const AnimationTransfer::Result result = firmwareTransfer.handle(payload, length);
  if (result == AnimationTransfer::Result::NotTransfer) return false;
  if (result == AnimationTransfer::Result::Failed) {
    reportTransfer("firmware", firmwareTransfer.transferId(), false, firmwareTransfer.lastError(), 0);
  } else if (result == AnimationTransfer::Result::Completed) {
    size_t size = 0;
    uint8_t *image = firmwareTransfer.takeBuffer(size);
    if (image != nullptr) holdFirmwareForConfirmation(image, size, firmwareTransfer.transferId());
  }
  return true;
}

// Cuts a UTF-8 string to at most maxBytes, between characters.
size_t utf8Fit(const uint8_t *text, size_t length, size_t maxBytes) {
  if (length <= maxBytes) return length;
  length = maxBytes;
  while (length > 0 && (text[length] & 0xC0) == 0x80) --length;
  return length;
}

String utf8String(const uint8_t *text, size_t length) {
  String result;
  result.reserve(length);
  for (size_t index = 0; index < length; ++index) result += static_cast<char>(text[index]);
  return result;
}

// Messages from friends (radio/messages) are only ever text: never a transfer,
// settings, a GIF link or firmware, whatever the payload looks like.
//   {"from": "<name>", "text": "<message>"} with exactly those two fields shows
//   the name; anything else is shown as it arrived, as plain text.
void handleFriendMessage(const uint8_t *payload, size_t length) {
  constexpr size_t MAX_SENDER_BYTES = 24;
  Serial.printf("Friend message received (%u bytes)\n", static_cast<unsigned int>(length));
  String sender;
  String text;
  bool structured = false;
  // A valid message is at most 24 + 800 bytes of text; allow for JSON escapes.
  if (length > 0 && length <= 4096 && payload[0] == '{') {
    DynamicJsonDocument document(JSON_OBJECT_SIZE(2) + length + 64);
    if (!deserializeJson(document, payload, length) && document.is<JsonObjectConst>()) {
      JsonObjectConst object = document.as<JsonObjectConst>();
      if (object.size() == 2 && object["from"].is<const char *>() && object["text"].is<const char *>()) {
        const char *from = object["from"];
        const char *body = object["text"];
        const uint8_t *fromBytes = reinterpret_cast<const uint8_t *>(from);
        const uint8_t *bodyBytes = reinterpret_cast<const uint8_t *>(body);
        sender = utf8String(fromBytes, utf8Fit(fromBytes, strlen(from), MAX_SENDER_BYTES));
        text = utf8String(bodyBytes, utf8Fit(bodyBytes, strlen(body), MAX_MESSAGE_BYTES));
        // The name sits on one line in the header.
        for (size_t index = 0; index < sender.length(); ++index) {
          if (static_cast<uint8_t>(sender[index]) < 0x20) sender.setCharAt(index, ' ');
        }
        sender.trim();
        structured = true;
      }
    }
  }
  if (!structured) text = utf8String(payload, utf8Fit(payload, length, MAX_MESSAGE_BYTES));
  if (text.length() == 0) {
    Serial.println("Friend message was empty; ignored");
    return;
  }
  messages.add(text, timeService.messageTime(), sender);
  addColony();
  Serial.printf("Stored friend message%s; unread count: %u\n", structured ? " with a name" : "",
                messages.unreadCount());
}

void onMessageReceived(const char *topic, const uint8_t *payload, size_t length) {
  if (strcmp(topic, MQTT_MESSAGES_TOPIC) == 0) {
    handleFriendMessage(payload, length);
    return;
  }
  if (strcmp(topic, MQTT_TOPIC) != 0) return;  // not one of ours
  if (handleAnimationTransfer(payload, length)) return;
  if (handleFirmwareTransfer(payload, length)) return;
  Serial.printf("MQTT message received (%u bytes)\n", static_cast<unsigned int>(length));

  DynamicJsonDocument document(768);
  const DeserializationError error = deserializeJson(document, payload, length);
  if (!error && document["type"].is<const char *>()) {
    const char *type = document["type"] | "";
    Serial.print("Message type detected: ");
    Serial.println(type);
    if (strcmp(type, "settings") == 0) {
      if (settings.apply(document.as<JsonObjectConst>())) {
        applySettings();
        Serial.println("Settings updated from the control page");
      }
      publishDeviceStatus();  // doubles as the confirmation
      return;
    }
    if (strcmp(type, "status") == 0) {
      publishDeviceStatus();
      return;
    }
    if (strcmp(type, "gif") == 0 || strcmp(type, "animation") == 0) {
      const char *url = document["url"] | "";
      if (url[0] == '\0') {
        Serial.println("GIF error: JSON message has no URL");
        gifPlayer.request("");
        return;
      }
      if (!String(url).startsWith("https://")) {
        Serial.println("GIF error: URL must use HTTPS");
        gifPlayer.request(url);
        return;
      }
      // The unread badge is raised in loop() once the download has succeeded.
      gifPlayer.request(url);
      Serial.println("Animation request handled separately from plaintext messages");
      return;
    }
  } else if (error) {
    Serial.print("JSON parse failed; treating payload as plaintext: ");
    Serial.println(error.c_str());
  } else {
    Serial.println("Message type detected: plaintext");
  }

  // Keep very long messages to a sensible size, cutting between UTF-8 characters.
  if (length > MAX_MESSAGE_BYTES) {
    length = MAX_MESSAGE_BYTES;
    while (length > 0 && (payload[length] & 0xC0) == 0x80) --length;
  }
  String message;
  message.reserve(length);
  for (size_t index = 0; index < length; ++index) message += static_cast<char>(payload[index]);
  messages.add(message, timeService.messageTime());
  addColony();
  Serial.printf("Stored plaintext message; unread count: %u\n", messages.unreadCount());
}

// Startup screen: stays until Wi-Fi is up and the clock is set, or a button is
// pressed. If Wi-Fi never appears, the display opens its setup hotspot.
void updateBoot(bool buttonPressed) {
  if (!ui.booting()) return;
  const bool wifiReady = wifiManager.connected();
  const uint32_t elapsed = millis() - bootStartedAt;
  if ((wifiReady && timeService.synced()) || buttonPressed || (wifiReady && elapsed > 25000)) {
    ui.finishBoot();
    return;
  }
  if (!wifiReady && elapsed > 30000) {
    ui.finishBoot();
    wifiManager.startSetup();
    return;
  }
  ui.showBoot(wifiReady ? "Setting the clock" : "Connecting to Wi-Fi");
}

// Holding both buttons for three seconds opens Wi-Fi setup at any time.
void checkSetupGesture() {
  static uint32_t heldSince = 0;
  const bool bothDown = !firmwarePending() && digitalRead(BUTTON_BOOT_PIN) == LOW &&
                        digitalRead(BUTTON_USER_PIN) == LOW;
  if (!bothDown) {
    heldSince = 0;
    return;
  }
  if (heldSince == 0) heldSince = millis() | 1;
  if (millis() - heldSince >= 3000 && !wifiManager.setupMode()) {
    if (ui.booting()) ui.finishBoot();
    wifiManager.startSetup();
  }
}

// A connection problem is only shown after it has lasted a while, so brief
// reconnects do not flash a warning.
void updateConnectivity() {
  constexpr uint32_t GRACE_MS = 15000;
  static uint32_t wifiDownSince = 0;
  static uint32_t brokerDownSince = 0;
  const uint32_t now = millis() | 1;  // never 0, which means "not down"
  const bool wifiUp = wifiManager.connected();
  const bool brokerUp = mqtt.connected();
  if (wifiUp) wifiDownSince = 0;
  else if (wifiDownSince == 0) wifiDownSince = now;
  if (brokerUp) brokerDownSince = 0;
  else if (brokerDownSince == 0) brokerDownSince = now;
  ui.setConnectivity(wifiDownSince == 0 || now - wifiDownSince < GRACE_MS,
                     brokerDownSince == 0 || now - brokerDownSince < GRACE_MS);
}

void handleButtons() {
  const ButtonEvent event = buttons.update();
  if (event != ButtonEvent::None) {
    backlight.wake();
    lastActivityAt = millis();
  }
  // A waiting update owns both buttons: upper installs, lower cancels.
  if (firmwarePending()) {
    if (event == ButtonEvent::Button2Short || event == ButtonEvent::Button2Long) {
      confirmPendingFirmware();
    } else if (event == ButtonEvent::Button1Short) {
      cancelPendingFirmware("cancelled with the lower button");
    }
    return;
  }
  if (ui.booting()) {
    updateBoot(event != ButtonEvent::None);
    return;
  }
  if (wifiManager.setupMode()) {
    // Any button leaves setup mode and returns to the normal screens.
    if (event != ButtonEvent::None) wifiManager.stopSetup();
    return;
  }
  if (event != ButtonEvent::None && cultureIdle) {
    // The press that ends the idle Culture screen only brings back the clock.
    cultureIdle = false;
    if (ui.screen() == Screen::Culture) {
      ui.openScreen(Screen::Clock);
      return;
    }
  }
  switch (event) {
    case ButtonEvent::Button1Short:
      ui.nextScreen();
      break;
    case ButtonEvent::Button2Short:
      ui.nextMessage(messages, gifMessages);
      break;
    case ButtonEvent::Button2Long:
      ui.markCurrentRead(messages);
      Serial.printf("Marked message %u as read; unread count: %u\n",
                    ui.selectedMessage() + 1, messages.unreadCount());
      break;
    case ButtonEvent::None:
      break;
  }
}
}  // namespace

void setup() {
  pinMode(DISPLAY_POWER_PIN, OUTPUT);
  digitalWrite(DISPLAY_POWER_PIN, HIGH);
  pinMode(DISPLAY_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(DISPLAY_BACKLIGHT_PIN, HIGH);

  Serial.begin(115200);
  delay(300);
  Serial.printf("T-Display S3 starting, firmware %s\n", FIRMWARE_VERSION);

  display.init();
  display.setRotation(1);
  backlight.begin();
  ui.begin(&display);
  bootStartedAt = millis();
  ui.showBoot("Starting up");
  gifPlayer.begin(&display);
  buttons.begin();

  // Bring back what was on the display before the restart.
  if (storage.begin()) {
    // The gift messages go in once, on the very first start: never again after
    // a restart, and not back after they have been read or pushed out.
    const bool firstStart = !storage.hasSavedMessages();
    storage.loadMessages(messages);
    if (firstStart && !giftFlag("preloaded")) preloadGiftMessages();
    size_t savedSize = 0;
    uint8_t *saved = storage.loadAnimation(savedSize);
    if (saved != nullptr) {
      if (gifPlayer.acceptUploadedBuffer(saved, savedSize)) gifMessages.restore();
      else heap_caps_free(saved);
    }
    storage.markAnimationSaved(gifPlayer.generation());
  }

  settings.load();
  applySettings();
  loadColonies();
  wifiManager.begin();
  timeService.begin();
  mqtt.begin(onMessageReceived);
  weather.begin();
  webServer.begin(&gifPlayer, &gifMessages, &timeService, &wifiManager);

  esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
  esp_task_wdt_add(nullptr);
  bootStartedAt = millis();
}

void loop() {
  esp_task_wdt_reset();
  wifiManager.update();
  if (wifiManager.restartDue()) {
    Serial.println("Restarting to join the new Wi-Fi network");
    delay(200);
    ESP.restart();
  }
  reportWiFiStatus();
  timeService.update();
  mqtt.update();
  mqtt.loop();
  if (animationTransfer.update()) {
    reportTransfer("animation", animationTransfer.transferId(), false, animationTransfer.lastError(), 0);
  }
  if (firmwareTransfer.update()) {
    reportTransfer("firmware", firmwareTransfer.transferId(), false, firmwareTransfer.lastError(), 0);
  }
  webServer.update();
  weather.update(wifiManager.connected());
  handleSerialCommands();
  handleButtons();
  checkSetupGesture();
  updateIdleCulture();
  backlight.update(timeService.hour(), settings.get());
  // Report in when the broker connection comes up, then once a minute.
  static uint32_t lastStatusAt = 0;
  if (mqtt.consumeConnected() || (mqtt.connected() && millis() - lastStatusAt >= 60000)) {
    lastStatusAt = millis();
    publishDeviceStatus();
  }
  updateConnectivity();
  storage.updateAnimationSave(gifPlayer.buffer(), gifPlayer.size(), gifPlayer.generation());
  storage.updateMessageSave(messages);

  // A firmware update waiting for the button takes over the whole screen,
  // with the animation paused. Everything above keeps running meanwhile.
  static bool firmwareWasPending = false;
  if (firmwarePending()) {
    firmwareWasPending = true;
    gifPlayer.update(false, false);
    updatePendingFirmware();
    return;
  }
  if (firmwareWasPending) {
    firmwareWasPending = false;
    ui.invalidate();
  }

  // Wi-Fi setup takes over the whole screen until it ends.
  static bool setupWasActive = false;
  if (wifiManager.setupMode()) {
    setupWasActive = true;
    gifPlayer.update(false, false);  // pause any animation
    static const String joinLine = String("Join Wi-Fi \"") + GIFT_SETUP_NETWORK + "\"";
    ui.showNotice("Wi-Fi setup", joinLine.c_str(), "then open 192.168.4.1", false);
    return;
  }
  if (setupWasActive) {
    setupWasActive = false;
    ui.invalidate();
  }
  static Screen previousScreen = Screen::Clock;
  if (ui.screen() != previousScreen) {
    if (ui.screen() == Screen::Messages) ui.selectNewest(gifMessages);
    previousScreen = ui.screen();
  }
  // Paint a newly selected screen before the player starts drawing GIF frames.
  if (!gifPlayer.overlayActive()) ui.update(timeService, weather, messages, gifMessages);
  gifPlayer.update(wifiManager.connected(),
                   ui.screen() == Screen::Animations && !ui.booting());
  if (gifPlayer.consumeDownloadApplied()) {
    gifMessages.add("mqtt-download", timeService.messageTime());
    addColony();
  }
  if (gifPlayer.consumePlaybackStarted()) gifMessages.markNewestRead();
  static bool gifOverlayWasActive = false;
  const bool gifOverlayActive = gifPlayer.overlayActive();
  if (gifOverlayWasActive && !gifOverlayActive) ui.invalidate();
  gifOverlayWasActive = gifOverlayActive;
  if (!gifOverlayActive) ui.update(timeService, weather, messages, gifMessages);
}
