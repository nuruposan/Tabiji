#pragma once

#include <NimBLEDevice.h>

class BleSerialServer : public NimBLEServerCallbacks, public NimBLECharacteristicCallbacks {
 private:
  static constexpr size_t DEVICE_NAME_LEN = 31;               // Maximum length of the BLE device name
  static constexpr uint8_t BLE_MTU_SIZE = 251;                // MTU size to set: default is 23 bytes, up to 251 bytes
  static constexpr char DEVICE_NAME_DEFAULT[] = "ESP32-BLE";  // Default BLE device name

  // BLE service and characteristic UUIDs for the UART service
  static constexpr char SERVICE_UUID[] = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";  // UART service UUID
  static constexpr char RX_UUID[] = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";       // RX characteristic UUID
  static constexpr char TX_UUID[] = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";       // TX characteristic UUID

  // BLE advertising interval (in units of 0.625 msec)
  static constexpr uint16_t ADV_INTERVAL = 160 * 2.5;  // (0.625 * 160) * 2.5 ms = 250 ms

  char _deviceName[(DEVICE_NAME_LEN + 1)];
  NimBLECharacteristic *_txChar;
  NimBLECharacteristic *_rxChar;
  NimBLEServer *_bleServer = nullptr;
  NimBLEService *_bleService = nullptr;
  NimBLEAdvertising *_bleAdvertising = nullptr;
  bool _clientConnected = false;
  void (*_onConnect)(NimBLEConnInfo &info);
  void (*_onDisconnect)(NimBLEConnInfo &info, int reason);
  void (*_onWrite)(NimBLEConnInfo &info, const char *data);

  void onConnect(NimBLEServer *server, NimBLEConnInfo &info) override;
  void onDisconnect(NimBLEServer *server, NimBLEConnInfo &info, int reason) override;
  void onReceive(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) override;
  void sendString(char *str, size_t len);

 public:
  BleSerialServer(const char *deviceName = DEVICE_NAME_DEFAULT);
  ~BleSerialServer();

  void begin();
  void end();
  void setOnConnect(void (*callback)(NimBLEConnInfo &info));
  void setOnDisconnect(void (*callback)(NimBLEConnInfo &info, int reason));
  void setOnWrite(void (*callback)(NimBLEConnInfo &info, const char *data));
};