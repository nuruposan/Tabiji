#pragma once

#include <NimBLEDevice.h>

class BleSerialClient : public NimBLEClientCallbacks, public NimBLEScanCallbacks
{
private:
    static constexpr const char *DEVICE_NAME_DEFAULT = "ESP32-BLE";
    static constexpr const char *CLIENT_NAME = "ESP32-BLE-Client";
    static constexpr uint8_t BLE_MTU_SIZE = 251;

    static constexpr const char *SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
    static constexpr const char *RX_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
    static constexpr const char *TX_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

    NimBLEClient *_bleClient = nullptr;
    NimBLERemoteCharacteristic *_txChar = nullptr;
    NimBLERemoteCharacteristic *_rxChar = nullptr;
    char _deviceName[32];
    bool _running = false;
    bool _connected = false;

    void (*_onConnect)(NimBLEConnInfo &info) = nullptr;
    void (*_onDisconnect)(NimBLEConnInfo &info, int reason) = nullptr;
    void (*_onReceiveData)(NimBLEConnInfo &info, const char *data, size_t len) = nullptr;

    void onConnect(NimBLEClient *client) override;
    void onConnectFail(NimBLEClient *client, int reason) override;
    void onDisconnect(NimBLEClient *client, int reason) override;
    void onAuthenticationComplete(NimBLEConnInfo &info) override;
    void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override;
    void onScanEnd(const NimBLEScanResults &results, int reason) override;
    void onNotify(NimBLERemoteCharacteristic *characteristic, uint8_t *data, size_t len, bool isNotify);

    bool connectToServer(const NimBLEAdvertisedDevice *advertisedDevice);
    bool setupCharacteristics();

public:
    BleSerialClient(const char *deviceName = DEVICE_NAME_DEFAULT);
    ~BleSerialClient();

    BleSerialClient(const BleSerialClient &) = delete;
    BleSerialClient &operator=(const BleSerialClient &) = delete;

    void start(uint32_t scanTimeMs = 5000);
    void stop();
    bool isConnected() const;
    bool sendString(const char *str, size_t len);
    void setOnConnect(void (*callback)(NimBLEConnInfo &info));
    void setOnDisconnect(void (*callback)(NimBLEConnInfo &info, int reason));
    void setOnReceiveData(void (*callback)(NimBLEConnInfo &info, const char *data, size_t len));
};