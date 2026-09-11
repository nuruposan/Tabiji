#include <Arduino.h>
#include "BleSerialServer.h"

BleSerialServer::BleSerialServer(const char *deviceName)
{
  Serial.println("Initializing BLE Serial Server...");

  // Copy the device name into the internal buffer and ensure null termination
  strncpy(_deviceName, deviceName, DEVICE_NAME_LEN);

  _deviceName[DEVICE_NAME_LEN] = '\0';

  // Initialize the NimBLE device with the given device name
  NimBLEDevice::init(std::string(_deviceName));

  // Set the default PHY to 2M and the advertising power level
  NimBLEDevice::setDefaultPhy(BLE_GAP_LE_PHY_2M_MASK, BLE_GAP_LE_PHY_2M_MASK);
  NimBLEDevice::setPowerLevel(ESP_PWR_LVL_P3, ESP_BLE_PWR_TYPE_ADV);
  NimBLEDevice::setMTU(BLE_MTU_SIZE);

  // Require an encrypted connection without MITM/passkey authentication
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  // Create the BLE server, service, and characteristics
  _bleServer = NimBLEDevice::createServer();
  _bleServer->setCallbacks(this);
  _bleService = _bleServer->createService(SERVICE_UUID);
  _txChar = _bleService->createCharacteristic(TX_UUID, NIMBLE_PROPERTY::NOTIFY);
  _rxChar = _bleService->createCharacteristic(RX_UUID, NIMBLE_PROPERTY::WRITE_ENC);
  _rxChar->setCallbacks(this);

  // Start the BLE service
  _bleServer->start();

  // Get the advertising object from the BLE server and add the service UUID to it
  _bleAdvertising = _bleServer->getAdvertising();
  _bleAdvertising->addServiceUUID(SERVICE_UUID);
  _bleAdvertising->setAdvertisingInterval(ADV_INTERVAL);
}

BleSerialServer::~BleSerialServer()
{
  // Clean up and release BLE resources before deinitializing the NimBLE device
  stop();

  // Deinitialize the NimBLE device to free up resources
  NimBLEDevice::deinit();
}

void BleSerialServer::start()
{
  // Start advertising the BLE service
  _bleAdvertising->start();
}

void BleSerialServer::stop()
{
  // Stop advertising the BLE service
  _bleAdvertising->stop();
}

bool BleSerialServer::isConnected() const
{
  return _clientConnected;
}

void BleSerialServer::setOnConnect(void (*callback)(NimBLEConnInfo &info))
{
  _onConnect = callback;
}

void BleSerialServer::setOnDisconnect(void (*callback)(NimBLEConnInfo &info, int reason))
{
  _onDisconnect = callback;
}

void BleSerialServer::setOnReceiveData(void (*callback)(NimBLEConnInfo &info, const char *data, size_t len))
{
  _onReceiveData = callback;
}

bool BleSerialServer::sendString(const char *str, size_t len)
{
  if (!_txChar || !_clientConnected || !str || len == 0)
  {
    return false;
  }

  Serial.print("Sending string: ");
  Serial.write(reinterpret_cast<const uint8_t *>(str), len);
  Serial.println();

  constexpr size_t ATT_HEADER_SIZE = 3;
  constexpr size_t MAX_NOTIFICATION_SIZE = BLE_MTU_SIZE - ATT_HEADER_SIZE;

  for (size_t offset = 0; offset < len; offset += MAX_NOTIFICATION_SIZE)
  {
    const size_t chunkSize = min(MAX_NOTIFICATION_SIZE, len - offset);
    _txChar->setValue(reinterpret_cast<const uint8_t *>(str) + offset, chunkSize);
    if (!_txChar->notify())
    {
      return false;
    }
  }
  return true;
}

void BleSerialServer::onConnect(NimBLEServer *server, NimBLEConnInfo &info)
{
  if (_clientConnHandle != BLE_HS_CONN_HANDLE_NONE)
  {
    Serial.println("Another client is already connected; rejecting connection.");
    _bleServer->disconnect(info.getConnHandle());
    return;
  }

  Serial.println("Client connected; starting encryption...");

  _clientConnHandle = info.getConnHandle();
  _clientConnected = false;
  if (!info.isEncrypted() && !NimBLEDevice::startSecurity(info.getConnHandle()))
  {
    Serial.println("Failed to start BLE security.");
    _bleServer->disconnect(info.getConnHandle());
  }
}

void BleSerialServer::onAuthenticationComplete(NimBLEConnInfo &info)
{
  if (info.getConnHandle() != _clientConnHandle)
  {
    return;
  }

  if (!info.isEncrypted())
  {
    Serial.println("BLE encryption failed.");
    _bleServer->disconnect(info.getConnHandle());
    return;
  }

  _clientConnected = true;
  if (_onConnect)
  {
    _onConnect(info);
  }
}

void BleSerialServer::onDisconnect(NimBLEServer *server, NimBLEConnInfo &info, int reason)
{
  if (info.getConnHandle() != _clientConnHandle)
  {
    return;
  }

  //  Serial.println("Client disconnected.");

  _clientConnected = false;
  _clientConnHandle = BLE_HS_CONN_HANDLE_NONE;
  _bleAdvertising->start(); // Restart advertising after a client disconnects

  if (_onDisconnect)
  {
    _onDisconnect(info, reason);
  }
}

void BleSerialServer::onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &info)
{
  const std::string value = characteristic->getValue();

  Serial.println("Data received from client.");
  Serial.print("Received data: ");
  Serial.write((const uint8_t *)value.data(), value.size());
  Serial.println();

  if (_onReceiveData)
  {
    _onReceiveData(info, value.c_str(), value.size());
  }
}
