#pragma once
#include <Arduino.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLERemoteCharacteristic.h>
#include <BLESecurity.h>
#include "SonyRemoteProtocol.h"

enum class ConnectionState : uint8_t { Starting, Scanning, Connecting, Pairing, Discovering, Ready, Disconnected, Error };

class SonyBleRemote : public BLEClientCallbacks, public BLEAdvertisedDeviceCallbacks, public BLESecurityCallbacks {
 public:
  using StateCallback = void (*)(ConnectionState, const char*);
  using FeedbackCallback = void (*)(SonyRemoteProtocol::Feedback);
  void begin(StateCallback, FeedbackCallback);
  void loop();
  bool connected() const;
  ConnectionState state() const { return state_; }
  int rssi() const;
  bool focusDown();
  bool releaseAll();
  bool takePhoto();
  bool shutterDown();
  bool shutterUp();
  bool toggleRecord();
  void clearBonds();

 private:
  void setState(ConnectionState, const char* = "");
  void startScan();
  bool connectCamera();
  bool isSonyCamera(BLEAdvertisedDevice&) const;
  bool isPairingAdvertisement(BLEAdvertisedDevice&) const;
  bool send(const uint8_t*, size_t, const char*);
  void onResult(BLEAdvertisedDevice) override;
  void onConnect(BLEClient*) override;
  void onDisconnect(BLEClient*) override;
  uint32_t onPassKeyRequest() override;
  void onPassKeyNotify(uint32_t) override;
  bool onSecurityRequest() override;
  void onAuthenticationComplete(esp_ble_auth_cmpl_t) override;
  bool onConfirmPIN(uint32_t) override;
  static void notifyCallback(BLERemoteCharacteristic*, uint8_t*, size_t, bool);
  static SonyBleRemote* instance_;
  BLEAddress* candidate_ = nullptr;
  BLEClient* client_ = nullptr;
  BLERemoteCharacteristic* command_ = nullptr;
  StateCallback stateCallback_ = nullptr;
  FeedbackCallback feedbackCallback_ = nullptr;
  ConnectionState state_ = ConnectionState::Starting;
  bool connectPending_ = false;
  bool authenticated_ = false;
  uint32_t connectAt_ = 0;
  uint32_t reconnectAt_ = 0;
};
