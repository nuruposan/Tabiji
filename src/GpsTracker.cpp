#include "GpsTracker.h"

GpsTracker::GpsTracker(uint8_t rxPin, uint8_t txPin, uint8_t sleepPin)
    : _serial(HardwareSerial(1)), _txPin(txPin), _rxPin(rxPin), _sleepPin(sleepPin) {
  // Initialize the serial port for GNSS communication
  _serial.setRxBufferSize(RX_BUFFER_SIZE);  // Increase the RX buffer size
  _serial.begin(BAUD_9600, SERIAL_8N1, _rxPin, _txPin);
}

GpsTracker::~GpsTracker() {
  // Clean up resources if necessary
  _serial.~HardwareSerial();
  _gps.~TinyGPSPlus();
}

void GpsTracker::process() {
  // Return early if there is no data to read from the GNSS module
  if (_serial.available() == 0) return;

  // Feed incoming data from the GNSS module to the TinyGPSPlus parser
  while (_serial.available() > 0) {
    char c = _serial.read();
    _gps.encode(c);

    // Echo the incoming data to the debug serial if debug output is enabled
    if (_debugOutput) Serial.write(c);
  }

  // Update the last known location data
  updateLocationData();
}

/**
 */
void GpsTracker::updateStabilizationStatus() {
  // return early if already stabilized or no new satellite data
  if (_stabilized) return;                   // Already stabilized
  if (!_gps.satellites.isUpdated()) return;  // No new data

  // Update the stabilized flag based on the GNSS data
  bool gnssFixed = (_gps.satellites.isValid()                  //
                    && (_gps.satellites.age() <= _maxDataAge)  //
                    && (_gps.satellites.value() >= _minSatellites));

  // If not fixed, reset the stabilization timer and count
  if (!gnssFixed) {
    _stabilizeStartTime = 0;
    _stabilizeCount = 0;

    return;
  }

  // Update stabilization timer and count
  uint32_t now = millis();
  if (_stabilizeStartTime == 0) {  // new stabilization check started
    _stabilizeStartTime = now;
    _stabilizeCount = 0;
  }
  _stabilizeCount += 1;

  // Update the stabilized flag based on the stabilization count and duration
  _stabilized = (_stabilizeCount >= STABILIZATION_SAMPLES  //
                 && (now - _stabilizeStartTime) >= STABILIZATION_TIME);

  // If stabilized, call the onStabilized callback if set
  if (_stabilized && _onStabilized) {
    _onStabilized();
  }
}

uint32_t GpsTracker::toUnixtime(TinyGPSDate &date, TinyGPSTime &time) {
  struct tm tm_time;
  tm_time.tm_year = date.year() - 1900;  // tm_year is years since 1900
  tm_time.tm_mon = date.month() - 1;     // tm_mon is months since January (0-11)
  tm_time.tm_mday = date.day();          // tm_mday is day of the month (1-31)
  tm_time.tm_hour = time.hour();
  tm_time.tm_min = time.minute();
  tm_time.tm_sec = time.second();
  tm_time.tm_isdst = -1;  // Not considering daylight saving time

  return mktime(&tm_time);  // Convert to Unix timestamp
}

void GpsTracker::updateLocationData() {
  // return early if not stabilized or no new location data
  if (!isStabilized()) return;
  if ((!_gps.location.isUpdated()) || (_gps.location.age() > _maxDataAge)) return;

  // Update the location data based on the latest GNSS data
  bool satValid = (_gps.satellites.isValid() && (_gps.satellites.age() < _maxDataAge)  //
                   && (_gps.satellites.value() >= _minSatellites));
  bool timeValid = (_gps.time.isValid() && (_gps.time.age() < _maxDataAge));
  bool locationValid = (_gps.location.isValid() && (_gps.location.age() < _maxDataAge));
  bool altitudeValid = (_gps.altitude.isValid() && (_gps.altitude.age() < _maxDataAge));
  bool speedValid = (_gps.speed.isValid() && (_gps.speed.age() < _maxDataAge));
  bool entryValid = (satValid && timeValid && locationValid);  // altitude and speed are optional

  // return early if not all fields are valid
  if (!entryValid) return;

  // update the last known location data
  GpsRecord newrec;
  newrec.time = toUnixtime(_gps.date, _gps.time);
  newrec.latitude = _gps.location.lat();
  newrec.longitude = _gps.location.lng();
  newrec.altitude = _gps.altitude.meters();
  newrec.speed = _gps.speed.kmph();
  newrec.valid = (timeValid ? 0x01 : 0)        // Timestamp
                 | (locationValid ? 0x02 : 0)  // Latitude and Longitude
                 | (altitudeValid ? 0x04 : 0)  // Altitude
                 | (speedValid ? 0x08 : 0);    // Speed
  uint32_t now = millis();

  // Record the first location data if not recorded yet
  if (_firstLocation.time == 0) {
    // Store the first location data for movement detection
    _firstLocation = newrec;
  }

  // Update the last known location data
  uint32_t timeDelta = now - _lastUpdateTime;
  if (timeDelta >= max(0, _updateInterval - UPDATE_MARGIN)) {  // Allow a small margin for timing inaccuracies
    _lastLocation = newrec;
    _lastUpdateTime = now;

    // Call the onLocationUpdate callback if set
    if (_onLocationUpdate) _onLocationUpdate();
  }
}

bool GpsTracker::isStabilized() {
  // Ensure the stabilization status is up to date
  if (!_stabilized) updateStabilizationStatus();

  return _stabilized;
}

void GpsTracker::setOnStabilized(void (*callback)()) {
  // Set the callback function to be called when the GNSS module is stabilized
  _onStabilized = callback;
}

