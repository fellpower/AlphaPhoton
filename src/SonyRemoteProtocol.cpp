#include "SonyRemoteProtocol.h"

namespace SonyRemoteProtocol {

Feedback parseFeedback(const uint8_t* data, size_t length) {
  if (length != 3 || data[0] != 0x02) return Feedback::Unknown;

  switch (data[1]) {
    case 0x3f:
      return data[2] == 0x20 ? Feedback::FocusAcquired : Feedback::FocusLost;
    case 0xa0:
      return data[2] == 0x20 ? Feedback::ShutterActive : Feedback::ShutterReady;
    case 0xd5:
      return data[2] == 0x20 ? Feedback::RecordingStarted : Feedback::RecordingStopped;
    default:
      return Feedback::Unknown;
  }
}

const char* feedbackName(Feedback feedback) {
  switch (feedback) {
    case Feedback::FocusLost: return "focus lost";
    case Feedback::FocusAcquired: return "focus acquired";
    case Feedback::ShutterReady: return "shutter ready";
    case Feedback::ShutterActive: return "shutter active";
    case Feedback::RecordingStopped: return "recording stopped";
    case Feedback::RecordingStarted: return "recording started";
    default: return "unknown";
  }
}

}  // namespace SonyRemoteProtocol

