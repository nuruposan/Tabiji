#include "BleSerialConnection.h"

/**
 * Constructor for the BleSerialConnection class.
 * Initializes the BLE device and configures it based on the connection mode (server or client).
 * @param mode The connection mode (server or client)
 * @param deviceName The name of the BLE device
 * @note The device name is used only in server mode; in client mode, a predefined client name is used.
 */
BleSerialConnection::BleSerialConnection(ConnectionMode mode, const char *serverName) : _mode(mode) {
  // Copy the device name into the internal buffer and ensure it is null-terminated
  strncpy(_serverName, serverName, DEVICE_NAME_LEN);
  _serverName[DEVICE_NAME_LEN] = '\0';

  // Initialize the BLE device with the appropriate name based on the connection mode
  NimBLEDevice::init(std::string(_mode == ConnectionMode::MODE_SERVER ? _serverName : CLIENT_NAME_DEFAULT));

  // Configure the BLE device's MTU, security authentication, and IO capabilities
  NimBLEDevice::setMTU(MTU_SIZE_PREFERRED);
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  // Configure BLE device based on the connection mode
  if (_mode == ConnectionMode::MODE_SERVER) {  // Server mode
    // Set the preferred PHY and power level for the server
    NimBLEDevice::setDefaultPhy(BLE_GAP_LE_PHY_2M_MASK, BLE_GAP_LE_PHY_2M_MASK);
    NimBLEDevice::setPowerLevel(ESP_PWR_LVL_P3, ESP_BLE_PWR_TYPE_ADV);

    // Create and configure the BLE server and service
    _bleServer = NimBLEDevice::createServer();
    _bleServer->setCallbacks(this);
    _bleService = _bleServer->createService(SERVICE_UUID);
    _txChar = _bleService->createCharacteristic(TX_UUID, NIMBLE_PROPERTY::NOTIFY);
    _rxChar = _bleService->createCharacteristic(RX_UUID, NIMBLE_PROPERTY::WRITE_ENC);
    _rxChar->setCallbacks(this);
    _bleServer->start();

    // set up advertising parameters
    _bleAdvertising = _bleServer->getAdvertising();
    _bleAdvertising->addServiceUUID(SERVICE_UUID);
    _bleAdvertising->setAdvertisingInterval(ADV_INTERVAL);
    return;
  } else {  // Client mode
    // Initialize BLE scan for client mode
    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(this);
    scan->setActiveScan(true);
  }
}

/**
 * Destructor for the BleSerialConnection class.
 * Cleans up BLE resources and stops any ongoing BLE operations.
 */
BleSerialConnection::~BleSerialConnection() {
  // Stop the current BLE operation (advertising or scanning)
  stop();

  // Delete the BLE client if it exists
  if (_mode == ConnectionMode::MODE_CLIENT && _bleClient) {
    NimBLEDevice::deleteClient(_bleClient);
    _bleClient = nullptr;
  }

  // Deinitialize the BLE device
  NimBLEDevice::deinit();
}

/**
 * Start the BLE connection (advertising for server, scanning for client)
 * @param scanTimeMs The scan duration in milliseconds (only used in client mode)
 */
void BleSerialConnection::start(uint32_t scanTimeMs) {
  if (_mode == ConnectionMode::MODE_SERVER) {  // server mode
    // Start advertising for the server
    if (_bleAdvertising) {
      _bleAdvertising->start();
      _running = true;
    }
  } else {  // client mode
    // Start scanning for BLE devices in client mode
    _running = true;
    NimBLEDevice::getScan()->start(scanTimeMs, false, true);
  }
}

void BleSerialConnection::process() {
  if (_mode != ConnectionMode::MODE_CLIENT || !_connectPending || !_pendingDevice) {
    return;
  }

  const NimBLEAdvertisedDevice *advertisedDevice = _pendingDevice;
  _pendingDevice = nullptr;
  _connectPending = false;

  if (!connectToServer(advertisedDevice) && _running && !_connected) {
    NimBLEDevice::getScan()->start(5000, false, true);
  }
}

/**
 * Stop the BLE connection (advertising for server, scanning for client)
 */
