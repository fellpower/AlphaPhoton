// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alpha Photon contributors

#include <Arduino.h>
#ifdef ALPHA_PHOTON_CORES3
#include <M5Unified.h>
#else
#include <M5StickCPlus.h>
#endif

#include "SonyBleRemote.h"

SonyBleRemote remote;
#ifdef ALPHA_PHOTON_CORES3
M5Canvas screen(&M5.Display);
#else
TFT_eSprite screen(&M5.Lcd);
#endif
ConnectionState uiState = ConnectionState::Starting;
String uiDetail = "boot";
SonyRemoteProtocol::Feedback uiFeedback = SonyRemoteProtocol::Feedback::Unknown;
bool displayDirty = true;
bool focusHeld = false;
bool recording = false;
uint32_t recordingStartedAt = 0;
uint32_t photoCount = 0;
uint32_t clipCount = 0;
uint32_t lastActivityAt = 0;
bool displayDimmed = false;

enum class FocusState : uint8_t { Idle, Searching, Locked, Lost };
FocusState focusState = FocusState::Idle;

void markActivity();

void setDisplayBrightness(uint8_t brightness) {
#ifdef ALPHA_PHOTON_CORES3
  M5.Display.setBrightness(brightness);
#else
  M5.Axp.ScreenBreath(brightness);
#endif
}

enum class UiMode : uint8_t { Remote, Menu, Setup, Running };
UiMode uiMode = UiMode::Remote;
uint8_t selectedTool = 0;
uint8_t setupField = 0;
uint8_t intervalIndex = 3;
uint8_t countIndex = 2;
uint8_t exposureIndex = 4;
uint8_t pauseIndex = 2;
uint32_t sequenceDone = 0;
uint32_t sequenceTarget = 0;
uint32_t nextActionAt = 0;
bool bulbActive = false;
bool nrWaiting = false;
bool chordActive = false;

constexpr uint16_t kIntervals[] = {1, 2, 5, 10, 30, 60};
constexpr uint16_t kCounts[] = {10, 25, 50, 100, 250};
constexpr uint16_t kExposures[] = {5, 10, 15, 20, 30, 60, 120, 180};
constexpr uint16_t kPauses[] = {1, 2, 3, 5, 10, 30};
const char* const kToolNames[] = {"INTERVAL", "TIMELAPSE", "ASTRO BULB"};

void startSequence() {
  if (!remote.connected()) {
    uiDetail = "camera offline";
    return;
  }
  sequenceDone = 0;
  sequenceTarget = selectedTool == 0 ? 0 : kCounts[countIndex];
  nextActionAt = millis() + 500;
  bulbActive = false;
  nrWaiting = false;
  uiMode = UiMode::Running;
  markActivity();
  displayDirty = true;
}

void stopSequence() {
  if (bulbActive) remote.shutterUp();
  bulbActive = false;
  nrWaiting = false;
  uiMode = UiMode::Menu;
  uiDetail = "sequence stopped";
  markActivity();
  displayDirty = true;
}

void runSequence() {
  if (uiMode != UiMode::Running || !remote.connected() || millis() < nextActionAt) return;

  if (selectedTool == 2) {
    // FF02 normally ends this wait via ShutterReady. This timeout is only a
    // fallback in case a BLE notification gets lost.
    if (nrWaiting) {
      nrWaiting = false;
      if (sequenceDone >= sequenceTarget) {
        uiMode = UiMode::Menu;
        uiDetail = "astro complete";
      } else {
        nextActionAt = millis() + (uint32_t)kPauses[pauseIndex] * 1000;
      }
      displayDirty = true;
      return;
    }
    if (!bulbActive) {
      if (remote.shutterDown()) {
        nrWaiting = false;
        bulbActive = true;
        nextActionAt = millis() + (uint32_t)kExposures[exposureIndex] * 1000;
      }
    } else {
      nrWaiting = true;
      remote.shutterUp();
      bulbActive = false;
      ++sequenceDone;
      ++photoCount;
      nextActionAt = millis() +
                     ((uint32_t)kExposures[exposureIndex] * 3 + 30) * 1000;
    }
  } else {
    if (remote.takePhoto()) {
      ++sequenceDone;
      ++photoCount;
    }
    if (sequenceTarget && sequenceDone >= sequenceTarget) {
      uiMode = UiMode::Menu;
      uiDetail = "sequence complete";
    } else {
      nextActionAt = millis() + (uint32_t)kIntervals[intervalIndex] * 1000;
    }
  }
  markActivity();
  displayDirty = true;
}

void markActivity() {
  lastActivityAt = millis();
  if (displayDimmed) {
    setDisplayBrightness(80);
    displayDimmed = false;
    displayDirty = true;
  }
}

