#pragma once

#include <Arduino.h>

#include "messages.h"

// Keeps the latest animation and the message history in flash (LittleFS), so
// they survive a restart or power cut.
class Storage {
 public:
  bool begin();

  // Returns a PSRAM buffer holding the saved animation, or nullptr. The caller
  // owns the buffer (free with heap_caps_free).
  uint8_t *loadAnimation(size_t &size);
  // Tells the saver that this generation is already what is stored.
  void markAnimationSaved(uint32_t generation);
  // Call every loop with the player's current animation. Writes a little at a
  // time so the screen and buttons stay responsive; restarts if the animation
  // changes midway. Only ever touches the buffer passed in this call.
  void updateAnimationSave(const uint8_t *buffer, size_t size, uint32_t generation);

  // True once a message history file exists (even an empty one).
  bool hasSavedMessages() const;
  void loadMessages(MessageStore &messages);
  // Deletes the saved message history (used by the gift reset command).
  void removeSavedMessages();
  void updateMessageSave(const MessageStore &messages);

 private:
  void abortAnimationSave();

  bool ready_ = false;
  bool savingAnimation_ = false;
  uint32_t savedGeneration_ = 0;
  uint32_t savingGeneration_ = 0;
  size_t saveOffset_ = 0;
  uint32_t savedMessageRevision_ = 0;
  uint32_t pendingMessageRevision_ = 0;
  uint32_t messageChangedAt_ = 0;
};