void BleSerialConnection::stop() {
  // mark the connection as not running
  _running = false;

  // stop the BLE operation based on the current mode
  if (_mode == ConnectionMode::MODE_SERVER) {  // server mode
    // stop advertising if it is active
    if (_bleAdvertising) {
      _bleAdvertising->stop();
    }

    // disconnect the client if it is connected
    if (_bleServer && _clientConnHandle != BLE_HS_CONN_HANDLE_NONE) {
      _bleServer->disconnect(_clientConnHandle);
    }
  } else {  // client mode
    // stop scanning if it is active
    NimBLEDevice::getScan()->stop();

    // disconnect from the server if it is connected
    if (_bleClient && _bleClient->isConnected()) {
      _bleClient->disconnect();
    }
  }
}

/**
 * Check if the BLE connection is currently established.
 * @return true if connected, false otherwise.
 */
bool BleSerialConnection::isConnected() const {
  return _connected;
}

/**
 * Set the callback function to be called when a BLE connection is established.
 * @param callback The callback function to be set.
 */
void BleSerialConnection::setOnConnect(void (*callback)(NimBLEConnInfo &info)) {
  _onConnect = callback;
}

/**
 * Set the callback function to be called immediately before connecting to a server.
 * @param callback The callback function to be set.
 */
void BleSerialConnection::setOnConnecting(void (*callback)(const NimBLEAdvertisedDevice &advertisedDevice)) {
  _onConnecting = callback;
}

/**
 * Set the callback function to be called when connecting to a server fails.
 * @param callback The callback function to be set.
 */
void BleSerialConnection::setOnConnectFail(void (*callback)(NimBLEConnInfo &info, int reason)) {
  _onConnectFail = callback;
}

/**
 * Set the callback function to be called when a BLE connection is disconnected.
 * @param callback The callback function to be set.
 */
void BleSerialConnection::setOnDisconnect(void (*callback)(NimBLEConnInfo &info, int reason)) {
  _onDisconnect = callback;
}

/**
 * Set the callback function to be called when data is received over the BLE connection.
 * @param callback The callback function to be set.
 */
void BleSerialConnection::setOnReceive(void (*callback)(NimBLEConnInfo &info, const char *data, size_t len)) {
  _onReceive = callback;
}

/**
 * Send a string of data over the BLE connection.
 * @param str The string data to be sent.
 * @param len The length of the string data.
 * @return true if the data was successfully sent, false otherwise.
 */
bool BleSerialConnection::sendString(const char *str, size_t len) {
  // Return false immediately if not connected or if the input string is null
  if (!_connected) return false;
  if (!str) return false;

  // Consider zero-length data as successfully sent
  if (len == 0) return true;

  //
  if (_mode == ConnectionMode::MODE_SERVER) {
    // Return false if the TX characteristic is not available
    if (!_txChar) return false;

    // Send the data in chunks of MAX_TRANSFER_SIZE
    for (size_t offset = 0; offset < len; offset += MAX_TRANSFER_SIZE) {
      const size_t chunkSize = min(MAX_TRANSFER_SIZE, len - offset);
      _txChar->setValue(reinterpret_cast<const uint8_t *>(str) + offset, chunkSize);
      if (!_txChar->notify()) {
        // Notification failed, return false to indicate the failure
        return false;
      }
    }
  } else {  // client mode
    // Return false if the remote RX characteristic is not available
    if (!_remoteRxChar) return false;

    // Send the data in chunks of MAX_TRANSFER_SIZE
    for (size_t offset = 0; offset < len; offset += MAX_TRANSFER_SIZE) {
      const size_t chunkSize = min(MAX_TRANSFER_SIZE, len - offset);
      if (!_remoteRxChar->writeValue(reinterpret_cast<const uint8_t *>(str) + offset, chunkSize, true)) {
        // Writing the current chunk of data to the remote RX characteristic failed
        return false;
      }
    }
  }

  return true;
}

