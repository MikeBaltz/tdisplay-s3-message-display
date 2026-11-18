#pragma once

#include <Arduino.h>

class TimeService {
 public:
  void begin();
  void update();
  bool synced() const;
  String timeText() const;
  String dateText() const;
  String messageTime() const;
  // The clock digits, always five characters: "20:37", or "08:37" in 12-hour mode.
  // shortTime() is the same moment for small labels: "20:37" or "8:37 PM".
  String shortTime() const;
  void setUse24Hour(bool use24Hour) { use24Hour_ = use24Hour; }
  bool use24Hour() const { return use24Hour_; }
  bool isPm() const;
  // Local hour 0-23, or -1 while the clock has not been set yet.
  int hour() const;

 private:
  bool synced_ = false;
  bool use24Hour_ = true;
  uint32_t lastCheck_ = 0;
};
