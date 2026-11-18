#pragma once

#include <Arduino.h>
#include <AnimatedGIF.h>
#include <TFT_eSPI.h>

class GifPlayer {
 public:
  void begin(TFT_eSPI *display);
  void request(const String &url);
  bool acceptUploadedBuffer(uint8_t *buffer, size_t size);
  void stop();
   void update(bool wifiConnected, bool playbackAllowed);
  bool consumePlaybackStarted();
  bool consumeDownloadApplied();
  bool playing() const;
  bool loading() const;
  bool overlayActive() const;
  // The animation currently held in memory. generation() changes every time
  // that animation is replaced or dropped, so it can be saved to flash once.
  const uint8_t *buffer() const { return gifBuffer_; }
  size_t size() const { return gifBuffer_ != nullptr ? gifSize_ : 0; }
  uint32_t generation() const { return generation_; }

 private:
  static void downloadTask(void *context);
  static void drawFrame(GIFDRAW *draw);
  bool download(const String &url);
  void showError(const char *message);
  void showLoading();
  void restartPlayback();
  void finishPlayback();

  static GifPlayer *instance_;
  TFT_eSPI *display_ = nullptr;
  AnimatedGIF gif_;
  String pendingUrl_;
  String downloadUrl_;
  String activeUrl_;
  uint8_t *gifBuffer_ = nullptr;
  size_t gifSize_ = 0;
  uint32_t generation_ = 0;
  TaskHandle_t downloadTaskHandle_ = nullptr;
  volatile bool downloadInProgress_ = false;
  volatile bool downloadReady_ = false;
  volatile bool downloadFailed_ = false;
  volatile bool downloadTooLarge_ = false;
  volatile bool downloadNoPsram_ = false;
  uint8_t *downloadedBuffer_ = nullptr;
  size_t downloadedSize_ = 0;
  bool downloadAttempted_ = false;
  uint8_t downloadAttempts_ = 0;
  bool downloadApplied_ = false;
  uint32_t lastDownloadAttempt_ = 0;
  bool playing_ = false;
  bool decoderOpen_ = false;
  bool loading_ = false;
  bool playbackStarted_ = false;
  uint32_t nextFrameAt_ = 0;
  uint32_t errorUntil_ = 0;
  int16_t canvasWidth_ = 0;
  int16_t canvasHeight_ = 0;
  int16_t destinationX_ = 0;
  int16_t destinationY_ = 0;
  int16_t destinationWidth_ = 0;
  int16_t destinationHeight_ = 0;
};