const char* stateName(ConnectionState state) {
  switch (state) {
    case ConnectionState::Starting: return "START";
    case ConnectionState::Scanning: return "SCANNING";
    case ConnectionState::Connecting: return "CONNECT";
    case ConnectionState::Pairing: return "PAIRING";
    case ConnectionState::Discovering: return "DISCOVER";
    case ConnectionState::Ready: return "READY";
    case ConnectionState::Disconnected: return "LOST";
    case ConnectionState::Error: return "ERROR";
    default: return "?";
  }
}

void onState(ConnectionState state, const char* detail) {
  uiState = state;
  uiDetail = detail;
  displayDirty = true;
  if (state == ConnectionState::Ready || state == ConnectionState::Disconnected ||
      state == ConnectionState::Error) markActivity();
}

void onFeedback(SonyRemoteProtocol::Feedback feedback) {
  uiFeedback = feedback;
  switch (feedback) {
    case SonyRemoteProtocol::Feedback::FocusAcquired:
      focusState = FocusState::Locked;
      break;
    case SonyRemoteProtocol::Feedback::FocusLost:
      focusState = focusHeld ? FocusState::Lost : FocusState::Idle;
      break;
    case SonyRemoteProtocol::Feedback::RecordingStarted:
      recording = true;
      recordingStartedAt = millis();
      ++clipCount;
      break;
    case SonyRemoteProtocol::Feedback::RecordingStopped:
      recording = false;
      break;
    case SonyRemoteProtocol::Feedback::ShutterReady:
      if (uiMode == UiMode::Running && selectedTool == 2 && nrWaiting) {
        nrWaiting = false;
        if (sequenceDone >= sequenceTarget) {
          uiMode = UiMode::Menu;
          uiDetail = "astro complete";
        } else {
          nextActionAt = millis() + (uint32_t)kPauses[pauseIndex] * 1000;
        }
      }
      break;
    default:
      break;
  }
  displayDirty = true;
  markActivity();
}

const char* focusName() {
  switch (focusState) {
    case FocusState::Searching: return "SEARCH";
    case FocusState::Locked: return "LOCK";
    case FocusState::Lost: return "NO LOCK";
    default: return "READY";
  }
}

uint8_t batteryPercent() {
#ifdef ALPHA_PHOTON_CORES3
  const int32_t level = M5.Power.getBatteryLevel();
  return static_cast<uint8_t>(level < 0 ? 0 : (level > 100 ? 100 : level));
#else
  const float voltage = M5.Axp.GetBatVoltage();
  if (voltage <= 3.30f) return 0;
  if (voltage >= 4.15f) return 100;
  return static_cast<uint8_t>((voltage - 3.30f) * 100.0f / 0.85f + 0.5f);
#endif
}

#ifdef ALPHA_PHOTON_CORES3
constexpr uint16_t kCoreBackground = 0x0842;  // deep navy
constexpr uint16_t kCoreHeader = 0x10A4;
constexpr uint16_t kCorePanel = 0x18E6;
constexpr uint16_t kCorePanelActive = 0x2148;
constexpr uint16_t kCoreMuted = 0x9CF3;

void drawCoreButton(int x, int y, int w, int h, uint16_t color, const char* title,
                    const char* subtitle = nullptr) {
  screen.fillRoundRect(x, y, w, h, 12, kCorePanel);
  screen.fillRoundRect(x + 10, y + h - 5, w - 20, 3, 2, color);
  screen.setTextColor(TFT_WHITE, kCorePanel);
  screen.setTextSize(2);
  screen.setCursor(x + 14, y + 13);
  screen.print(title);
  if (subtitle) {
    screen.setTextColor(kCoreMuted, kCorePanel);
    screen.setTextSize(1);
    screen.setCursor(x + 14, y + h - 19);
    screen.print(subtitle);
  }
}

