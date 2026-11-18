#include "animation_transfer.h"

#include <esp_heap_caps.h>

namespace {
constexpr uint8_t TYPE_START = 1;
constexpr uint8_t TYPE_CHUNK = 2;
constexpr uint8_t TYPE_END = 3;

uint32_t readU32(const uint8_t *bytes) {
  return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

// Standard CRC-32 (IEEE 802.3), processed four bits at a time so no large
// table is needed. Pass the previous return value to continue a running CRC.
uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t length) {
  static const uint32_t TABLE[16] = {
      0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4,
      0x4DB26158, 0x5005713C, 0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
      0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C};
  crc = ~crc;
  for (size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    crc = TABLE[crc & 0x0F] ^ (crc >> 4);
    crc = TABLE[crc & 0x0F] ^ (crc >> 4);
  }
  return ~crc;
}
}  // namespace

bool AnimationTransfer::isTransferMessage(const uint8_t *payload, size_t length) {
  return payload != nullptr && length >= 9 &&
         (memcmp(payload, "TDA1", 4) == 0 || memcmp(payload, "TDF1", 4) == 0);
}

bool AnimationTransfer::matches(const uint8_t *payload, size_t length) const {
  return payload != nullptr && length >= 9 && memcmp(payload, magic_, 4) == 0;
}

AnimationTransfer::Result AnimationTransfer::handle(const uint8_t *payload, size_t length) {
  if (!matches(payload, length)) return Result::NotTransfer;
  const uint8_t type = payload[4];
  const uint32_t id = readU32(payload + 5);

  if (type == TYPE_START) {
    if (length < HEADER_SIZE) return Result::InProgress;
    // A new START always wins: the sender has given up on any earlier transfer.
    reset();
    transferId_ = id;
    const uint32_t totalSize = readU32(payload + 9);
    if (totalSize < 6) return fail("File is too small");
    if (totalSize > MAX_SIZE) return fail("File exceeds 4 MiB limit");
    if (heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) < totalSize) {
      return fail("Not enough PSRAM available");
    }
    buffer_ = static_cast<uint8_t *>(
        heap_caps_malloc(totalSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (buffer_ == nullptr) return fail("PSRAM allocation failed");
    totalSize_ = totalSize;
    active_ = true;
    lastActivity_ = millis();
    Serial.printf("Transfer %08X started: %u bytes\n", static_cast<unsigned int>(id),
                  static_cast<unsigned int>(totalSize));
    return Result::InProgress;
  }

  // Pieces of a transfer that is not (or no longer) running are ignored, so
  // one failure is reported once instead of once per remaining chunk.
  if (!active_ || id != transferId_) return Result::InProgress;

  if (type == TYPE_CHUNK) {
    if (length < HEADER_SIZE) return fail("Malformed chunk");
    const uint32_t offset = readU32(payload + 9);
    const size_t dataLength = length - HEADER_SIZE;
    if (offset != received_) return fail("Chunk lost or out of order");
    if (dataLength > totalSize_ - received_) return fail("More data than announced");
    memcpy(buffer_ + received_, payload + HEADER_SIZE, dataLength);
    crc_ = crc32Update(crc_, payload + HEADER_SIZE, dataLength);
    received_ += dataLength;
    lastActivity_ = millis();
    const uint8_t percent = static_cast<uint8_t>((static_cast<uint64_t>(received_) * 100) / totalSize_);
    if (percent >= nextProgressLog_ && percent < 100) {
      Serial.printf("Transfer: %u%%\n", percent);
      nextProgressLog_ = static_cast<uint8_t>(percent - percent % 25 + 25);
    }
    return Result::InProgress;
  }

  if (type == TYPE_END) {
    if (length < HEADER_SIZE) return fail("Malformed end message");
    if (received_ != totalSize_) return fail("Transfer incomplete");
    if (readU32(payload + 9) != crc_) return fail("Checksum mismatch");
    if (kind_ == Kind::Gif) {
      if (memcmp(buffer_, "GIF87a", 6) != 0 && memcmp(buffer_, "GIF89a", 6) != 0) {
        return fail("File is not a GIF");
      }
    } else if (buffer_[0] != 0xE9) {  // every ESP32 application image starts with 0xE9
      return fail("File is not a firmware image");
    }
    active_ = false;
    complete_ = true;
    Serial.printf("Transfer %08X complete: %u bytes\n", static_cast<unsigned int>(id),
                  static_cast<unsigned int>(received_));
    return Result::Completed;
  }

  return Result::InProgress;
}

bool AnimationTransfer::update() {
  if (!active_ || millis() - lastActivity_ < STALL_TIMEOUT_MS) return false;
  fail("Transfer timed out");
  return true;
}

uint8_t *AnimationTransfer::takeBuffer(size_t &size) {
  if (!complete_) {
    size = 0;
    return nullptr;
  }
  uint8_t *buffer = buffer_;
  size = received_;
  buffer_ = nullptr;
  complete_ = false;
  totalSize_ = 0;
  received_ = 0;
  return buffer;
}

AnimationTransfer::Result AnimationTransfer::fail(const char *reason) {
  Serial.printf("Transfer %08X failed: %s\n", static_cast<unsigned int>(transferId_),
                reason);
  const uint32_t id = transferId_;
  reset();
  transferId_ = id;
  lastError_ = reason;
  return Result::Failed;
}

void AnimationTransfer::reset() {
  if (buffer_ != nullptr) heap_caps_free(buffer_);
  buffer_ = nullptr;
  totalSize_ = 0;
  received_ = 0;
  crc_ = 0;
  nextProgressLog_ = 25;
  active_ = false;
  complete_ = false;
  lastError_ = "";
}
