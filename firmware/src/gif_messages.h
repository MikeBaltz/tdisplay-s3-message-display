#pragma once

#include <Arduino.h>

struct GifMessage {
  String url;
  String receivedTime;
  bool unread;
};

class GifMessageStore {
 public:
  void add(const String &url, const String &receivedTime);
  uint8_t unreadCount() const;
  uint32_t revision() const;
  bool hasLatest() const;
  const String &latestUrl() const;
  const String &latestTime() const;
  void markNewestRead();
  // An animation loaded from flash at startup: present, but not new.
  void restore();

 private:
  GifMessage latest_;
  bool hasMessage_ = false;
  uint32_t revision_ = 0;
};