void drawStatusCoreS3() {
  displayDirty = false;
  screen.fillSprite(kCoreBackground);

  const bool ready = uiState == ConnectionState::Ready;
  const uint8_t battery = batteryPercent();
  screen.fillRect(0, 0, 320, 32, kCoreHeader);
  screen.setTextColor(TFT_CYAN, kCoreHeader);
  screen.setTextSize(1);
  screen.setCursor(10, 11);
  screen.print("ALPHA PHOTON");
  screen.fillCircle(106, 15, 5, ready ? TFT_GREEN : TFT_ORANGE);
  screen.setTextColor(TFT_WHITE, kCoreHeader);
  screen.setTextSize(1);
  screen.setCursor(116, 11);
  screen.print(ready ? "CAM READY" : stateName(uiState));
  screen.setCursor(263, 11);
  screen.printf("%u%%", battery);
  screen.drawRoundRect(240, 9, 18, 11, 2, TFT_WHITE);
  screen.fillRect(258, 12, 2, 5, TFT_WHITE);
  screen.fillRect(243, 12, (battery * 12) / 100, 5, battery <= 15 ? TFT_RED : TFT_GREEN);

  if (uiMode == UiMode::Remote) {
    const uint16_t recColor = recording ? TFT_RED : (ready ? TFT_GREEN : TFT_ORANGE);
    const uint16_t videoBackground = recording ? 0x6000 : kCorePanelActive;
    screen.fillRoundRect(8, 39, 160, 114, 16, videoBackground);
    screen.setTextColor(kCoreMuted, videoBackground);
    screen.setTextSize(1);
    screen.setCursor(20, 51);
    screen.print(recording ? "RECORDING" : "VIDEO CONTROL");
    screen.drawCircle(56, 97, 25, recColor);
    screen.drawCircle(56, 97, 24, recColor);
    screen.fillCircle(56, 97, recording ? 12 : 9, recording ? TFT_RED : recColor);
    screen.setTextColor(TFT_WHITE, videoBackground);
    screen.setTextSize(2);
    screen.setCursor(94, 72);
    screen.print(recording ? "STOP" : "START");
    if (recording) {
      const uint32_t seconds = (millis() - recordingStartedAt) / 1000;
      screen.setTextColor(TFT_WHITE, videoBackground);
      screen.setTextSize(1);
      screen.setCursor(94, 104);
      screen.printf("%02lu:%02lu:%02lu", (unsigned long)(seconds / 3600),
                    (unsigned long)((seconds / 60) % 60),
                    (unsigned long)(seconds % 60));
    } else {
      screen.setTextColor(kCoreMuted, videoBackground);
      screen.setTextSize(1);
      screen.setCursor(94, 104);
      screen.printf("%lu CLIPS", (unsigned long)clipCount);
    }
    screen.setTextColor(kCoreMuted, videoBackground);
    screen.setTextSize(1);
    screen.setCursor(20, 134);
    screen.print("TAP TO TOGGLE");

    screen.fillRoundRect(176, 39, 136, 54, 14, 0x03CC);
    screen.drawRect(190, 56, 22, 15, TFT_WHITE);
    screen.fillRect(196, 53, 9, 3, TFT_WHITE);
    screen.drawCircle(201, 63, 4, TFT_WHITE);
    screen.setTextColor(TFT_WHITE, 0x03CC);
    screen.setTextSize(2);
    screen.setCursor(222, 49);
    screen.print("PHOTO");
    screen.setTextSize(1);
    screen.setCursor(223, 73);
    screen.printf("%lu  PHOTO MODE", (unsigned long)photoCount);

    screen.fillRoundRect(176, 101, 136, 52, 14, kCorePanel);
    screen.setTextColor(TFT_WHITE, kCorePanel);
    screen.setTextSize(2);
    screen.setCursor(190, 111);
    screen.print("TOOLS");
    screen.setTextColor(TFT_CYAN, kCorePanel);
    screen.setCursor(282, 111);
    screen.print(">");
    screen.setTextColor(kCoreMuted, kCorePanel);
    screen.setTextSize(1);
    screen.setCursor(190, 136);
    screen.print("SEQUENCES");

    const uint16_t afColor = focusState == FocusState::Locked ? TFT_GREEN :
                             (focusState == FocusState::Lost ? TFT_RED : TFT_CYAN);
    screen.fillRoundRect(8, 161, 304, 70, 15, kCorePanel);
    screen.fillRoundRect(18, 226, 284, 3, 2, afColor);
    screen.setTextColor(TFT_WHITE, kCorePanel);
    screen.setTextSize(3);
    screen.setCursor(21, 177);
    screen.print("AF");
    screen.setTextColor(afColor, kCorePanel);
    screen.setTextSize(2);
    screen.setCursor(82, 179);
    screen.print(focusName());
    screen.setTextColor(kCoreMuted, kCorePanel);
    screen.setTextSize(1);
    screen.setCursor(82, 205);
    screen.print("PRESS + HOLD TO FOCUS");
    screen.drawCircle(276, 194, 20, afColor);
    screen.drawFastHLine(262, 194, 28, afColor);
    screen.drawFastVLine(276, 180, 28, afColor);
  } else if (uiMode == UiMode::Menu) {
    for (uint8_t i = 0; i < 3; ++i) {
      const int y = 37 + i * 51;
      const uint16_t color = i == selectedTool ? TFT_CYAN : TFT_DARKGREY;
      screen.fillRoundRect(8, y, 304, 47, 10,
                           i == selectedTool ? kCorePanelActive : kCorePanel);
      screen.drawRoundRect(8, y, 304, 47, 10, color);
      screen.fillRoundRect(13, y + 8, 4, 31, 2, color);
      screen.setTextColor(TFT_WHITE, i == selectedTool ? kCorePanelActive : kCorePanel);
      screen.setTextSize(2);
      screen.setCursor(26, y + 14);
      screen.print(kToolNames[i]);
      screen.setTextColor(kCoreMuted, i == selectedTool ? kCorePanelActive : kCorePanel);
      screen.setTextSize(1);
      screen.setCursor(252, y + 19);
      screen.print("SET  >");
    }
    drawCoreButton(8, 192, 304, 39, TFT_LIGHTGREY, "BACK");
  } else if (uiMode == UiMode::Setup) {
    screen.setTextColor(TFT_CYAN, kCoreBackground);
    screen.setTextSize(2);
    screen.setCursor(10, 42);
    screen.print(kToolNames[selectedTool]);
    const uint8_t fields = selectedTool == 0 ? 1 : (selectedTool == 1 ? 2 : 3);
    const char* labels[] = {"INTERVAL", "PHOTOS", "PAUSE"};
    for (uint8_t i = 0; i < fields; ++i) {
      const int w = fields == 1 ? 304 : (fields == 2 ? 148 : 97);
      const int x = 8 + i * (w + (fields == 2 ? 8 : 5));
      screen.fillRoundRect(x, 75, w, 100, 10,
                           i == setupField ? kCorePanelActive : kCorePanel);
      screen.drawRoundRect(x, 75, w, 100, 10, i == setupField ? TFT_CYAN : TFT_DARKGREY);
      const char* label = labels[i];
      uint16_t value = 0;
      const char* suffix = "";
      if (selectedTool == 0) { label = "INTERVAL"; value = kIntervals[intervalIndex]; suffix = " s"; }
      else if (selectedTool == 1) {
        label = i == 0 ? "INTERVAL" : "PHOTOS";
        value = i == 0 ? kIntervals[intervalIndex] : kCounts[countIndex];
        suffix = i == 0 ? " s" : "";
      } else {
        const char* astroLabels[] = {"EXPOSURE", "PAUSE", "PHOTOS"};
        label = astroLabels[i];
        value = i == 0 ? kExposures[exposureIndex] :
                (i == 1 ? kPauses[pauseIndex] : kCounts[countIndex]);
        suffix = i < 2 ? " s" : "";
      }
      const uint16_t fieldBackground = i == setupField ? kCorePanelActive : kCorePanel;
      screen.setTextColor(kCoreMuted, fieldBackground);
      screen.setTextSize(1);
      screen.setCursor(x + 10, 91);
      screen.print(label);
      screen.setTextColor(TFT_WHITE, fieldBackground);
      screen.setTextSize(3);
      screen.setCursor(x + 12, 122);
      screen.printf("%u%s", value, suffix);
    }
    drawCoreButton(8, 190, 92, 41, TFT_LIGHTGREY, "BACK");
    drawCoreButton(108, 190, 96, 41, TFT_CYAN, "CHANGE");
    drawCoreButton(212, 190, 100, 41, TFT_GREEN,
                   setupField + 1 >= fields ? "START" : "NEXT");
  } else {
    const uint32_t remaining = nextActionAt > millis() ?
                               (nextActionAt - millis() + 999) / 1000 : 0;
    const uint16_t color = bulbActive ? TFT_RED : TFT_GREEN;
    screen.fillRoundRect(8, 42, 304, 138, 14, kCorePanel);
    screen.drawRoundRect(8, 42, 304, 138, 14, color);
    screen.setTextColor(color, kCorePanel);
    screen.setTextSize(3);
    screen.setCursor(14, 55);
    screen.print(bulbActive ? "EXPOSING" : (nrWaiting ? "PROCESSING" : "NEXT PHOTO"));
    screen.setTextColor(TFT_WHITE, kCorePanel);
    screen.setTextSize(5);
    screen.setCursor(120, 105);
    screen.printf("%lu", (unsigned long)remaining);
    screen.setTextSize(2);
    screen.setCursor(15, 153);
    if (sequenceTarget) screen.printf("%lu / %lu", (unsigned long)sequenceDone,
                                      (unsigned long)sequenceTarget);
    else screen.printf("%lu / --", (unsigned long)sequenceDone);
    drawCoreButton(8, 190, 304, 41, TFT_RED, "STOP");
  }
  screen.pushSprite(0, 0);
}
#endif

