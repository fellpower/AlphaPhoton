// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alpha Photon contributors

#include "SonyBleRemote.h"
#include <BLEScan.h>
#include <BLERemoteService.h>
#include <esp_gap_ble_api.h>
#include <cstring>

SonyBleRemote* SonyBleRemote::instance_ = nullptr;

namespace {
bool isBonded(const BLEAddress& address) {
  int count = esp_ble_get_bond_device_num();
  if (count <= 0) return false;

  auto* bonds = new esp_ble_bond_dev_t[count];
  int returned = count;
  esp_ble_get_bond_device_list(&returned, bonds);
  bool found = false;
  for (int i = 0; i < returned; ++i) {
    if (std::memcmp(bonds[i].bd_addr,
                    const_cast<BLEAddress&>(address).getNative(),
                    ESP_BD_ADDR_LEN) == 0) {
      found = true;
      break;
    }
  }
  delete[] bonds;
  return found;
}
}  // namespace

void SonyBleRemote::begin(StateCallback stateCb, FeedbackCallback feedbackCb) {
  instance_ = this; stateCallback_ = stateCb; feedbackCallback_ = feedbackCb;
  setState(ConnectionState::Starting, "Bluedroid init");
  BLEDevice::init("ALPHA PHOTON");
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  BLEDevice::setSecurityCallbacks(this);
  startScan();
}

