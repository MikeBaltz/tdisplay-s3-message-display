#pragma once

#include <Arduino.h>

struct RadioMessage {
  String content;
  String receivedTime;
  bool unread;
  String sender;  // who sent it; empty for the owner's messages
};

class MessageStore {
 public:
  static constexpr uint8_t MAX_MESSAGES = 10;

  void add(const String &content, const String &receivedTime, const String &sender = "");
  // Appends an older message when loading saved history (newest must be restored first).
  void restore(const String &content, const String &receivedTime, bool unread,
               const String &sender = "");
  RadioMessage *at(uint8_t index);
  const RadioMessage *at(uint8_t index) const;
  uint8_t count() const;
  uint8_t unreadCount() const;
  uint32_t revision() const;
  void markRead(uint8_t index);
  int newestIndex() const;

 private:
  RadioMessage messages_[MAX_MESSAGES];
  uint8_t count_ = 0;
  uint32_t revision_ = 0;
};