void drawStatus() {
#ifdef ALPHA_PHOTON_CORES3
  drawStatusCoreS3();
  return;
#endif
  displayDirty = false;
  screen.fillSprite(TFT_BLACK);

  if (uiMode != UiMode::Remote) {
    screen.setTextColor(TFT_CYAN, TFT_BLACK);
    screen.setTextSize(2);
    screen.setCursor(8, 9);
    screen.print(uiMode == UiMode::Menu ? "TOOLS" : kToolNames[selectedTool]);
    screen.drawFastHLine(7, 34, 121, TFT_DARKGREY);

    if (uiMode == UiMode::Menu) {
      for (uint8_t i = 0; i < 3; ++i) {
        const int y = 52 + i * 45;
        if (i == selectedTool) screen.fillRoundRect(5, y - 8, 125, 34, 6, TFT_DARKCYAN);
        screen.setTextColor(i == selectedTool ? TFT_WHITE : TFT_LIGHTGREY,
                            i == selectedTool ? TFT_DARKCYAN : TFT_BLACK);
        screen.setTextSize(2);
        screen.setCursor(12, y);
        screen.print(kToolNames[i]);
      }
      screen.setTextSize(1);
      screen.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      screen.setCursor(8, 205);
      screen.print("B NEXT   A SELECT");
      screen.setCursor(8, 224);
      screen.print("PWR BACK");
    } else if (uiMode == UiMode::Setup) {
      screen.setTextSize(1);
      screen.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      screen.setCursor(8, 48);
      if (selectedTool == 0) {
        screen.print("INTERVAL");
        screen.setTextSize(3);
        screen.setTextColor(TFT_WHITE, TFT_BLACK);
        screen.setCursor(18, 78);
        screen.printf("%u s", kIntervals[intervalIndex]);
      } else if (selectedTool == 1) {
        screen.print(setupField == 0 ? "> INTERVAL" : "  INTERVAL");
        screen.setTextSize(2);
        screen.setTextColor(TFT_WHITE, TFT_BLACK);
        screen.setCursor(18, 70);
        screen.printf("%u sec", kIntervals[intervalIndex]);
        screen.setTextSize(1);
        screen.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        screen.setCursor(8, 112);
        screen.print(setupField == 1 ? "> PHOTOS" : "  PHOTOS");
        screen.setTextSize(2);
        screen.setTextColor(TFT_WHITE, TFT_BLACK);
        screen.setCursor(18, 134);
        screen.printf("%u", kCounts[countIndex]);
      } else {
        const char* labels[] = {"EXPOSURE", "PAUSE", "PHOTOS"};
        const uint16_t values[] = {kExposures[exposureIndex], kPauses[pauseIndex], kCounts[countIndex]};
        for (uint8_t i = 0; i < 3; ++i) {
          const int y = 51 + i * 45;
          screen.setTextSize(1);
          screen.setTextColor(i == setupField ? TFT_CYAN : TFT_LIGHTGREY, TFT_BLACK);
          screen.setCursor(8, y);
          screen.printf("%c %s", i == setupField ? '>' : ' ', labels[i]);
          screen.setTextSize(2);
          screen.setTextColor(TFT_WHITE, TFT_BLACK);
          screen.setCursor(18, y + 15);
          screen.printf("%u%s", values[i], i < 2 ? " sec" : "");
        }
      }
      screen.setTextSize(1);
      screen.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      screen.setCursor(8, 205);
      screen.print("B CHANGE  A NEXT/GO");
      screen.setCursor(8, 224);
      screen.print("PWR BACK");
    } else {
      const uint32_t remaining = nextActionAt > millis() ? (nextActionAt - millis() + 999) / 1000 : 0;
      screen.setTextColor(bulbActive ? TFT_RED : TFT_GREEN, TFT_BLACK);
      screen.setTextSize(2);
      screen.setCursor(10, 55);
      screen.print(bulbActive ? "EXPOSING" : (nrWaiting ? "PROCESSING" : "NEXT PHOTO"));
      screen.setTextSize(4);
      screen.setCursor(28, 88);
      screen.printf("%lu", (unsigned long)remaining);
      screen.setTextSize(2);
      screen.setCursor(8, 145);
      if (sequenceTarget) screen.printf("%lu / %lu", (unsigned long)sequenceDone,
                                        (unsigned long)sequenceTarget);
      else screen.printf("%lu / --", (unsigned long)sequenceDone);
      screen.setTextSize(1);
      screen.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      screen.setCursor(8, 185);
      screen.print(selectedTool == 2 ? "CAMERA: M + BULB" : "CAMERA: PHOTO MODE");
      screen.setCursor(8, 224);
      screen.print("A  STOP");
    }
    screen.pushSprite(0, 0);
    return;
  }

  const bool ready = uiState == ConnectionState::Ready;
  const uint8_t battery = batteryPercent();

  // Compact status bar: camera link and a graphical battery gauge.
  screen.fillCircle(10, 13, 5, ready ? TFT_GREEN : TFT_ORANGE);
  screen.setTextSize(1);
  screen.setTextColor(TFT_WHITE, TFT_BLACK);
  screen.setCursor(20, 10);
  screen.print(ready ? "CAM" : "LINK...");
  screen.drawRoundRect(91, 6, 31, 15, 2, TFT_WHITE);
  screen.fillRect(122, 10, 3, 7, TFT_WHITE);
  const uint16_t batteryColor = battery <= 15 ? TFT_RED : TFT_GREEN;
  screen.fillRect(94, 9, (battery * 25) / 100, 9, batteryColor);
  screen.setCursor(68, 10);
  screen.printf("%u", battery);

  // Main recording card.
  screen.drawRoundRect(5, 30, 125, 76, 8, recording ? TFT_RED : TFT_DARKGREY);
  if (recording) {
    screen.fillCircle(22, 50, 9, TFT_RED);
    screen.setTextColor(TFT_RED, TFT_BLACK);
    screen.setTextSize(3);
    screen.setCursor(40, 39);
    screen.print("REC");
    const uint32_t seconds = (millis() - recordingStartedAt) / 1000;
    screen.setTextColor(TFT_WHITE, TFT_BLACK);
    screen.setTextSize(2);
    screen.setCursor(18, 76);
    screen.printf("%02lu:%02lu:%02lu", (unsigned long)(seconds / 3600),
                  (unsigned long)((seconds / 60) % 60), (unsigned long)(seconds % 60));
  } else {
    screen.drawCircle(22, 51, 9, ready ? TFT_GREEN : TFT_ORANGE);
    screen.setTextColor(ready ? TFT_GREEN : TFT_ORANGE, TFT_BLACK);
    screen.setTextSize(2);
    screen.setCursor(40, 42);
    screen.print(ready ? "READY" : "CONNECT");
    screen.setTextSize(1);
    screen.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    screen.setCursor(18, 78);
    screen.print("A  START / STOP");
  }

  // Large autofocus card.
  const uint16_t afColor = focusState == FocusState::Locked ? TFT_GREEN :
                           (focusState == FocusState::Lost ? TFT_RED : TFT_CYAN);
  screen.drawRoundRect(5, 113, 125, 55, 8, afColor);
  screen.setTextColor(TFT_WHITE, TFT_BLACK);
  screen.setTextSize(2);
  screen.setCursor(14, 124);
  screen.print("AF");
  screen.setTextColor(afColor, TFT_BLACK);
  screen.setCursor(48, 124);
  screen.print(focusName());
  screen.setTextSize(1);
  screen.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  screen.setCursor(14, 151);
  screen.print("B  HOLD TO FOCUS");

  // Session counters with simple photo and video icons.
  screen.drawRoundRect(5, 176, 59, 47, 7, TFT_DARKGREY);
  screen.drawRect(13, 187, 19, 13, TFT_WHITE);
  screen.fillRect(17, 184, 8, 3, TFT_WHITE);
  screen.drawCircle(22, 193, 4, TFT_WHITE);
  screen.setTextColor(TFT_WHITE, TFT_BLACK);
  screen.setTextSize(2);
  screen.setCursor(37, 184);
  screen.printf("%lu", (unsigned long)photoCount);
  screen.setTextSize(1);
  screen.setCursor(13, 207);
  screen.print("PHOTO");

  screen.drawRoundRect(70, 176, 60, 47, 7, TFT_DARKGREY);
  screen.drawRect(78, 187, 17, 13, TFT_WHITE);
  screen.fillTriangle(96, 188, 104, 193, 96, 199, TFT_WHITE);
  screen.setTextSize(2);
  screen.setCursor(108, 184);
  screen.printf("%lu", (unsigned long)clipCount);
  screen.setTextSize(1);
  screen.setCursor(78, 207);
  screen.print("CLIP");

  screen.setTextColor(TFT_DARKGREY, TFT_BLACK);
  screen.setCursor(7, 230);
  screen.print("HOLD A: TOOLS");
  screen.pushSprite(0, 0);
}