void SonyBleRemote::loop() {
  if (connectPending_ && millis() >= connectAt_) { connectPending_ = false; connectCamera(); }
  if (!connected() && state_ == ConnectionState::Disconnected && millis() >= reconnectAt_) startScan();
}
bool SonyBleRemote::connected() const { return client_ && client_->isConnected() && command_; }
int SonyBleRemote::rssi() const { return client_ && client_->isConnected() ? client_->getRssi() : 0; }
void SonyBleRemote::setState(ConnectionState state, const char* detail) {
  state_ = state; Serial.printf("[BLE] state=%u %s\n", (unsigned)state, detail);
  if (stateCallback_) stateCallback_(state, detail);
}
void SonyBleRemote::startScan() {
  if (candidate_) { delete candidate_; candidate_ = nullptr; }
  command_ = nullptr;
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(this, true);
  scan->setActiveScan(true); scan->setInterval(100); scan->setWindow(99);
  setState(ConnectionState::Scanning, "Sony camera");
  scan->start(0, nullptr, false);
}
bool SonyBleRemote::isSonyCamera(BLEAdvertisedDevice& d) const {
  if (!d.haveManufacturerData()) return false;
  auto data = d.getManufacturerData();
  return data.size() >= 4 && (uint8_t)data[0] == 0x2d && (uint8_t)data[1] == 0x01 &&
         (uint8_t)data[2] == 0x03 && (uint8_t)data[3] == 0x00;
}
bool SonyBleRemote::isPairingAdvertisement(BLEAdvertisedDevice& d) const {
  auto data = d.getManufacturerData();
  for (size_t i = 0; i + 1 < data.size(); ++i)
    if ((uint8_t)data[i] == 0x22) return ((uint8_t)data[i + 1] & 0xc0) == 0xc0;
  return false;
}
void SonyBleRemote::onResult(BLEAdvertisedDevice d) {
  if (!isSonyCamera(d) || candidate_) return;
  bool pairing = isPairingAdvertisement(d);
  bool bonded = isBonded(d.getAddress());
  Serial.printf("[SCAN] Sony %s RSSI=%d pairing=%s name=%s\n", d.getAddress().toString().c_str(),
                d.getRSSI(), pairing ? "yes" : "no", d.getName().c_str());
  if (!pairing && !bonded) return;
  Serial.printf("[SCAN] selected %s Sony camera\n", bonded ? "bonded" : "pairing");
  candidate_ = new BLEAddress(d.getAddress());
  BLEDevice::getScan()->stop(); connectAt_ = millis() + 250; connectPending_ = true;
}
bool SonyBleRemote::connectCamera() {
  if (!candidate_) return false;
  setState(ConnectionState::Connecting, candidate_->toString().c_str()); authenticated_ = false;
  client_ = BLEDevice::createClient(); client_->setClientCallbacks(this);
  if (!client_->connect(*candidate_)) {
    setState(ConnectionState::Disconnected, "connect failed"); reconnectAt_ = millis() + 1500; return false;
  }
  setState(ConnectionState::Pairing, "camera security");
  setState(ConnectionState::Discovering, "remote service");
  auto* service = client_->getService(BLEUUID(SonyRemoteProtocol::kServiceUuid));
  if (!service) { setState(ConnectionState::Error, "service FF00 missing"); client_->disconnect(); return false; }
  command_ = service->getCharacteristic(BLEUUID(SonyRemoteProtocol::kCommandUuid));
  auto* notify = service->getCharacteristic(BLEUUID(SonyRemoteProtocol::kNotifyUuid));
  if (!command_) { setState(ConnectionState::Error, "command FF01 missing"); client_->disconnect(); return false; }
  if (notify && notify->canNotify()) notify->registerForNotify(notifyCallback);
  setState(ConnectionState::Ready, authenticated_ ? "bonded" : "camera ready"); return true;
}
bool SonyBleRemote::send(const uint8_t* data, size_t len, const char* label) {
  if (!connected()) { Serial.printf("[TX] %s skipped: not connected\n", label); return false; }
  Serial.printf("[TX] %s", label); for (size_t i=0; i<len; ++i) Serial.printf(" %02X", data[i]); Serial.println();
  command_->writeValue(const_cast<uint8_t*>(data), len, true); return client_->isConnected();
}
bool SonyBleRemote::focusDown() { return send(SonyRemoteProtocol::kHalfDown, 2, "half-down"); }
bool SonyBleRemote::releaseAll() { bool ok=send(SonyRemoteProtocol::kFullUp,2,"full-up"); delay(10); return send(SonyRemoteProtocol::kHalfUp,2,"half-up")&&ok; }
bool SonyBleRemote::takePhoto() { bool ok=focusDown(); delay(120); ok=send(SonyRemoteProtocol::kFullDown,2,"full-down")&&ok; delay(80); return releaseAll()&&ok; }
bool SonyBleRemote::shutterDown() {
  bool ok=focusDown(); delay(120);
  ok=send(SonyRemoteProtocol::kFullDown,2,"bulb-open-down")&&ok; delay(80);
  return releaseAll()&&ok;
}
bool SonyBleRemote::shutterUp() {
  bool ok=focusDown(); delay(120);
  ok=send(SonyRemoteProtocol::kFullDown,2,"bulb-close-down")&&ok; delay(80);
  return releaseAll()&&ok;
}
bool SonyBleRemote::toggleRecord() { bool ok=send(SonyRemoteProtocol::kRecordDown,2,"record-down"); delay(80); return send(SonyRemoteProtocol::kRecordUp,2,"record-up")&&ok; }
void SonyBleRemote::clearBonds() {
  int count=esp_ble_get_bond_device_num();
  if(count>0){auto* list=new esp_ble_bond_dev_t[count]; esp_ble_get_bond_device_list(&count,list); for(int i=0;i<count;++i)esp_ble_remove_bond_device(list[i].bd_addr); delete[] list;}
  Serial.printf("[BLE] removed %d bonds\n",count);
}
void SonyBleRemote::notifyCallback(BLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
  Serial.print("[RX]"); for(size_t i=0;i<len;++i)Serial.printf(" %02X",data[i]);
  auto f=SonyRemoteProtocol::parseFeedback(data,len); Serial.printf(" (%s)\n",SonyRemoteProtocol::feedbackName(f));
  if(instance_&&instance_->feedbackCallback_)instance_->feedbackCallback_(f);
}
void SonyBleRemote::onConnect(BLEClient*) { Serial.println("[BLE] link connected"); }
void SonyBleRemote::onDisconnect(BLEClient*) { command_=nullptr; setState(ConnectionState::Disconnected,"link lost"); reconnectAt_=millis()+1000; }
uint32_t SonyBleRemote::onPassKeyRequest(){Serial.println("[PAIR] passkey requested");return 0;}
void SonyBleRemote::onPassKeyNotify(uint32_t key){Serial.printf("[PAIR] passkey %06lu\n",(unsigned long)key);}
bool SonyBleRemote::onSecurityRequest(){Serial.println("[PAIR] security accepted");return true;}
bool SonyBleRemote::onConfirmPIN(uint32_t pin){Serial.printf("[PAIR] PIN %06lu accepted\n",(unsigned long)pin);return true;}
void SonyBleRemote::onAuthenticationComplete(esp_ble_auth_cmpl_t r){authenticated_=r.success;Serial.printf("[PAIR] complete success=%d failReason=0x%02X\n",r.success,r.fail_reason);}
