#include "BleSerialConnection.h"

BleSerialConnection::BleSerialConnection(Mode mode, const char *deviceName) : _mode(mode) {
  strncpy(_deviceName, deviceName, DEVICE_NAME_LEN);
  _deviceName[DEVICE_NAME_LEN] = '\0';

  NimBLEDevice::init(std::string(_mode == Mode::BLE_SERIAL_SERVER ? _deviceName : CLIENT_NAME));
  NimBLEDevice::setMTU(MTU_SIZE_PREFFERED);
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  if (_mode == Mode::BLE_SERIAL_SERVER) {
    NimBLEDevice::setDefaultPhy(BLE_GAP_LE_PHY_2M_MASK, BLE_GAP_LE_PHY_2M_MASK);
    NimBLEDevice::setPowerLevel(ESP_PWR_LVL_P3, ESP_BLE_PWR_TYPE_ADV);

    _bleServer = NimBLEDevice::createServer();
    _bleServer->setCallbacks(this);
    _bleService = _bleServer->createService(SERVICE_UUID);
    _txChar = _bleService->createCharacteristic(TX_UUID, NIMBLE_PROPERTY::NOTIFY);
    _rxChar = _bleService->createCharacteristic(RX_UUID, NIMBLE_PROPERTY::WRITE_ENC);
    _rxChar->setCallbacks(this);
    _bleServer->start();

    _bleAdvertising = _bleServer->getAdvertising();
    _bleAdvertising->addServiceUUID(SERVICE_UUID);
    _bleAdvertising->setAdvertisingInterval(ADV_INTERVAL);
    return;
  }

  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(this);
  scan->setActiveScan(true);
}

BleSerialConnection::~BleSerialConnection() {
  stop();
  if (_mode == Mode::BLE_SERIAL_CLIENT && _bleClient) {
    NimBLEDevice::deleteClient(_bleClient);
    _bleClient = nullptr;
  }
  NimBLEDevice::deinit();
}

void BleSerialConnection::start(uint32_t scanTimeMs) {
  if (_mode == Mode::BLE_SERIAL_SERVER) {
    if (_bleAdvertising) {
      _bleAdvertising->start();
    }
    return;
  }

  _running = true;
  NimBLEDevice::getScan()->start(scanTimeMs, false, true);
}

void BleSerialConnection::stop() {
  if (_mode == Mode::BLE_SERIAL_SERVER) {
    if (_bleAdvertising) {
      _bleAdvertising->stop();
    }
    return;
  }

  _running = false;
  NimBLEDevice::getScan()->stop();
  if (_bleClient && _bleClient->isConnected()) {
    _bleClient->disconnect();
  }
}

bool BleSerialConnection::isConnected() const {
  return _connected;
}

void BleSerialConnection::setOnConnect(void (*callback)(NimBLEConnInfo &info)) {
  _onConnect = callback;
}

void BleSerialConnection::setOnDisconnect(void (*callback)(NimBLEConnInfo &info, int reason)) {
  _onDisconnect = callback;
}

void BleSerialConnection::setOnReceive(void (*callback)(NimBLEConnInfo &info, const char *data, size_t len)) {
  _onReceive = callback;
}

bool BleSerialConnection::sendString(const char *str, size_t len) {
  if (!str || len == 0) {
    return false;
  }

  constexpr size_t ATT_HEADER_SIZE = 3;
  constexpr size_t MAX_TRANSFER_SIZE = MTU_SIZE_PREFFERED - ATT_HEADER_SIZE;

  if (_mode == Mode::BLE_SERIAL_SERVER) {
    if (!_txChar || !_connected) {
      return false;
    }

    for (size_t offset = 0; offset < len; offset += MAX_TRANSFER_SIZE) {
      const size_t chunkSize = min(MAX_TRANSFER_SIZE, len - offset);
      _txChar->setValue(reinterpret_cast<const uint8_t *>(str) + offset, chunkSize);
      if (!_txChar->notify()) {
        return false;
      }
    }
    return true;
  }

  if (!_remoteRxChar || !_connected) {
    return false;
  }

  for (size_t offset = 0; offset < len; offset += MAX_TRANSFER_SIZE) {
    const size_t chunkSize = min(MAX_TRANSFER_SIZE, len - offset);
    if (!_remoteRxChar->writeValue(reinterpret_cast<const uint8_t *>(str) + offset, chunkSize, true)) {
      return false;
    }
  }
  return true;
}

void BleSerialConnection::onConnect(NimBLEServer *server, NimBLEConnInfo &info) {
  if (_mode != Mode::BLE_SERIAL_SERVER) {
    return;
  }
  if (_clientConnHandle != BLE_HS_CONN_HANDLE_NONE) {
    _bleServer->disconnect(info.getConnHandle());
    return;
  }

  _clientConnHandle = info.getConnHandle();
  _connected = false;
  if (!info.isEncrypted() && !NimBLEDevice::startSecurity(info.getConnHandle())) {
    _bleServer->disconnect(info.getConnHandle());
  }
}

