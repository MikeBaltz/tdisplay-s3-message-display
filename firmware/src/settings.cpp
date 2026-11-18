#include "settings.h"

#include <Preferences.h>

namespace {
constexpr char NAMESPACE[] = "settings";

// Reads an integer field if it is present and inside [low, high].
bool readByte(JsonObjectConst message, const char *key, int low, int high, uint8_t &target) {
  if (!message[key].is<int>()) return false;
  const int value = message[key].as<int>();
  if (value < low || value > high || value == target) return false;
  target = static_cast<uint8_t>(value);
  return true;
}

bool readFlag(JsonObjectConst message, const char *key, bool &target) {
  if (!message[key].is<bool>()) return false;
  const bool value = message[key].as<bool>();
  if (value == target) return false;
  target = value;
  return true;
}
}  // namespace

void SettingsStore::load() {
  Preferences preferences;
  if (!preferences.begin(NAMESPACE, true)) return;  // nothing saved yet: keep defaults
  settings_.brightnessDay = preferences.getUChar("day", settings_.brightnessDay);
  settings_.brightnessNight = preferences.getUChar("night", settings_.brightnessNight);
  settings_.nightStartHour = preferences.getUChar("nightStart", settings_.nightStartHour);
  settings_.nightEndHour = preferences.getUChar("nightEnd", settings_.nightEndHour);
  settings_.clock24Hour = preferences.getBool("clock24", settings_.clock24Hour);
  preferences.getString("city", settings_.city, sizeof(settings_.city));
  if (settings_.city[0] == '\0') strlcpy(settings_.city, "London", sizeof(settings_.city));
  settings_.latitude = preferences.getFloat("lat", settings_.latitude);
  settings_.longitude = preferences.getFloat("lon", settings_.longitude);
  settings_.showWeather = preferences.getBool("weather", settings_.showWeather);
  settings_.showMessages = preferences.getBool("messages", settings_.showMessages);
  settings_.showAnimations = preferences.getBool("animations", settings_.showAnimations);
  preferences.end();
}

void SettingsStore::save() const {
  Preferences preferences;
  if (!preferences.begin(NAMESPACE, false)) {
    Serial.println("Settings: could not open flash storage; changes last until restart");
    return;
  }
  preferences.putUChar("day", settings_.brightnessDay);
  preferences.putUChar("night", settings_.brightnessNight);
  preferences.putUChar("nightStart", settings_.nightStartHour);
  preferences.putUChar("nightEnd", settings_.nightEndHour);
  preferences.putBool("clock24", settings_.clock24Hour);
  preferences.putString("city", settings_.city);
  preferences.putFloat("lat", settings_.latitude);
  preferences.putFloat("lon", settings_.longitude);
  preferences.putBool("weather", settings_.showWeather);
  preferences.putBool("messages", settings_.showMessages);
  preferences.putBool("animations", settings_.showAnimations);
  preferences.end();
}

bool SettingsStore::apply(JsonObjectConst message) {
  bool changed = false;
  changed |= readByte(message, "brightnessDay", 20, 255, settings_.brightnessDay);
  changed |= readByte(message, "brightnessNight", 2, 255, settings_.brightnessNight);
  changed |= readByte(message, "nightStartHour", 0, 23, settings_.nightStartHour);
  changed |= readByte(message, "nightEndHour", 0, 23, settings_.nightEndHour);
  changed |= readFlag(message, "clock24Hour", settings_.clock24Hour);
  changed |= readFlag(message, "showWeather", settings_.showWeather);
  changed |= readFlag(message, "showMessages", settings_.showMessages);
  changed |= readFlag(message, "showAnimations", settings_.showAnimations);

  // A place is only accepted whole: name, latitude and longitude together.
  const char *city = message["city"] | "";
  if (city[0] != '\0' && message["latitude"].is<float>() && message["longitude"].is<float>()) {
    const float latitude = message["latitude"].as<float>();
    const float longitude = message["longitude"].as<float>();
    if (latitude >= -90 && latitude <= 90 && longitude >= -180 && longitude <= 180 &&
        (strncmp(city, settings_.city, sizeof(settings_.city) - 1) != 0 ||
         latitude != settings_.latitude || longitude != settings_.longitude)) {
      // Copy at most what fits, without cutting a UTF-8 character in half.
      size_t length = strlen(city);
      if (length > sizeof(settings_.city) - 1) {
        length = sizeof(settings_.city) - 1;
        while (length > 0 && (static_cast<uint8_t>(city[length]) & 0xC0) == 0x80) --length;
      }
      memcpy(settings_.city, city, length);
      settings_.city[length] = '\0';
      settings_.latitude = latitude;
      settings_.longitude = longitude;
      changed = true;
    }
  }
  if (changed) save();
  return changed;
}

void SettingsStore::toJson(JsonObject target) const {
  target["brightnessDay"] = settings_.brightnessDay;
  target["brightnessNight"] = settings_.brightnessNight;
  target["nightStartHour"] = settings_.nightStartHour;
  target["nightEndHour"] = settings_.nightEndHour;
  target["clock24Hour"] = settings_.clock24Hour;
  target["city"] = settings_.city;
  target["latitude"] = settings_.latitude;
  target["longitude"] = settings_.longitude;
  target["showWeather"] = settings_.showWeather;
  target["showMessages"] = settings_.showMessages;
  target["showAnimations"] = settings_.showAnimations;
}