void printHelp() {
  Serial.println("Commands: f=focus down, u=release, s=photo, r=record toggle, x=clear bonds, ?=help");
}

void handleSerial() {
  while (Serial.available()) {
    switch (Serial.read()) {
      case 'f': remote.focusDown(); break;
      case 'u': remote.releaseAll(); break;
      case 's': remote.takePhoto(); break;
      case 'r': remote.toggleRecord(); break;
      case 'x': remote.clearBonds(); break;
      case '?': printHelp(); break;
      default: break;
    }
  }
}

#ifdef ALPHA_PHOTON_CORES3
enum class TouchAction : uint8_t { None, Video, Photo, Focus, Tools, Back, Change, Next, Tool0, Tool1, Tool2, Stop };
TouchAction touchAction = TouchAction::None;

void wakeDisplayOnMotion() {
  static bool initialized = false;
  static float previousX = 0.0f;
  static float previousY = 0.0f;
  static float previousZ = 0.0f;

  if (!M5.Imu.update()) return;
  const auto data = M5.Imu.getImuData();
  if (initialized && displayDimmed) {
    const float accelerationDelta = fabsf(data.accel.x - previousX) +
                                    fabsf(data.accel.y - previousY) +
                                    fabsf(data.accel.z - previousZ);
    const float rotation = fabsf(data.gyro.x) + fabsf(data.gyro.y) + fabsf(data.gyro.z);
    if (accelerationDelta > 0.12f || rotation > 18.0f) {
      Serial.printf("[UI] motion wake accel=%.2f gyro=%.1f\n", accelerationDelta, rotation);
      markActivity();
    }
  }
  previousX = data.accel.x;
  previousY = data.accel.y;
  previousZ = data.accel.z;
  initialized = true;
}