void BleSerialConnection::onDisconnect(NimBLEServer *server, NimBLEConnInfo &info, int reason) {
  if (_mode != Mode::BLE_SERIAL_SERVER || info.getConnHandle() != _clientConnHandle) {
    return;
  }

  _connected = false;
  _clientConnHandle = BLE_HS_CONN_HANDLE_NONE;
  if (_bleAdvertising) {
    _bleAdvertising->start();
  }
  if (_onDisconnect) {
    _onDisconnect(info, reason);
  }
}

void BleSerialConnection::onReceive(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) {
  if (_mode != Mode::BLE_SERIAL_SERVER) {
    return;
  }

  const std::string value = characteristic->getValue();
  if (_onReceive) {
    _onReceive(info, value.data(), value.size());
  }
}

void BleSerialConnection::onConnect(NimBLEClient *client) {
  if (_mode != Mode::BLE_SERIAL_CLIENT) {
    return;
  }

  _bleClient = client;
  if (!client->getConnInfo().isEncrypted() && !client->secureConnection()) {
    client->disconnect();
  }
}

void BleSerialConnection::onConnectFail(NimBLEClient *client, int reason) {
  if (_mode == Mode::BLE_SERIAL_CLIENT) {
    Serial.printf("Failed to connect to server, reason = %d.\n", reason);
  }
}

void BleSerialConnection::onAuthenticationComplete(NimBLEConnInfo &info) {
  if (_mode == Mode::BLE_SERIAL_SERVER) {
    if (info.getConnHandle() != _clientConnHandle) {
      return;
    }
    if (!info.isEncrypted()) {
      _bleServer->disconnect(info.getConnHandle());
      return;
    }
    _connected = true;
    if (_onConnect) {
      _onConnect(info);
    }
    return;
  }

  NimBLEClient *client = NimBLEDevice::getClientByHandle(info.getConnHandle());
  if (!info.isEncrypted() || !client || !setupCharacteristics()) {
    if (client) {
      client->disconnect();
    }
    return;
  }

  _connected = true;
  if (_onConnect) {
    _onConnect(info);
  }
}

void BleSerialConnection::onDisconnect(NimBLEClient *client, int reason) {
  if (_mode != Mode::BLE_SERIAL_CLIENT) {
    return;
  }

  _connected = false;
  _remoteTxChar = nullptr;
  _remoteRxChar = nullptr;
  NimBLEConnInfo info = client->getConnInfo();
  if (_onDisconnect) {
    _onDisconnect(info, reason);
  }
  if (_running) {
    NimBLEDevice::getScan()->start(5000, false, true);
  }
}

void BleSerialConnection::onResult(const NimBLEAdvertisedDevice *advertisedDevice) {
  if (_mode != Mode::BLE_SERIAL_CLIENT || (_bleClient && _bleClient->isConnected())) {
    return;
  }
  if (advertisedDevice->isAdvertisingService(NimBLEUUID(SERVICE_UUID)) &&
      (!advertisedDevice->haveName() || advertisedDevice->getName() == _deviceName)) {
    NimBLEDevice::getScan()->stop();
    connectToServer(advertisedDevice);
  }
}

void BleSerialConnection::onScanEnd(const NimBLEScanResults &results, int reason) {
  if (_mode == Mode::BLE_SERIAL_CLIENT && _running && !_connected) {
    NimBLEDevice::getScan()->start(5000, false, true);
  }
}

bool BleSerialConnection::connectToServer(const NimBLEAdvertisedDevice *advertisedDevice) {
  _bleClient = NimBLEDevice::getClientByPeerAddress(advertisedDevice->getAddress());
  if (!_bleClient) {
    _bleClient = NimBLEDevice::getDisconnectedClient();
  }
  if (!_bleClient) {
    _bleClient = NimBLEDevice::createClient();
    if (!_bleClient) {
      return false;
    }
    _bleClient->setConnectTimeout(5000);
  }
  _bleClient->setClientCallbacks(this, false);

  if (!_bleClient->connect(advertisedDevice, true, false, true)) {
    _bleClient->disconnect();
    return false;
  }
  return true;
}

bool BleSerialConnection::setupCharacteristics() {
  NimBLERemoteService *service = _bleClient->getService(SERVICE_UUID);
  if (!service) {
    return false;
  }

  _remoteRxChar = service->getCharacteristic(RX_UUID);
  _remoteTxChar = service->getCharacteristic(TX_UUID);
  if (!_remoteRxChar || !_remoteTxChar || (!_remoteTxChar->canNotify() && !_remoteTxChar->canIndicate())) {
    return false;
  }

  return _remoteTxChar->subscribe(
      _remoteTxChar->canNotify(), [this](NimBLERemoteCharacteristic *characteristic, uint8_t *data, size_t len,
                                      bool isNotify) { onNotify(characteristic, data, len, isNotify); });
}

void BleSerialConnection::onNotify(
    NimBLERemoteCharacteristic *characteristic, uint8_t *data, size_t len, bool isNotify) {
  if (_mode == Mode::BLE_SERIAL_CLIENT && _onReceive && _bleClient) {
    NimBLEConnInfo info = _bleClient->getConnInfo();
    _onReceive(info, reinterpret_cast<const char *>(data), len);
  }
}
