#pragma once

#include <Arduino.h>
#include <WebServer.h>

class GifMessageStore;
class GifPlayer;
class TimeService;
class WifiManager;

class WebServerService {
 public:
  void begin(GifPlayer *player, GifMessageStore *messages, TimeService *timeService,
             WifiManager *wifi);
  void update();

 private:
  static void serverTask(void *context);
  void runServer();
  void handleRoot();
  void handleStatus();
  void handleWifiSave();
  void handleWifiScan();
  void handleUploadResult();
  void handleUpload();
  void finishUpload(bool success, const char *error = nullptr);
  void takeCompletedUpload();

  WebServer server_{80};
  GifPlayer *player_ = nullptr;
  GifMessageStore *messages_ = nullptr;
  TimeService *timeService_ = nullptr;
  WifiManager *wifi_ = nullptr;
  TaskHandle_t taskHandle_ = nullptr;
  uint8_t *uploadBuffer_ = nullptr;
  size_t uploadSize_ = 0;
  size_t uploadCapacity_ = 0;
  bool uploadFailed_ = false;
  String uploadError_;
  uint32_t lastProgressLog_ = 0;
  volatile uint8_t *completedBuffer_ = nullptr;
  volatile size_t completedSize_ = 0;
  volatile bool completedReady_ = false;
  portMUX_TYPE bufferMux_ = portMUX_INITIALIZER_UNLOCKED;
  bool uploadResultOk_ = false;
  String uploadResultError_;
};