void BleSerialConnection::onConnect(NimBLEServer *server, NimBLEConnInfo &info) {
  if (_mode != ConnectionMode::MODE_SERVER) {
    return;
  }

  // If there is already a client connected, do not allow another connection
  if (_clientConnHandle != BLE_HS_CONN_HANDLE_NONE) {
    _bleServer->disconnect(info.getConnHandle());
    return;
  }

  // Accept the new client connection
  _clientConnHandle = info.getConnHandle();
  _connected = true;
  Serial.println("BLE server client connected");
  if (_onConnect) {
    _onConnect(info);
  }
}

void BleSerialConnection::onDisconnect(NimBLEServer *server, NimBLEConnInfo &info, int reason) {
  if (_mode != ConnectionMode::MODE_SERVER || info.getConnHandle() != _clientConnHandle) {
    return;
  }

  _connected = false;
  _clientConnHandle = BLE_HS_CONN_HANDLE_NONE;
  if (_running && _bleAdvertising) {
    _bleAdvertising->start();
  }
  if (_onDisconnect) {
    _onDisconnect(info, reason);
  }
}

void BleSerialConnection::onReceive(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) {
  if (_mode != ConnectionMode::MODE_SERVER) {
    return;
  }

  const std::string value = characteristic->getValue();
  if (_onReceive) {
    _onReceive(info, value.data(), value.size());
  }
}

void BleSerialConnection::onConnect(NimBLEClient *client) {
  if (_mode != ConnectionMode::MODE_CLIENT) {
    return;
  }

  _bleClient = client;
  Serial.println("BLE client callback received");
}

void BleSerialConnection::onConnectFail(NimBLEClient *client, int reason) {
  if (_mode == ConnectionMode::MODE_CLIENT) {
    Serial.printf("BLE client connect failed: %d\n", reason);
    if (_onConnectFail) {
      NimBLEConnInfo info = client->getConnInfo();
      _onConnectFail(info, reason);
    }
  }
}

void BleSerialConnection::onAuthenticationComplete(NimBLEConnInfo &info) {
  if (_mode == ConnectionMode::MODE_SERVER) {
    // Ensure that the authentication complete event is for the current client connection
    if (info.getConnHandle() != _clientConnHandle) {
      return;
    }

    // Check if the connection is encrypted after the authentication process
    if (!info.isEncrypted()) {  // not encrypted
      Serial.println("BLE server authentication failed");
      // Disconnect the client and terminate the connection process
      _bleServer->disconnect(info.getConnHandle());
      return;
    }

    // The connection callback was already delivered when the link was established.
    _connected = true;
    Serial.println("BLE server authentication complete");
    return;
  } else {  // Client mode
    // A secured characteristic may trigger this callback after the initial connection.
    NimBLEClient *client = NimBLEDevice::getClientByHandle(info.getConnHandle());
    if (!info.isEncrypted() || !client) {  // Failed to set up
      Serial.printf("BLE client authentication failed: encrypted=%d client=%p\n", info.isEncrypted(), client);
      // disconnect from the server and terminate the connection process
      if (client) client->disconnect();
      return;
    }

    Serial.println("BLE client authentication complete");
    return;
  }

  // Mark the connection as established for both server and client modes
  _connected = true;
  Serial.println("BLE client ready");

  // Notify the application that the connection has been established
  if (_onConnect) _onConnect(info);
}

void BleSerialConnection::onDisconnect(NimBLEClient *client, int reason) {
  // Ensure this callback is only processed in client mode
  if (_mode != ConnectionMode::MODE_CLIENT) {
    return;
  }

  // Mark the connection as no longer established
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
  if (_mode != ConnectionMode::MODE_CLIENT || (_bleClient && _bleClient->isConnected())) {
    return;
  }

  const bool hasService = advertisedDevice->isAdvertisingService(NimBLEUUID(SERVICE_UUID));
  const bool hasExpectedName = advertisedDevice->haveName() && advertisedDevice->getName() == _serverName;
  Serial.printf("BLE advertisement: %s\n", advertisedDevice->toString().c_str());

  if (hasService || hasExpectedName) {
    Serial.printf("BLE server found: %s\n", advertisedDevice->toString().c_str());
    _pendingDevice = advertisedDevice;
    _connectPending = true;
    NimBLEDevice::getScan()->stop();
  }
}

