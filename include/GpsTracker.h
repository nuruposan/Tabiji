#pragma once

#include <Arduino.h>
#include <TinyGPSPlus.h>

typedef struct {
  double latitude;   // Latitude in decimal degrees
  double longitude;  // Longitude in decimal degrees
  uint32_t time;     // Unixtime in seconds since epoch
  float altitude;    // Altitude in meters
  float speed;       // Speed in kilometers per hour
  uint8_t valid;     // Bitmask for validity of each field (1 = valid, 0 = invalid)
                     // 0x01 = unixtime, 0x02 = location, 0x04 = altitude, 0x08 = speed
} GpsRecord;

typedef enum {
  BAUD_NO_DETECT = 0,
  BAUD_9600 = 9600,
  BAUD_19200 = 19200,
  BAUD_38400 = 38400,
  BAUD_57600 = 57600,
  BAUD_115200 = 115200
} GpsBaudRate;

class GpsTracker {
 public:
  // Default parameters
  static const uint16_t RX_BUFFER_SIZE = 1024;           // Size of the RX buffer for GNSS data in bytes
  static const uint16_t DATA_MAX_AGE = 2000;             // Maximum age of GNSS data in milliseconds
  static const uint16_t STABILIZATION_TIME = 12000;      // Time in milliseconds to consider GNSS stabilized
  static const uint8_t STABILIZATION_SAMPLES = 12;       // Number of valid readings to consider GNSS stabilized
  static const uint8_t MIN_SATELLITES = 6;               // Minimum of SATs required for a valid fix (must be >=4)
  static const uint8_t DEFAULT_RX_PIN = 43;              // Default RX pin for GNSS module
  static const uint8_t DEFAULT_TX_PIN = 44;              // Default TX pin for GNSS module
  static const uint8_t DEFAULT_SLEEP_PIN = -1;           // Default sleep pin for GNSS module (not used)
  static const uint16_t DEFAULT_UPDATE_INTERVAL = 5000;  // Default logging interval in milliseconds

  static const uint16_t UPDATE_MARGIN = 100;            // Margin in milliseconds for update interval
  static const GpsBaudRate GPS_BAUD_RATE = BAUD_19200;  // Default baud rate for GNSS module

 private:
  HardwareSerial _serial;                              // Reference to the serial port used for GNSS communication
  TinyGPSPlus _gps = TinyGPSPlus();                    // Instance of TinyGPSPlus for GNSS data parsing
  uint32_t _baudRate = BAUD_NO_DETECT;                 // Flag to indicate if the GNSS module has been detected
  bool _debugOutput = false;                           // Flag to enable or disable debug output
  bool _stabilized = false;                            // Flag to indicate if the GNSS module is stabilized
  bool _sleeping = false;                              // Flag to indicate if the device is in sleep mode
  uint8_t _txPin = DEFAULT_TX_PIN;                     // Pin used for GNSS TX (transmit)
  uint8_t _rxPin = DEFAULT_RX_PIN;                     // Pin used for GNSS RX (receive)
  uint8_t _sleepPin = DEFAULT_SLEEP_PIN;               // Pin used to control sleep mode
  uint16_t _updateInterval = DEFAULT_UPDATE_INTERVAL;  // Logging interval in milliseconds
  //  uint32_t _lastLoggingTime = 0;            // Timestamp of the last log entry
  uint32_t _stabilizeStartTime = 0;         // Timestamp when stabilization checks started
  uint8_t _stabilizeCount = 0;              // Counter for stabilization checks
  uint16_t _maxDataAge = DATA_MAX_AGE;      // Maximum age of GNSS data in milliseconds
  uint8_t _minSatellites = MIN_SATELLITES;  // Minimum number of satellites for a valid fix (>= 4)

  // Location data
  GpsRecord _firstLocation = {};  // Location at the first fix for movement detection
  GpsRecord _lastLocation = {};   // Last known location for movement detection
  uint32_t _lastUpdateTime = 0;   // Timestamp of the last location update

  // Callback function pointers for events
  void (*_onStabilized)() = nullptr;      // Callback function for stabilization event
  void (*_onLocationUpdate)() = nullptr;  // Callback function for location update event

  static uint32_t toUnixtime(TinyGPSDate &date, TinyGPSTime &time);
  void updateStabilizationStatus();
  void updateLocationData();
  void sendGnssCommand(const char *command, uint16_t waitTime = 0);
  uint32_t detectBaudRate();
  uint32_t setBaudRate(GpsBaudRate baudRate);
  void loadDataFromFile();

 public:
  GpsTracker(uint8_t rxPin = 43, uint8_t txPin = 44, uint8_t sleepPin = -1);
  ~GpsTracker();
  bool begin();
  void process();
  bool isStabilized();
  void setOnStabilized(void (*callback)());
  void setOnLocationUpdate(void (*callback)());
  GpsRecord getLastLocation() const;
  uint16_t getUpdateInterval() const;
  void setUpdateInterval(uint16_t interval);
  void setDebugOutput(bool enable);
  uint32_t getBaudRate() const;
  uint16_t getMaxDataAge() const;
  void setMaxDataAge(uint16_t maxAge);
  uint8_t getMinSatellites() const;
  void setMinSatellites(uint8_t minSat);
};