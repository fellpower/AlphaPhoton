#include <Arduino.h>
#include <M5StickCPlus.h>

#include "SonyBleRemote.h"

SonyBleRemote remote;
TFT_eSprite screen(&M5.Lcd);
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
    M5.Axp.ScreenBreath(80);
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
  const float voltage = M5.Axp.GetBatVoltage();
  if (voltage <= 3.30f) return 0;
  if (voltage >= 4.15f) return 100;
  return static_cast<uint8_t>((voltage - 3.30f) * 100.0f / 0.85f + 0.5f);
}

void drawStatus() {
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

void setup() {
  Serial.begin(115200);
  delay(300);
  M5.begin(true, true, true);
  M5.Lcd.setRotation(2);
  M5.Lcd.setTextWrap(false);
  screen.setColorDepth(16);
  screen.createSprite(M5.Lcd.width(), M5.Lcd.height());
  screen.setTextWrap(false);
  M5.Axp.ScreenBreath(80);
  lastActivityAt = millis();
  Serial.println("\nAlpha Photon firmware");
  printHelp();

  remote.begin(onState, onFeedback);
}

void loop() {
  M5.update();
  remote.loop();
  handleSerial();
  runSequence();

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

  static uint32_t lastRefresh = 0;
  const uint32_t refreshInterval = (recording || uiMode == UiMode::Running) ? 250 : 1000;
  if (displayDirty || millis() - lastRefresh > refreshInterval) {
    lastRefresh = millis();
    drawStatus();
  }

  if (!recording && uiMode != UiMode::Running && !displayDimmed && millis() - lastActivityAt >= 20000) {
    M5.Axp.ScreenBreath(25);
    displayDimmed = true;
  }
  delay(5);
}