void BleSerialConnection::onScanEnd(const NimBLEScanResults &results, int reason) {
  if (_mode == ConnectionMode::MODE_CLIENT && _running && !_connected && !_connectPending) {
    NimBLEDevice::getScan()->start(5000, false, true);
  }
}

bool BleSerialConnection::connectToServer(const NimBLEAdvertisedDevice *advertisedDevice) {
  Serial.printf("BLE connecting to %s\n", advertisedDevice->getAddress().toString().c_str());
  // Attempt to retrieve an existing client by the peer address of the advertised device.
  _bleClient = NimBLEDevice::getClientByPeerAddress(advertisedDevice->getAddress());
  if (!_bleClient)  // no existing client found
  {
    _bleClient = NimBLEDevice::getDisconnectedClient();
  }

  // If no existing client is found, attempt to retrieve a disconnected client.
  if (!_bleClient)  // no disconnected client available
  {
    _bleClient = NimBLEDevice::createClient();  // create a new BLE client
    if (!_bleClient)                            // failed to create a new BLE client
    {
      Serial.println("BLE failed to create client");
      return false;
    }

    _bleClient->setConnectTimeout(5000);
  }
  _bleClient->setClientCallbacks(this, false);

  // Notify the application that the client is attempting to connect to the server.
  if (_onConnecting) {
    _onConnecting(*advertisedDevice);
  }

  // Attempt to connect to the server.
  bool result = _bleClient->connect(advertisedDevice, true, false, true);
  if (!result) {  // Connection attempt failed
    _bleClient->disconnect();

    // Notify the application that the connection attempt failed.
    if (_onConnectFail) {
      NimBLEConnInfo info = _bleClient->getConnInfo();
      _onConnectFail(info, BLE_HS_EUNKNOWN);
    }
    return false;
  }

  Serial.println("BLE connect() completed");

  // A synchronous connect can complete before the client callback is dispatched.
  // Do not make readiness depend on callback timing.
  if (!_connected && _bleClient->isConnected()) {
    Serial.println("BLE client connected; discovering services");
    if (!setupCharacteristics()) {
      Serial.println("BLE client service setup failed");
      _bleClient->disconnect();
      return false;
    }

    _connected = true;
    Serial.println("BLE client ready");
    if (_onConnect) {
      NimBLEConnInfo info = _bleClient->getConnInfo();
      _onConnect(info);
    }
  }

  return true;
}

/**
 * Sets up the remote RX and TX characteristics for the BLE client.
 * @return true if the characteristics are successfully set up, false otherwise.
 */
bool BleSerialConnection::setupCharacteristics() {
  // Retrieve the remote service for the BLE client
  NimBLERemoteService *service = _bleClient->getService(SERVICE_UUID);
  if (!service) {
    Serial.println("BLE service not found");
    return false;
  }

  // Retrieve the remote RX and TX characteristics for the BLE client
  _remoteRxChar = service->getCharacteristic(RX_UUID);
  _remoteTxChar = service->getCharacteristic(TX_UUID);
  if (!_remoteRxChar || !_remoteTxChar || (!_remoteTxChar->canNotify() && !_remoteTxChar->canIndicate())) {
    Serial.println("BLE RX/TX characteristic setup failed");
    return false;
  }

  const bool subscribed = _remoteTxChar->subscribe(
      _remoteTxChar->canNotify(), [this](NimBLERemoteCharacteristic *characteristic, uint8_t *data, size_t len,
                                      bool isNotify) { onNotify(characteristic, data, len, isNotify); });
  if (!subscribed) {
    Serial.println("BLE notification subscription failed");
  }
  return subscribed;
}

void BleSerialConnection::onNotify(
    NimBLERemoteCharacteristic *characteristic, uint8_t *data, size_t len, bool isNotify) {
  if (_mode == ConnectionMode::MODE_CLIENT && _onReceive && _bleClient) {
    NimBLEConnInfo info = _bleClient->getConnInfo();
    _onReceive(info, reinterpret_cast<const char *>(data), len);
  }
}
