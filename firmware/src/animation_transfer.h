#pragma once

#include <Arduino.h>

// Receives a GIF that is sent in pieces over MQTT, so the sender does not need
// to be on the same network as the display.
//
// Every transfer message is binary and starts with a 4-byte magic, "TDA1" for
// an animation (the same framing with "TDF1" carries a firmware update):
//
//   START  "TDA1" 0x01 <id:u32> <totalSize:u32>
//   CHUNK  "TDA1" 0x02 <id:u32> <offset:u32> <data...>
//   END    "TDA1" 0x03 <id:u32> <crc32:u32>
//
// All integers are little-endian. Chunks must arrive in order. The finished
// buffer is only handed over after the size, CRC-32 and GIF signature match,
// so a broken transfer never replaces the animation that is already stored.
class AnimationTransfer {
 public:
  enum class Result : uint8_t { NotTransfer, InProgress, Completed, Failed };
  enum class Kind : uint8_t { Gif, Firmware };

  AnimationTransfer() = default;
  // magic: four characters that open every message of this transfer type.
  AnimationTransfer(const char *magic, Kind kind) : magic_(magic), kind_(kind) {}

  static constexpr size_t MAX_SIZE = 4 * 1024 * 1024;
  static constexpr size_t MAX_CHUNK_DATA = 8192;
  static constexpr size_t HEADER_SIZE = 13;
  static constexpr uint32_t STALL_TIMEOUT_MS = 20000;

  // True for a message of any transfer type (used to keep them out of the log).
  static bool isTransferMessage(const uint8_t *payload, size_t length);

  // Handles one MQTT payload. Completed means takeBuffer() has a GIF ready.
  Result handle(const uint8_t *payload, size_t length);

  // Drops a transfer that stopped arriving. Returns true when one was dropped.
  bool update();

  // Ownership of the PSRAM buffer moves to the caller (free with heap_caps_free).
  uint8_t *takeBuffer(size_t &size);

  uint32_t transferId() const { return transferId_; }
  size_t receivedBytes() const { return received_; }
  const char *lastError() const { return lastError_; }

 private:
  Result fail(const char *reason);
  void reset();

  bool matches(const uint8_t *payload, size_t length) const;

  const char *magic_ = "TDA1";
  Kind kind_ = Kind::Gif;
  uint8_t *buffer_ = nullptr;
  size_t totalSize_ = 0;
  size_t received_ = 0;
  uint32_t transferId_ = 0;
  uint32_t crc_ = 0;
  uint32_t lastActivity_ = 0;
  uint8_t nextProgressLog_ = 25;
  bool active_ = false;
  bool complete_ = false;
  const char *lastError_ = "";
};
