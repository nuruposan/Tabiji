#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

class BleSerialConnection : public NimBLEServerCallbacks,
                            public NimBLECharacteristicCallbacks,
                            public NimBLEClientCallbacks,
                            public NimBLEScanCallbacks {
 public:
  enum class Mode {
    BLE_SERIAL_SERVER,  // run as a server
    BLE_SERIAL_CLIENT   // run as a client
  };

  // Constructor for the BLE serial connection, allowing an optional custom device name
  BleSerialConnection(Mode mode, const char *deviceName = DEVICE_NAME_DEFAULT);
  ~BleSerialConnection();

  // Delete copy constructor and copy assignment operator to prevent copying of the BLE serial connection object
  BleSerialConnection(const BleSerialConnection &) = delete;
  BleSerialConnection &operator=(const BleSerialConnection &) = delete;

  void start(uint32_t scanTimeMs = 5000);
  void stop();
  bool isConnected() const;
  bool sendString(const char *str, size_t len);
  void setOnConnect(void (*callback)(NimBLEConnInfo &info));
  void setOnDisconnect(void (*callback)(NimBLEConnInfo &info, int reason));
  void setOnReceive(void (*callback)(NimBLEConnInfo &info, const char *data, size_t len));

 private:
  // Default device names for the BLE connection
  static constexpr const char *DEVICE_NAME_DEFAULT = "ESP32-BLE";
  static constexpr const char *CLIENT_NAME = "ESP32-BLE-Client";
  static constexpr size_t DEVICE_NAME_LEN = 31;

  // Preferred MTU size for the BLE connection
  static constexpr uint8_t MTU_SIZE_PREFFERED = 251;

  // BLE service and characteristic UUIDs
  static constexpr const char *SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
  static constexpr const char *RX_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
  static constexpr const char *TX_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

  // Advertising interval in units of 0.625 msec
  static constexpr uint16_t ADV_INTERVAL = 160 * 5 / 2;  // 250 msec

  Mode _mode;
  char _deviceName[DEVICE_NAME_LEN + 1];

  // BLE server objects and characteristics
  NimBLEServer *_bleServer = nullptr;
  NimBLEService *_bleService = nullptr;
  NimBLEAdvertising *_bleAdvertising = nullptr;
  NimBLECharacteristic *_txChar = nullptr;
  NimBLECharacteristic *_rxChar = nullptr;

  // BLE client objects and remote characteristics
  NimBLEClient *_bleClient = nullptr;
  NimBLERemoteCharacteristic *_remoteTxChar = nullptr;
  NimBLERemoteCharacteristic *_remoteRxChar = nullptr;

  bool _running = false;
  bool _connected = false;
  uint16_t _clientConnHandle = BLE_HS_CONN_HANDLE_NONE;

  void (*_onConnect)(NimBLEConnInfo &info) = nullptr;
  void (*_onDisconnect)(NimBLEConnInfo &info, int reason) = nullptr;
  void (*_onReceive)(NimBLEConnInfo &info, const char *data, size_t len) = nullptr;

  void onConnect(NimBLEServer *server, NimBLEConnInfo &info) override;
  void onDisconnect(NimBLEServer *server, NimBLEConnInfo &info, int reason) override;
  void onReceive(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) override;

  void onConnect(NimBLEClient *client) override;
  void onConnectFail(NimBLEClient *client, int reason) override;
  void onDisconnect(NimBLEClient *client, int reason) override;
  void onAuthenticationComplete(NimBLEConnInfo &info) override;
  void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override;
  void onScanEnd(const NimBLEScanResults &results, int reason) override;
  void onNotify(NimBLERemoteCharacteristic *characteristic, uint8_t *data, size_t len, bool isNotify);

  bool connectToServer(const NimBLEAdvertisedDevice *advertisedDevice);
  bool setupCharacteristics();
};
