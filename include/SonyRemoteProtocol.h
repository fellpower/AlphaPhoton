#pragma once

#include <Arduino.h>

namespace SonyRemoteProtocol {

// Sony's proprietary BLE remote service (RMT-P1BT compatible).
constexpr char kServiceUuid[] = "8000ff00-ff00-ffff-ffff-ffffffffffff";
constexpr char kCommandUuid[] = "0000ff01-0000-1000-8000-00805f9b34fb";
constexpr char kNotifyUuid[] = "0000ff02-0000-1000-8000-00805f9b34fb";

constexpr uint8_t kHalfUp[] = {0x01, 0x06};
constexpr uint8_t kHalfDown[] = {0x01, 0x07};
constexpr uint8_t kFullUp[] = {0x01, 0x08};
constexpr uint8_t kFullDown[] = {0x01, 0x09};
constexpr uint8_t kRecordUp[] = {0x01, 0x0e};
constexpr uint8_t kRecordDown[] = {0x01, 0x0f};
constexpr uint8_t kAfOnUp[] = {0x01, 0x14};
constexpr uint8_t kAfOnDown[] = {0x01, 0x15};

enum class Feedback : uint8_t {
  Unknown,
  FocusLost,
  FocusAcquired,
  ShutterReady,
  ShutterActive,
  RecordingStopped,
  RecordingStarted,
};

Feedback parseFeedback(const uint8_t* data, size_t length);
const char* feedbackName(Feedback feedback);

}  // namespace SonyRemoteProtocol

