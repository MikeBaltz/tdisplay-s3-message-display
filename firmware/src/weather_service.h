#pragma once

#include <Arduino.h>

struct WeatherData {
  bool valid = false;
  float currentTemperature = 0;
  float minimumTemperature = 0;
  float maximumTemperature = 0;
  int weatherCode = -1;
  bool isDay = true;
  uint32_t updatedAt = 0;
};

class WeatherService {
 public:
  void begin();
  void update(bool wifiConnected);
  // Changes the place. The next update() fetches fresh data for it.
  void setLocation(float latitude, float longitude);
  const WeatherData &data() const;
  String condition() const;

 private:
  static void fetchTask(void *context);
  bool fetch(WeatherData &result);
  WeatherData data_;
  String url_;
  float pendingLatitude_ = 0;
  float pendingLongitude_ = 0;
  bool locationPending_ = false;
  uint32_t lastAttempt_ = 0;
  uint32_t lastSuccess_ = 0;
  volatile bool fetching_ = false;
  TaskHandle_t taskHandle_ = nullptr;
};
