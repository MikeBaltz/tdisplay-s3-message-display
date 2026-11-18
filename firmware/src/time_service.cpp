#include "time_service.h"

#include <time.h>

void TimeService::begin() {
  configTzTime("EET-2EEST,M3.5.0/3,M10.5.0/4", "pool.ntp.org", "time.nist.gov");
  Serial.println("NTP time synchronization started");
}

void TimeService::update() {
  if (synced_ && millis() - lastCheck_ < 60000) return;
  lastCheck_ = millis();
  struct tm localTime;
  if (getLocalTime(&localTime, 100)) {
    if (!synced_) Serial.println("NTP time synchronized");
    synced_ = true;
  }
}

bool TimeService::synced() const { return synced_; }

String TimeService::timeText() const {
  struct tm localTime;
  if (!getLocalTime(&localTime, 0)) return "--:--";
  char result[6];
  strftime(result, sizeof(result), use24Hour_ ? "%H:%M" : "%I:%M", &localTime);
  return result;
}

String TimeService::shortTime() const {
  struct tm localTime;
  if (!getLocalTime(&localTime, 0)) return "--:--";
  char result[12];
  if (use24Hour_) {
    strftime(result, sizeof(result), "%H:%M", &localTime);
    return result;
  }
  strftime(result, sizeof(result), "%I:%M %p", &localTime);
  return result[0] == '0' ? String(result + 1) : String(result);
}

bool TimeService::isPm() const {
  struct tm localTime;
  if (!getLocalTime(&localTime, 0)) return false;
  return localTime.tm_hour >= 12;
}

String TimeService::dateText() const {
  struct tm localTime;
  if (!getLocalTime(&localTime, 0)) return "Waiting for time";
  char result[24];
  strftime(result, sizeof(result), "%a, %d %b %Y", &localTime);
  return result;
}

String TimeService::messageTime() const { return shortTime(); }

int TimeService::hour() const {
  struct tm localTime;
  if (!getLocalTime(&localTime, 0)) return -1;
  return localTime.tm_hour;
}
