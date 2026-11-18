#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Everything that can be changed from the control page. Saved in flash (NVS).
struct Settings {
  uint8_t brightnessDay = 255;    // 20-255
  uint8_t brightnessNight = 14;   // 2-255
  uint8_t nightStartHour = 23;    // 0-23
  uint8_t nightEndHour = 7;       // 0-23; equal to the start hour = never dim
  bool clock24Hour = true;
  char city[32] = "London";
  float latitude = 51.5072f;
  float longitude = -0.1276f;
  bool showWeather = true;
  bool showMessages = true;
  bool showAnimations = true;
};

class SettingsStore {
 public:
  void load();
  // Applies the fields present in a {"type":"settings",...} message. Values out
  // of range are ignored. Returns true if anything changed (and was saved).
  bool apply(JsonObjectConst message);
  void toJson(JsonObject target) const;
  const Settings &get() const { return settings_; }

 private:
  void save() const;
  Settings settings_;
};
