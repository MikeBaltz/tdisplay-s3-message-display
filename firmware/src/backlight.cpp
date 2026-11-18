#include "backlight.h"

#include "app_config.h"

namespace {
constexpr uint8_t PWM_CHANNEL = 0;
constexpr uint32_t PWM_FREQUENCY_HZ = 2000;
constexpr uint8_t PWM_RESOLUTION_BITS = 8;
constexpr uint32_t FADE_STEP_MS = 4;  // one brightness step: about 1 s for a full fade

bool isNight(int hour, const Settings &settings) {
  const int start = settings.nightStartHour;
  const int end = settings.nightEndHour;
  if (hour < 0 || start == end) return false;
  if (start > end) return hour >= start || hour < end;  // the usual case: spans midnight
  return hour >= start && hour < end;
}
}  // namespace

void Backlight::begin() {
  ledcSetup(PWM_CHANNEL, PWM_FREQUENCY_HZ, PWM_RESOLUTION_BITS);
  ledcAttachPin(DISPLAY_BACKLIGHT_PIN, PWM_CHANNEL);
  current_ = 255;
  target_ = 255;
  ledcWrite(PWM_CHANNEL, current_);
}

void Backlight::update(int hour, const Settings &settings) {
  const uint32_t now = millis();
  if (woken_ && static_cast<int32_t>(now - wakeUntil_) >= 0) woken_ = false;
  target_ = (woken_ || !isNight(hour, settings)) ? settings.brightnessDay
                                                 : settings.brightnessNight;
  if (current_ == target_ || now - lastStep_ < FADE_STEP_MS) return;
  lastStep_ = now;
  // Waking up is quick; settling down to the night level is gentle.
  if (current_ < target_) {
    current_ = static_cast<uint8_t>(min(static_cast<int>(target_), current_ + 6));
  } else {
    --current_;
  }
  ledcWrite(PWM_CHANNEL, current_);
}

void Backlight::wake() {
  woken_ = true;
  wakeUntil_ = millis() + BRIGHTNESS_WAKE_MS;
}
