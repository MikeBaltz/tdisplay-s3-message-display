#pragma once

#include <Arduino.h>

#include "settings.h"

// Drives the display backlight with PWM and fades between day and night levels.
class Backlight {
 public:
  void begin();
  // hour: local hour 0-23, or -1 when the time is not known yet.
  void update(int hour, const Settings &settings);
  // Call on any button press: full brightness for BRIGHTNESS_WAKE_MS.
  void wake();
  uint8_t level() const { return current_; }

 private:
  uint8_t current_ = 255;
  uint8_t target_ = 255;
  uint32_t wakeUntil_ = 0;
  uint32_t lastStep_ = 0;
  bool woken_ = false;
};