void changeSetupValue() {
  if (selectedTool < 2 && setupField == 0) intervalIndex = (intervalIndex + 1) % 6;
  else if (selectedTool == 1 || (selectedTool == 2 && setupField == 2)) countIndex = (countIndex + 1) % 5;
  else if (selectedTool == 2 && setupField == 0) exposureIndex = (exposureIndex + 1) % 8;
  else if (selectedTool == 2 && setupField == 1) pauseIndex = (pauseIndex + 1) % 6;
}

void handleCoreTouch() {
  const auto touch = M5.Touch.getDetail();
  if (touch.wasPressed()) {
    markActivity();
    if (uiMode == UiMode::Remote) {
      if (touch.y >= 39 && touch.y < 156) {
        if (touch.x < 172) touchAction = TouchAction::Video;
        else touchAction = touch.y < 97 ? TouchAction::Photo : TouchAction::Tools;
      } else if (touch.y >= 158) {
        touchAction = TouchAction::Focus;
        focusHeld = remote.focusDown();
        focusState = FocusState::Searching;
        uiDetail = "AF held";
        displayDirty = true;
      }
    } else if (uiMode == UiMode::Menu) {
      if (touch.y >= 190) touchAction = TouchAction::Back;
      else if (touch.y >= 34) {
        const uint8_t tool = min<uint8_t>(2, (touch.y - 34) / 52);
        touchAction = static_cast<TouchAction>(static_cast<uint8_t>(TouchAction::Tool0) + tool);
      }
    } else if (uiMode == UiMode::Setup) {
      if (touch.y >= 190) {
        touchAction = touch.x < 104 ? TouchAction::Back :
                      (touch.x < 208 ? TouchAction::Change : TouchAction::Next);
      } else if (touch.y >= 75 && touch.y <= 180) {
        const uint8_t fields = selectedTool == 0 ? 1 : (selectedTool == 1 ? 2 : 3);
        setupField = min<uint8_t>(fields - 1, (touch.x * fields) / 320);
        touchAction = TouchAction::Change;
        displayDirty = true;
      }
    } else if (uiMode == UiMode::Running) {
      touchAction = TouchAction::Stop;
    }
  }

  if (!touch.wasReleased()) return;
  markActivity();
  switch (touchAction) {
    case TouchAction::Video:
      remote.toggleRecord();
      uiDetail = "record toggle";
      break;
    case TouchAction::Photo:
      if (remote.takePhoto()) ++photoCount;
      uiDetail = "photo";
      break;
    case TouchAction::Focus:
      if (focusHeld) remote.releaseAll();
      focusHeld = false;
      focusState = FocusState::Idle;
      uiDetail = "AF released";
      break;
    case TouchAction::Tools:
      uiMode = UiMode::Menu;
      break;
    case TouchAction::Back:
      uiMode = uiMode == UiMode::Setup ? UiMode::Menu : UiMode::Remote;
      break;
    case TouchAction::Change:
      changeSetupValue();
      break;
    case TouchAction::Next: {
      const uint8_t lastField = selectedTool == 0 ? 0 : (selectedTool == 1 ? 1 : 2);
      if (setupField >= lastField) startSequence();
      else ++setupField;
      break;
    }
    case TouchAction::Tool0:
    case TouchAction::Tool1:
    case TouchAction::Tool2:
      selectedTool = static_cast<uint8_t>(touchAction) - static_cast<uint8_t>(TouchAction::Tool0);
      setupField = 0;
      uiMode = UiMode::Setup;
      break;
    case TouchAction::Stop:
      stopSequence();
      break;
    default:
      break;
  }
  touchAction = TouchAction::None;
  displayDirty = true;
}
#endif

