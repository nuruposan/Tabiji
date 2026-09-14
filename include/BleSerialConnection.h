#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

class BleSerialConnection : public NimBLEServerCallbacks,
                            public NimBLECharacteristicCallbacks,
                            public NimBLEClientCallbacks,
                            public NimBLEScanCallbacks {
 public:
  enum class ConnectionMode {
    MODE_SERVER,  // run as a server
    MODE_CLIENT   // run as a client
  };

  // Constructor for the BLE serial connection, allowing an optional custom device name
  BleSerialConnection(ConnectionMode mode, const char *deviceName = SERVER_NAME_DEFAULT);
  ~BleSerialConnection();

  // Delete copy constructor and copy assignment operator to prevent copying of the BLE serial connection object
  BleSerialConnection(const BleSerialConnection &) = delete;
  BleSerialConnection &operator=(const BleSerialConnection &) = delete;

  void start(uint32_t scanTimeMs = 5000);
  void process();
  void stop();
  bool isConnected() const;
  bool sendString(const char *str, size_t len);
  void setOnConnect(void (*callback)(NimBLEConnInfo &info));
  void setOnConnecting(void (*callback)(const NimBLEAdvertisedDevice &advertisedDevice));
  void setOnConnectFail(void (*callback)(NimBLEConnInfo &info, int reason));
  void setOnDisconnect(void (*callback)(NimBLEConnInfo &info, int reason));
  void setOnReceive(void (*callback)(NimBLEConnInfo &info, const char *data, size_t len));

 private:
  // Default device names for the BLE connection
  static constexpr const char *SERVER_NAME_DEFAULT = "ESP32-BLE-Server";
  static constexpr const char *CLIENT_NAME_DEFAULT = "ESP32-BLE-Client";
  static constexpr size_t DEVICE_NAME_LEN = 31;

  // Transfer size calculations for the BLE connection
  static constexpr size_t ATT_HEADER_SIZE = 3;
  static constexpr size_t MTU_SIZE_PREFERRED = 251;
  static constexpr size_t MAX_TRANSFER_SIZE = (MTU_SIZE_PREFERRED - ATT_HEADER_SIZE);

  // BLE service and characteristic UUIDs
  static constexpr const char *SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
  static constexpr const char *RX_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
  static constexpr const char *TX_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

  // Advertising interval in units of 0.625 msec
  static constexpr uint16_t ADV_INTERVAL = 160 * 5 / 2;  // 250 msec

  ConnectionMode _mode;
  char _serverName[DEVICE_NAME_LEN + 1];

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
  bool _connectPending = false;
  const NimBLEAdvertisedDevice *_pendingDevice = nullptr;
  uint16_t _clientConnHandle = BLE_HS_CONN_HANDLE_NONE;

  void (*_onConnect)(NimBLEConnInfo &info) = nullptr;
  void (*_onConnecting)(const NimBLEAdvertisedDevice &advertisedDevice) = nullptr;
  void (*_onConnectFail)(NimBLEConnInfo &info, int reason) = nullptr;
  void (*_onDisconnect)(NimBLEConnInfo &info, int reason) = nullptr;
  void (*_onReceive)(NimBLEConnInfo &info, const char *data, size_t len) = nullptr;

  void onConnect(NimBLEServer *server, NimBLEConnInfo &info) override;
  void onDisconnect(NimBLEServer *server, NimBLEConnInfo &info, int reason) override;
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) override;

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
