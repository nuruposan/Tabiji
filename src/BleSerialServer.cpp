#include "BleSerialServer.h"

BleSerialServer::BleSerialServer(const char *deviceName) {
  // Copy the device name into the internal buffer and ensure null termination
  strncpy(_deviceName, deviceName, DEVICE_NAME_LEN);
  _deviceName[DEVICE_NAME_LEN] = '\0';

  // Initialize the NimBLE device with the given device name
  NimBLEDevice::init(_deviceName);

  // Set the default PHY to 2M and the advertising power level
  NimBLEDevice::setDefaultPhy(BLE_GAP_LE_PHY_2M_MASK, BLE_GAP_LE_PHY_2M_MASK);
  NimBLEDevice::setPowerLevel(ESP_PWR_LVL_P9, ESP_BLE_PWR_TYPE_ADV);
  NimBLEDevice::setMTU(BLE_MTU_SIZE);

  // Set BLE security authentication and IO capabilities as "Just Works"
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  // Create the BLE server, service, and characteristics
  _bleServer = NimBLEDevice::createServer();
  _bleServer->setCallbacks(this);
  _bleService = _bleServer->createService(SERVICE_UUID);
  _txChar = _bleService->createCharacteristic(TX_UUID, NIMBLE_PROPERTY::NOTIFY);
  _rxChar = _bleService->createCharacteristic(RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  _rxChar->setCallbacks(this);

  // Start the BLE service
  _bleService->start();

  // Get the advertising object from the BLE server and add the service UUID to it
  _bleAdvertising = _bleServer->getAdvertising();
  _bleAdvertising->addServiceUUID(SERVICE_UUID);
  _bleAdvertising->setAdvertisingInterval(ADV_INTERVAL);
}

BleSerialServer::~BleSerialServer() {
  end();

  NimBLEDevice::deinit();
  _bleServer = nullptr;
  _bleService = nullptr;
  _txChar = nullptr;
  _rxChar = nullptr;
  _bleAdvertising = nullptr;
}

void BleSerialServer::begin() {
  // Start advertising the BLE service
  _bleAdvertising->start();
}

void BleSerialServer::end() {
  // Stop advertising the BLE service
  _bleAdvertising->stop();
}

void BleSerialServer::setOnConnect(void (*callback)(NimBLEConnInfo &info)) {
  _onConnect = callback;
}

void BleSerialServer::setOnDisconnect(void (*callback)(NimBLEConnInfo &info, int reason)) {
  _onDisconnect = callback;
}

void BleSerialServer::setOnWrite(void (*callback)(NimBLEConnInfo &info, const char *data)) {
  _onWrite = callback;
}

void BleSerialServer::sendString(char *str, size_t len) {
  if (_txChar) {
    _txChar->setValue((uint8_t *)str, len);
    _txChar->notify();
  }
}

void BleSerialServer::onConnect(NimBLEServer *server, NimBLEConnInfo &info) {
  _clientConnected = true;
  if (_onConnect) {
    _onConnect(info);
  }
}

void BleSerialServer::onDisconnect(NimBLEServer *server, NimBLEConnInfo &info, int reason) {
  _clientConnected = false;
  if (_onDisconnect) {
    _onDisconnect(info, reason);
  }
}

void BleSerialServer::onReceive(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) {
  if (_onWrite) {
    _onWrite(info, characteristic->getValue().c_str());
  }
}
