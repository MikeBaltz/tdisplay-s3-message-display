#include "messages.h"

void MessageStore::add(const String &content, const String &receivedTime, const String &sender) {
  ++revision_;
  const uint8_t newCount = count_ < MAX_MESSAGES ? count_ + 1 : MAX_MESSAGES;
  for (uint8_t index = newCount - 1; index > 0; --index) {
    messages_[index] = messages_[index - 1];
  }
  messages_[0] = {content, receivedTime, true, sender};
  count_ = newCount;
}

void MessageStore::restore(const String &content, const String &receivedTime, bool unread,
                           const String &sender) {
  if (count_ >= MAX_MESSAGES) return;
  messages_[count_++] = {content, receivedTime, unread, sender};
  ++revision_;
}

RadioMessage *MessageStore::at(uint8_t index) {
  return index < count_ ? &messages_[index] : nullptr;
}

const RadioMessage *MessageStore::at(uint8_t index) const {
  return index < count_ ? &messages_[index] : nullptr;
}

uint8_t MessageStore::count() const { return count_; }

uint8_t MessageStore::unreadCount() const {
  uint8_t unread = 0;
  for (uint8_t index = 0; index < count_; ++index) {
    if (messages_[index].unread) ++unread;
  }
  return unread;
}

uint32_t MessageStore::revision() const { return revision_; }

void MessageStore::markRead(uint8_t index) {
  if (index < count_ && messages_[index].unread) {
    messages_[index].unread = false;
    ++revision_;  // so the change is saved
  }
}

int MessageStore::newestIndex() const { return count_ == 0 ? -1 : 0; }
