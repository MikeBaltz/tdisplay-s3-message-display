#pragma once

#include <Arduino.h>

constexpr char FIRMWARE_VERSION[] = "1.3.0";

constexpr uint16_t SCREEN_WIDTH = 320;
constexpr uint16_t SCREEN_HEIGHT = 170;
constexpr uint8_t DISPLAY_POWER_PIN = 15;
constexpr uint8_t DISPLAY_BACKLIGHT_PIN = 38;

// Backlight levels and night hours are settings (see settings.h). A button
// press at night brings the screen back to the day level for this long.
constexpr uint32_t BRIGHTNESS_WAKE_MS = 12000;

constexpr uint8_t BUTTON_BOOT_PIN = 0;
constexpr uint8_t BUTTON_USER_PIN = 14;

// The control page is served at http://<MDNS_HOSTNAME>.local/
constexpr char MDNS_HOSTNAME[] = "tdisplay";

// Your MQTT broker (TLS on MQTT_PORT). Any broker with TLS and username/password
// login works; the project was built on EMQX Cloud.
constexpr char MQTT_HOST[] = "your-broker.example.com";
constexpr uint16_t MQTT_PORT = 8883;
// Owner topic: text, settings, GIF links, GIF transfers and firmware updates.
constexpr char MQTT_TOPIC[] = "radio/inbox";
// Friends' topic: only ever shown as a text message, whatever it contains.
// Plain text, or JSON {"from": "<name>", "text": "<message>"}.
constexpr char MQTT_MESSAGES_TOPIC[] = "radio/messages";
// The display reports the outcome of each animation transfer here so the
// control page can confirm delivery.
constexpr char MQTT_STATUS_TOPIC[] = "radio/status";

// Latitude and longitude are appended from the settings.
constexpr char WEATHER_URL_BASE[] =
    "https://api.open-meteo.com/v1/forecast?current=temperature_2m,weather_code,is_day&daily=temperature_2m_max,temperature_2m_min&forecast_days=1&timezone=auto";
