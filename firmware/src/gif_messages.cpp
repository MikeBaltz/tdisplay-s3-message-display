#include "gif_messages.h"

void GifMessageStore::add(const String &url, const String &receivedTime) {
  ++revision_;
  latest_ = {url, receivedTime, true};
  hasMessage_ = true;
}

uint8_t GifMessageStore::unreadCount() const {
  return hasMessage_ && latest_.unread ? 1 : 0;
}

uint32_t GifMessageStore::revision() const { return revision_; }

bool GifMessageStore::hasLatest() const { return hasMessage_; }

const String &GifMessageStore::latestUrl() const { return latest_.url; }

const String &GifMessageStore::latestTime() const { return latest_.receivedTime; }

void GifMessageStore::markNewestRead() {
  if (hasMessage_) latest_.unread = false;
}

void GifMessageStore::restore() {
  ++revision_;
  latest_ = {"saved", "", false};
  hasMessage_ = true;
}
