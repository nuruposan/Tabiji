#include <Arduino.h>
#include "BleSerialClient.h"

BleSerialClient::BleSerialClient(const char *deviceName)
{
    strncpy(_deviceName, deviceName, sizeof(_deviceName) - 1);
    _deviceName[sizeof(_deviceName) - 1] = '\0';

    NimBLEDevice::init(CLIENT_NAME);
    NimBLEDevice::setMTU(BLE_MTU_SIZE);
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(this);
    scan->setActiveScan(true);
}

BleSerialClient::~BleSerialClient()
{
    stop();
    if (_bleClient)
    {
        NimBLEDevice::deleteClient(_bleClient);
        _bleClient = nullptr;
    }
    NimBLEDevice::deinit();
}

void BleSerialClient::start(uint32_t scanTimeMs)
{
    _running = true;
    NimBLEDevice::getScan()->start(scanTimeMs, false, true);
}

void BleSerialClient::stop()
{
    _running = false;
    NimBLEDevice::getScan()->stop();
    if (_bleClient && _bleClient->isConnected())
    {
        _bleClient->disconnect();
    }
}

bool BleSerialClient::isConnected() const
{
    return _connected;
}

void BleSerialClient::setOnConnect(void (*callback)(NimBLEConnInfo &info))
{
    _onConnect = callback;
}

void BleSerialClient::setOnDisconnect(void (*callback)(NimBLEConnInfo &info, int reason))
{
    _onDisconnect = callback;
}

void BleSerialClient::setOnReceiveData(void (*callback)(NimBLEConnInfo &info, const char *data, size_t len))
{
    _onReceiveData = callback;
}

bool BleSerialClient::sendString(const char *str, size_t len)
{
    if (!_rxChar || !str || len == 0)
    {
        return false;
    }

    constexpr size_t ATT_HEADER_SIZE = 3;
    constexpr size_t MAX_WRITE_SIZE = BLE_MTU_SIZE - ATT_HEADER_SIZE;

    for (size_t offset = 0; offset < len; offset += MAX_WRITE_SIZE)
    {
        const size_t chunkSize = min(MAX_WRITE_SIZE, len - offset);
        if (!_rxChar->writeValue(reinterpret_cast<const uint8_t *>(str) + offset, chunkSize, true))
        {
            return false;
        }
    }
    return true;
}

void BleSerialClient::onConnect(NimBLEClient *client)
{
    _bleClient = client;
    Serial.println("Server connected; starting encryption...");
    if (!client->getConnInfo().isEncrypted() && !client->secureConnection())
    {
        Serial.println("Failed to start BLE security.");
        client->disconnect();
    }
}

void BleSerialClient::onConnectFail(NimBLEClient *client, int reason)
{
    Serial.printf("Failed to connect to server, reason = %d.\n", reason);
}

void BleSerialClient::onAuthenticationComplete(NimBLEConnInfo &info)
{
    NimBLEClient *client = NimBLEDevice::getClientByHandle(info.getConnHandle());
    if (!info.isEncrypted() || !client || !setupCharacteristics())
    {
        Serial.println("BLE connection setup failed.");
        if (client)
        {
            client->disconnect();
        }
        return;
    }

    _connected = true;
    if (_onConnect)
    {
        _onConnect(info);
    }
}

void BleSerialClient::onDisconnect(NimBLEClient *client, int reason)
{
    _connected = false;
    _txChar = nullptr;
    _rxChar = nullptr;

    NimBLEConnInfo info = client->getConnInfo();
    if (_onDisconnect)
    {
        _onDisconnect(info, reason);
    }
    if (_running)
    {
        NimBLEDevice::getScan()->start(5000, false, true);
    }
}

void BleSerialClient::onResult(const NimBLEAdvertisedDevice *advertisedDevice)
{
    if (_bleClient && _bleClient->isConnected())
    {
        return;
    }
    if (advertisedDevice->isAdvertisingService(NimBLEUUID(SERVICE_UUID)) &&
        (!advertisedDevice->haveName() || advertisedDevice->getName() == _deviceName))
    {
        NimBLEDevice::getScan()->stop();
        connectToServer(advertisedDevice);
    }
}

void BleSerialClient::onScanEnd(const NimBLEScanResults &results, int reason)
{
    if (_running && !_connected)
    {
        NimBLEDevice::getScan()->start(5000, false, true);
    }
}

bool BleSerialClient::connectToServer(const NimBLEAdvertisedDevice *advertisedDevice)
{
    _bleClient = NimBLEDevice::getClientByPeerAddress(advertisedDevice->getAddress());
    if (!_bleClient)
    {
        _bleClient = NimBLEDevice::getDisconnectedClient();
    }
    if (!_bleClient)
    {
        _bleClient = NimBLEDevice::createClient();
        if (!_bleClient)
        {
            return false;
        }
        _bleClient->setClientCallbacks(this, false);
        _bleClient->setConnectTimeout(5000);
    }
    else
    {
        _bleClient->setClientCallbacks(this, false);
    }

    if (!_bleClient->connect(advertisedDevice, true, false, true))
    {
        _bleClient->disconnect();
        return false;
    }
    return true;
}

bool BleSerialClient::setupCharacteristics()
{
    NimBLERemoteService *service = _bleClient->getService(SERVICE_UUID);
    if (!service)
    {
        return false;
    }

    _rxChar = service->getCharacteristic(RX_UUID);
    _txChar = service->getCharacteristic(TX_UUID);
    if (!_rxChar || !_txChar || (!_txChar->canNotify() && !_txChar->canIndicate()))
    {
        return false;
    }

    return _txChar->subscribe(_txChar->canNotify(),
                              [this](NimBLERemoteCharacteristic *characteristic, uint8_t *data, size_t len, bool isNotify)
                              {
                                  onNotify(characteristic, data, len, isNotify);
                              });
}

void BleSerialClient::onNotify(NimBLERemoteCharacteristic *characteristic, uint8_t *data, size_t len, bool isNotify)
{
    if (_onReceiveData && _bleClient)
    {
        NimBLEConnInfo info = _bleClient->getConnInfo();
        _onReceiveData(info, reinterpret_cast<const char *>(data), len);
    }
}