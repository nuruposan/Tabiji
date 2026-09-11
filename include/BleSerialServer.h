#pragma once

#include <NimBLEDevice.h>

class BleSerialServer : public NimBLEServerCallbacks, public NimBLECharacteristicCallbacks
{
private:
  static constexpr const char *DEVICE_NAME_DEFAULT = "ESP32-BLE"; // Default BLE device name

  static constexpr size_t DEVICE_NAME_LEN = 31; // Maximum length of the BLE device name
  static constexpr uint8_t BLE_MTU_SIZE = 251;  // MTU size to set: default is 23 bytes, up to 251 bytes

  // BLE service and characteristic UUIDs for the UART service
  static constexpr const char *SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"; // UART service UUID
  static constexpr const char *RX_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";      // RX characteristic UUID
  static constexpr const char *TX_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";      // TX characteristic UUID

  // BLE advertising interval (in units of 0.625 msec)
  static constexpr uint16_t ADV_INTERVAL = 160 * 2.5; // (0.625 * 160) * 2.5 ms = 250 ms

  // BLE server, service, advertising, and characteristic objects
  NimBLEServer *_bleServer = nullptr;
  NimBLEService *_bleService = nullptr;
  NimBLEAdvertising *_bleAdvertising = nullptr;
  NimBLECharacteristic *_txChar = nullptr;
  NimBLECharacteristic *_rxChar = nullptr;

  // BLE device name and client connection status
  char _deviceName[(DEVICE_NAME_LEN + 1)];
  bool _clientConnected = false;
  uint16_t _clientConnHandle = BLE_HS_CONN_HANDLE_NONE;

  // Callback function pointers for BLE events
  void (*_onConnect)(NimBLEConnInfo &info) = nullptr;
  void (*_onDisconnect)(NimBLEConnInfo &info, int reason) = nullptr;
  void (*_onReceiveData)(NimBLEConnInfo &info, const char *data, size_t len) = nullptr;

  // NimBLE server and characteristic callback overrides
  void onConnect(NimBLEServer *server, NimBLEConnInfo &info) override;
  void onDisconnect(NimBLEServer *server, NimBLEConnInfo &info, int reason) override;
  void onAuthenticationComplete(NimBLEConnInfo &info) override;
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) override;

public:
  // Constructor and destructor
  BleSerialServer(const char *deviceName = DEVICE_NAME_DEFAULT);
  ~BleSerialServer();

  // Delete copy constructor and copy assignment operator to prevent copying
  BleSerialServer(const BleSerialServer &) = delete;
  BleSerialServer &operator=(const BleSerialServer &) = delete;

  void start();
  void stop();
  bool isConnected() const;
  bool sendString(const char *str, size_t len);
  void setOnConnect(void (*callback)(NimBLEConnInfo &info));
  void setOnDisconnect(void (*callback)(NimBLEConnInfo &info, int reason));
  void setOnReceiveData(void (*callback)(NimBLEConnInfo &info, const char *data, size_t len));
};