void setup() {
  Serial.begin(115200);
  delay(300);
#ifdef ALPHA_PHOTON_CORES3
  auto cfg = M5.config();
  cfg.clear_display = true;
  cfg.output_power = true;
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.setTextWrap(false);
  screen.setColorDepth(16);
  screen.createSprite(M5.Display.width(), M5.Display.height());
  screen.setTextWrap(false);
  setDisplayBrightness(100);
#else
  M5.begin(true, true, true);
  M5.Lcd.setRotation(2);
  M5.Lcd.setTextWrap(false);
  screen.setColorDepth(16);
  screen.createSprite(M5.Lcd.width(), M5.Lcd.height());
  screen.setTextWrap(false);
  setDisplayBrightness(80);
#endif
  lastActivityAt = millis();
  Serial.println("\nAlpha Photon firmware");
  printHelp();

  remote.begin(onState, onFeedback);
}

void loop() {
  M5.update();
#ifdef ALPHA_PHOTON_CORES3
  wakeDisplayOnMotion();
#endif
  remote.loop();
  handleSerial();
  runSequence();

#ifdef ALPHA_PHOTON_CORES3
  handleCoreTouch();
#else
  const bool powerPressed = M5.Axp.GetBtnPress() == 0x02;
  if (uiMode == UiMode::Remote && !chordActive && M5.BtnA.pressedFor(1200)) {
    if (focusHeld) remote.releaseAll();
    focusHeld = false;
    focusState = FocusState::Idle;
    chordActive = true;
    uiMode = UiMode::Menu;
    markActivity();
    displayDirty = true;
  }
  if (uiMode == UiMode::Remote && !chordActive && M5.BtnA.pressedFor(800) && M5.BtnB.pressedFor(800)) {
    if (focusHeld) remote.releaseAll();
    focusHeld = false;
    focusState = FocusState::Idle;
    chordActive = true;
    uiMode = UiMode::Menu;
    markActivity();
    displayDirty = true;
  }

  if (chordActive) {
    if (!M5.BtnA.isPressed() && !M5.BtnB.isPressed()) chordActive = false;
  } else if (uiMode == UiMode::Remote) {
    if (M5.BtnA.wasReleased()) {
      markActivity();
      remote.toggleRecord();
      uiDetail = "record toggle";
      displayDirty = true;
    }
    if (M5.BtnB.wasPressed()) {
      markActivity();
      focusHeld = remote.focusDown();
      focusState = FocusState::Searching;
      uiDetail = "AF held";
      displayDirty = true;
    }
    if (M5.BtnB.wasReleased() && focusHeld) {
      markActivity();
      remote.releaseAll();
      focusHeld = false;
      focusState = FocusState::Idle;
      uiDetail = "AF released";
      displayDirty = true;
    }
    if (powerPressed) {
      markActivity();
      if (remote.takePhoto()) ++photoCount;
      uiDetail = "photo";
      displayDirty = true;
    }
  } else if (uiMode == UiMode::Menu) {
    if (M5.BtnB.wasReleased()) {
      selectedTool = (selectedTool + 1) % 3;
      markActivity();
      displayDirty = true;
    }
    if (M5.BtnA.wasReleased()) {
      setupField = 0;
      uiMode = UiMode::Setup;
      markActivity();
      displayDirty = true;
    }
    if (powerPressed) {
      uiMode = UiMode::Remote;
      markActivity();
      displayDirty = true;
    }
  } else if (uiMode == UiMode::Setup) {
    if (M5.BtnB.wasReleased()) {
      if (selectedTool < 2 && setupField == 0) intervalIndex = (intervalIndex + 1) % 6;
      else if (selectedTool == 1 || (selectedTool == 2 && setupField == 2)) countIndex = (countIndex + 1) % 5;
      else if (selectedTool == 2 && setupField == 0) exposureIndex = (exposureIndex + 1) % 8;
      else if (selectedTool == 2 && setupField == 1) pauseIndex = (pauseIndex + 1) % 6;
      markActivity();
      displayDirty = true;
    }
    if (M5.BtnA.wasReleased()) {
      const uint8_t lastField = selectedTool == 0 ? 0 : (selectedTool == 1 ? 1 : 2);
      if (setupField >= lastField) startSequence();
      else ++setupField;
      markActivity();
      displayDirty = true;
    }
    if (powerPressed) {
      uiMode = UiMode::Menu;
      markActivity();
      displayDirty = true;
    }
  } else if (uiMode == UiMode::Running) {
    if (M5.BtnA.wasReleased() || powerPressed) stopSequence();
  }
#endif

  static uint32_t lastRefresh = 0;
  const uint32_t refreshInterval = (recording || uiMode == UiMode::Running) ? 250 : 1000;
  if (displayDirty || millis() - lastRefresh > refreshInterval) {
    lastRefresh = millis();
    drawStatus();
  }

  if (!recording && uiMode != UiMode::Running && !displayDimmed && millis() - lastActivityAt >= 20000) {
    setDisplayBrightness(25);
    displayDimmed = true;
  }
  delay(5);
}