void GpsTracker::setOnLocationUpdate(void (*callback)()) {
  // Set the callback function to be called when new location data is available
  _onLocationUpdate = callback;
}

GpsRecord GpsTracker::getLastLocation() const {
  // Return the last known location data
  return _lastLocation;
}

uint16_t GpsTracker::getUpdateInterval() const {
  // Return the current update interval for location data in milliseconds
  return _updateInterval;
}

void GpsTracker::setUpdateInterval(uint16_t interval) {
  // Set the update interval for location data in milliseconds
  _updateInterval = interval;
}

void GpsTracker::setDebugOutput(bool enable) {
  // Enable or disable debug output for incoming GNSS data
  _debugOutput = enable;
}

bool GpsTracker::begin() {
  detectBaudRate();
  if (_baudRate == BAUD_NO_DETECT) {
    Serial.println("[GNSS.err] GNSS module not detected. Please check the connections.");
    return false;
  }

  if (_baudRate != GPS_BAUD_RATE) {
    setBaudRate(GPS_BAUD_RATE);  // Set the baud rate to the specified value
  }

  return true;
}

uint32_t GpsTracker::detectBaudRate() {
  // List of common baud rates to test
  const uint32_t testDuration = 1100;  // Duration to test each baud rate in milliseconds
  const GpsBaudRate baudRates[] = {
      BAUD_9600,    // default baud rate of 'L76K GNSS module for XIAO'
      BAUD_115200,  // default baud rate of 'Unit GPS v1.1 for M5Stack series'
      BAUD_19200,
      BAUD_38400,
      BAUD_57600,
  };
  const size_t numBaudRates = sizeof(baudRates) / sizeof(baudRates[0]);

  uint32_t oldBaudRate = _serial.baudRate();

  bool detected = false;
  Serial.printf("[GNSS.info] Detecting GNSS module at baud rate: ");
  for (size_t i = 0; i < numBaudRates; i++) {
    uint32_t baudRate = (uint32_t)baudRates[i];

    // Reinitialize the serial port with the current baud rate to test
    _serial.end();
    _serial.begin(baudRate, SERIAL_8N1, _rxPin, _txPin);

    Serial.printf("%lu", baudRate);

    // Clear any existing data in the serial buffer
    while (_serial.available()) _serial.read();

    // Wait for a short period to allow the GNSS module to send data
    unsigned long startTime = millis();
    while ((!detected) && ((millis() - startTime) <= testDuration)) {  // Wait for up to 1 second
      char buf[3] = {};
      while ((!detected) && _serial.available()) {
        buf[0] = buf[1];
        buf[1] = buf[2];
        buf[2] = _serial.read();

        // Check if the received data matches the expected NMEA sentence format
        detected = (buf[0] == '$')                        //
                   && (buf[1] >= 'A') && (buf[1] <= 'Z')  //
                   && (buf[2] >= 'A') && (buf[2] <= 'Z');
      }

      delay(10);
    }

    if (detected) {
      Serial.printf(" -> detected\n");
      _baudRate = (uint32_t)baudRates[i];
      break;
    } else {
      Serial.printf(", ");
    }
  }

  if (!detected) {
    Serial.printf(" NOT DETECTED!\n");
    _baudRate = (uint32_t)BAUD_NO_DETECT;

    _serial.end();
    _serial.begin(oldBaudRate, SERIAL_8N1, _rxPin, _txPin);
  }

  return _baudRate;
}

void GpsTracker::sendGnssCommand(const char *command, uint16_t waitTime) {
  char checksum = 0;
  char *p = (char *)command;
  while (*p != '\0') {
    checksum ^= *p++;
  }

  char buffer[128];
  snprintf(buffer, sizeof(buffer), "$%s*%02X\r\n", command, checksum);
  _serial.write(buffer);

  Serial.printf("[GNSS.debug] Send command: %s", buffer);

  // Wait for the specified time to allow the GNSS module to process the command
  if (waitTime > 0) delay(waitTime);
}

uint32_t GpsTracker::setBaudRate(GpsBaudRate baudRate) {
  // Send the command to set the GNSS module's baud rate
  uint8_t baudIndex = 1;
  switch (baudRate) {
  case BAUD_115200:
    baudIndex = 5;
    break;
  case BAUD_57600:
    baudIndex = 4;
    break;
  case BAUD_38400:
    baudIndex = 3;
    break;
  case BAUD_19200:
    baudIndex = 2;
    break;
  case BAUD_9600:
  default:
    baudIndex = 1;
    break;
  }

  // Send the command to the GNSS module to change its baud rate
  char buf[64];
  sprintf(buf, "PCAS01,%d", baudIndex);
  sendGnssCommand(buf, 100);

  // Reinitialize the serial port with the new baud rate
  _baudRate = (uint32_t)baudRate;
  _serial.end();
  _serial.begin(_baudRate, SERIAL_8N1, _rxPin, _txPin);

  // Print the new baud rate to the debug serial
  Serial.printf("[GNSS.notice] GNSS module baud rate set to: %lu\n", _baudRate);

  return _baudRate;
}

uint32_t GpsTracker::getBaudRate() const {
  return _baudRate;
}

uint16_t GpsTracker::getMaxDataAge() const {
  return _maxDataAge;
}

void GpsTracker::setMaxDataAge(uint16_t maxAge) {
  _maxDataAge = maxAge;
}

uint8_t GpsTracker::getMinSatellites() const {
  return _minSatellites;
}

void GpsTracker::setMinSatellites(uint8_t minSat) {
  _minSatellites = max((int)MIN_SATELLITES, (int)minSat);
}
