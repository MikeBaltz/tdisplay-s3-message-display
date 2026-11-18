#include "storage.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>

namespace {
constexpr char ANIMATION_PATH[] = "/animation.gif";
constexpr char ANIMATION_TEMP_PATH[] = "/animation.tmp";
constexpr char MESSAGES_PATH[] = "/messages.json";
constexpr char MESSAGES_TEMP_PATH[] = "/messages.tmp";
constexpr size_t MAX_ANIMATION_SIZE = 4 * 1024 * 1024;
constexpr size_t SAVE_CHUNK = 8192;
constexpr size_t FREE_SPACE_MARGIN = 64 * 1024;
constexpr uint32_t MESSAGE_SAVE_DELAY_MS = 1500;
File animationFile;
}  // namespace

bool Storage::begin() {
  // true: format the partition the first time, when it holds no filesystem yet.
  ready_ = LittleFS.begin(true);
  if (!ready_) {
    Serial.println("Storage: could not mount flash filesystem; nothing will be saved");
    return false;
  }
  Serial.printf("Storage ready: %u of %u bytes used\n",
                static_cast<unsigned int>(LittleFS.usedBytes()),
                static_cast<unsigned int>(LittleFS.totalBytes()));
  // A leftover temp file means power was lost mid-save; the previous good file is still intact.
  LittleFS.remove(ANIMATION_TEMP_PATH);
  LittleFS.remove(MESSAGES_TEMP_PATH);
  return true;
}

uint8_t *Storage::loadAnimation(size_t &size) {
  size = 0;
  if (!ready_ || !LittleFS.exists(ANIMATION_PATH)) return nullptr;
  File file = LittleFS.open(ANIMATION_PATH, "r");
  if (!file) return nullptr;
  const size_t fileSize = file.size();
  if (fileSize < 6 || fileSize > MAX_ANIMATION_SIZE) {
    file.close();
    return nullptr;
  }
  uint8_t *buffer = static_cast<uint8_t *>(
      heap_caps_malloc(fileSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (buffer == nullptr) {
    file.close();
    return nullptr;
  }
  const size_t read = file.read(buffer, fileSize);
  file.close();
  if (read != fileSize) {
    heap_caps_free(buffer);
    Serial.println("Storage: saved animation could not be read completely");
    return nullptr;
  }
  size = fileSize;
  Serial.printf("Storage: loaded saved animation (%u bytes)\n", static_cast<unsigned int>(size));
  return buffer;
}

void Storage::markAnimationSaved(uint32_t generation) {
  abortAnimationSave();
  savedGeneration_ = generation;
}

void Storage::abortAnimationSave() {
  if (!savingAnimation_) return;
  animationFile.close();
  LittleFS.remove(ANIMATION_TEMP_PATH);
  savingAnimation_ = false;
}

void Storage::updateAnimationSave(const uint8_t *buffer, size_t size, uint32_t generation) {
  if (!ready_) return;
  if (savingAnimation_ && generation != savingGeneration_) abortAnimationSave();
  if (!savingAnimation_) {
    if (generation == savedGeneration_) return;
    if (buffer == nullptr || size == 0) {
      // The animation was dropped (for example it could not be decoded).
      LittleFS.remove(ANIMATION_PATH);
      savedGeneration_ = generation;
      return;
    }
    // The old file is replaced, so its space counts as available.
    size_t available = LittleFS.totalBytes() - LittleFS.usedBytes();
    if (LittleFS.exists(ANIMATION_PATH)) {
      File existing = LittleFS.open(ANIMATION_PATH, "r");
      if (existing) {
        available += existing.size();
        existing.close();
      }
      // Both files exist briefly; make room first if the new one would not fit beside the old.
      if (LittleFS.totalBytes() - LittleFS.usedBytes() < size + FREE_SPACE_MARGIN) {
        LittleFS.remove(ANIMATION_PATH);
      }
    }
    if (available < size + FREE_SPACE_MARGIN) {
      Serial.printf("Storage: animation (%u bytes) is too large to save; it will be lost on restart\n",
                    static_cast<unsigned int>(size));
      LittleFS.remove(ANIMATION_PATH);
      savedGeneration_ = generation;
      return;
    }
    animationFile = LittleFS.open(ANIMATION_TEMP_PATH, "w");
    if (!animationFile) {
      Serial.println("Storage: could not open file for saving animation");
      savedGeneration_ = generation;
      return;
    }
    savingAnimation_ = true;
    savingGeneration_ = generation;
    saveOffset_ = 0;
  }

  const size_t toWrite = min(SAVE_CHUNK, size - saveOffset_);
  if (animationFile.write(buffer + saveOffset_, toWrite) != toWrite) {
    Serial.println("Storage: write failed while saving animation");
    abortAnimationSave();
    savedGeneration_ = generation;
    return;
  }
  saveOffset_ += toWrite;
  if (saveOffset_ < size) return;

  animationFile.close();
  savingAnimation_ = false;
  LittleFS.remove(ANIMATION_PATH);
  if (LittleFS.rename(ANIMATION_TEMP_PATH, ANIMATION_PATH)) {
    Serial.printf("Storage: animation saved (%u bytes)\n", static_cast<unsigned int>(size));
  } else {
    Serial.println("Storage: could not finish saving animation");
  }
  savedGeneration_ = generation;
}

bool Storage::hasSavedMessages() const { return ready_ && LittleFS.exists(MESSAGES_PATH); }

void Storage::removeSavedMessages() {
  if (ready_) LittleFS.remove(MESSAGES_PATH);
}

void Storage::loadMessages(MessageStore &messages) {
  if (!ready_ || !LittleFS.exists(MESSAGES_PATH)) return;
  File file = LittleFS.open(MESSAGES_PATH, "r");
  if (!file) return;
  DynamicJsonDocument document(24576);
  const DeserializationError error = deserializeJson(document, file);
  file.close();
  if (error) {
    Serial.print("Storage: saved messages could not be read: ");
    Serial.println(error.c_str());
    return;
  }
  uint8_t restored = 0;
  for (JsonObject entry : document["messages"].as<JsonArray>()) {
    const char *content = entry["text"] | "";
    if (content[0] == '\0') continue;
    messages.restore(content, entry["time"] | "--:--", entry["unread"] | false,
                     entry["from"] | "");
    ++restored;
  }
  savedMessageRevision_ = messages.revision();
  pendingMessageRevision_ = savedMessageRevision_;
  Serial.printf("Storage: restored %u messages\n", restored);
}

void Storage::updateMessageSave(const MessageStore &messages) {
  if (!ready_) return;
  const uint32_t revision = messages.revision();
  if (revision == savedMessageRevision_) return;
  // Wait for a quiet moment so several quick changes become one write.
  if (revision != pendingMessageRevision_) {
    pendingMessageRevision_ = revision;
    messageChangedAt_ = millis();
    return;
  }
  if (millis() - messageChangedAt_ < MESSAGE_SAVE_DELAY_MS) return;

  DynamicJsonDocument document(24576);
  JsonArray list = document.createNestedArray("messages");
  for (uint8_t index = 0; index < messages.count(); ++index) {
    const RadioMessage *message = messages.at(index);
    JsonObject entry = list.createNestedObject();
    entry["text"] = message->content;
    entry["time"] = message->receivedTime;
    entry["unread"] = message->unread;
    if (message->sender.length() > 0) entry["from"] = message->sender;
  }
  savedMessageRevision_ = revision;
  File file = LittleFS.open(MESSAGES_TEMP_PATH, "w");
  if (!file) {
    Serial.println("Storage: could not open file for saving messages");
    return;
  }
  const bool written = serializeJson(document, file) > 0 && !document.overflowed();
  file.close();
  if (!written) {
    LittleFS.remove(MESSAGES_TEMP_PATH);
    Serial.println("Storage: messages were not saved");
    return;
  }
  LittleFS.remove(MESSAGES_PATH);
  LittleFS.rename(MESSAGES_TEMP_PATH, MESSAGES_PATH);
}
