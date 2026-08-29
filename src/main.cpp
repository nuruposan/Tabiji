#include <Arduino.h>

#include "ButtonManager.h"
#include "GpsTracker.h"
#include "LogStore.h"

// Application version number (major, minor, patch, build)
static const char APP_NAME[] = "Tabiji";         // YYYYMMDD format
static const uint32_t APP_VERSION = 0x20260827;  // YYYYMMDD format

// Debugging and serial communication constants
static const uint32_t DEBUG_SERIAL_BAUD = 115200;
static const uint32_t SERIAL_INIT_DELAY = 100;

// Pin definitions for the connected hardware components
static const uint8_t PIN_GPS_RX = 44;
static const uint8_t PIN_GPS_TX = 43;
static const uint8_t PIN_GPS_WUP = 6;
static const uint8_t PIN_BUZZER = 1;
static const uint8_t PIN_BUTTON1 = 2;
static const uint8_t PIN_BUTTON2 = 3;
static const uint8_t PIN_BUTTON3 = 4;
static const uint8_t PIN_USER_LED = 21;

// Log storage configuration constants
static const uint32_t LOG_UPDATE_INTERVAL = 15000;

// Buffer depth configuration for log storage
static const uint8_t LOG_ENTRY_SIZE = sizeof(GpsRecord);
static const uint8_t WRITE_BUF_DEPTH = 8;
static const uint8_t READ_BUF_DEPTH = 16;

// Beep configuration constants
static const uint16_t BEEP_FREQ_HIGH = 2000;
static const uint16_t BEEP_FREQ_LOW = 1000;
static const uint16_t BEEP_DURATION = 75;
static const uint16_t BEEP_INTERVAL = 100;

// Error codes for system halt conditions
static const uint8_t ERROR_CODE_GNSS_INIT_FAILED = 0x02;
static const uint8_t ERROR_CODE_FLASH_INIT_FAILED = 0x03;

void blinkLed(uint8_t repeat = 1, uint16_t duration = 200, uint16_t interval = 200);
void beep(uint16_t frequency,
    uint16_t repeat = 1,  //
    uint16_t duration = BEEP_DURATION, uint16_t interval = BEEP_INTERVAL);
void onButton1Pressed();
void onButton2Pressed();
void onButton3Pressed();
void onGnssStabilized();
void onLocationUpdate();
void enableGnssModule(bool enable);
void enableDebugOutput(bool enable);
char *toISO8601DateTime(uint32_t unixtime, char *buffer, size_t bufferSize);
uint32_t toUnixtime(TinyGPSDate &date, TinyGPSTime &time);
void halt(uint8_t errorCode = 1);

GpsTracker tracker = GpsTracker(PIN_GPS_RX, PIN_GPS_TX, PIN_GPS_WUP);  // Initialize the GNSS tracker
ButtonManager buttons = ButtonManager(3);                              // Manage up to 3 buttons
LogStore logStore = LogStore(APP_VERSION);                             // Initialize the log storage with a magic number
bool gnssEnabled = false;                                              // Track the GNSS module's enable state
bool gnssDebugging = false;                                            // Track the GNSS debugging state

void blinkLed(uint8_t repeat, uint16_t duration, uint16_t interval) {
  for (uint8_t i = 0; i < repeat; i++) {
    digitalWrite(PIN_USER_LED, LOW);
    delay(duration);
    digitalWrite(PIN_USER_LED, HIGH);

    // if this is not the last blink, wait for the interval before the next
    if (i < (repeat - 1)) delay(interval);
  }
}

void beep(uint16_t frequency, uint16_t repeat, uint16_t duration, uint16_t interval) {
  for (uint16_t i = 0; i < repeat; i++) {
    // play a beep in the specified frequency and duration
    tone(PIN_BUZZER, frequency, duration);

    // if this is not the last beep, wait for the interval before the next
    if (i < (repeat - 1)) delay(interval);
  }
}

void onButton1Pressed() {  // left button
  beep(BEEP_FREQ_HIGH);

  Serial.print("<gpx>\n");
  Serial.print("<trk>\n");
  Serial.print("<trkseg>\n");
  char buffer[32];  // Buffer for ISO 8601 date-time conversion
  GpsRecord *gr = (GpsRecord *)logStore.readFirst();
  while (gr != nullptr) {
    Serial.printf("<trkpt lon=\"%.6f\" lat=\"%.6f\">", gr->longitude, gr->latitude);
    Serial.printf("<time>%s</time>", toISO8601DateTime(gr->time, buffer, sizeof(buffer)));
    if (gr->valid & 0x04) Serial.printf("<ele>%.2f</ele>", gr->altitude);
    if (gr->valid & 0x08) Serial.printf("<speed>%.2f</speed>", gr->speed);
    Serial.println("</trkpt>");

    gr = (GpsRecord *)logStore.readNext();
  }
  Serial.print("</trkseg>\n");
  Serial.print("</trk>\n");
  Serial.print("</gpx>\n");
}

void onButton2Pressed() {  // middle button
  beep(BEEP_FREQ_HIGH, 2);

  Serial.println("onButton2Pressed: Dumping log storage contents.");

  // Display the log storage header
  logStore.dump();
  return;

  // Ensure any buffered logs are written to the file
  logStore.flush();

  // Read and print all log entries from the log storage
  uint32_t entryCount = logStore.entryCount();
  GpsRecord *location = (GpsRecord *)logStore.readFirst();
  uint32_t i = 0;
  while (location != nullptr) {
    Serial.printf(
        "entry[%lu]: Time: %lu, Location: (%.6f, %.6f), Alt: %.2f m, "  //
        "Spd: %.2f km/h, Valid: 0x%02X\n",                              //
        i, location->time, location->latitude, location->longitude,     //
        location->altitude, location->speed, location->valid);

    location = (GpsRecord *)logStore.readNext();
    i++;
  }
}

void onButton3Pressed() {  // right (red) button
  beep(BEEP_FREQ_LOW, 3);
  delay(1000);

  Serial.println("onButton3Pressed: System reset requested.");

  logStore.flush();  // Ensure any buffered logs are written to the file
  esp_restart();     // Reset the ESP32
}

void onGnssStabilized() {
  Serial.println("onGnssStabilized: GNSS module stabilized. Ready to log location data.");
  beep(BEEP_FREQ_HIGH, 2);
  delay(500);
}

void onLocationUpdate() {
  // Print the new location data to the serial console
  GpsRecord location = tracker.getLastLocation();
  logStore.append(&location);  // Append the new location data to the log storage

  beep(BEEP_FREQ_LOW);
}

/**
 * Sets the GNSS enable pin to the desired state.
 * @param enable true to enable the GNSS module (set PIN_GPS_WUP HIGH), false to
 * disable it (set PIN_GPS_WUP LOW).
 */
void enableGnssModule(bool enable) {
  // Do nothing if the GNSS module is already in the desired state
  if (gnssEnabled == enable) return;

  // Set the GNSS enable pin to the desired state
  gnssEnabled = enable;
  digitalWrite(PIN_GPS_WUP, (uint8_t)enable);
}

void enableDebugOutput(bool enable) {
  gnssDebugging = enable;
  tracker.setDebugOutput(enable);
}

// Convert a Unix timestamp to an ISO 8601 date-time string
char *toISO8601DateTime(uint32_t unixtime, char *buffer, size_t bufferSize) {
  // Convert the Unix timestamp to a struct tm
  time_t rawtime = (time_t)unixtime;
  struct tm *timeinfo = gmtime(&rawtime);

  // Format the date and time as ISO 8601
  snprintf(buffer, bufferSize, "%04d-%02d-%02dT%02d:%02d:%02dZ",          //
      timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,  //
      timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

  return buffer;
}

// Convert TinyGPSDate and TinyGPSTime to a Unix timestamp
uint32_t toUnixtime(TinyGPSDate &date, TinyGPSTime &time) {
  // Convert the date and time to a Unix timestamp
  struct tm t;
  t.tm_year = (date.year() - 1900);  // tm_year is years since 1900
  t.tm_mon = (date.month() - 1);     // tm_mon is months since January (0-11)
  t.tm_mday = date.day();
  t.tm_hour = time.hour();
  t.tm_min = time.minute();
  t.tm_sec = time.second();
  t.tm_isdst = -1;  // Not considering daylight saving time

  return mktime(&t);
}

void reload() {
  Serial.println("[SYS.warn] Reloading system...");

  // Clean up resources before restarting
  logStore.~LogStore();
  tracker.~GpsTracker();
  buttons.~ButtonManager();
  Serial.end();

  // Wait a moment to ensure all resources are released
  delay(100);

  // Restart the ESP32
  esp_restart();
}

void halt(uint8_t errorCode) {
  Serial.printf("[SYS.err] System halted with error code: 0x%02X\n", errorCode);

  beep(BEEP_FREQ_LOW, 5);  // Beep to indicate an error state

  while (true) {
    blinkLed(3, 50, 50);
    delay(500);
    blinkLed(errorCode, 200, 200);  // Blink the LED to indicate an error state
    delay(3000);
  }
}

void setup() {
  // Initialize the serial ports for debugging and GNSS communication
  Serial.begin(DEBUG_SERIAL_BAUD);
  delay(SERIAL_INIT_DELAY);  // Allow time for the serial port to initialize

  // set up the GNSS tracker instance
  if (!tracker.begin()) {  // Initialize the GNSS tracker
    Serial.println("[SYS.err] Failed to initialize GNSS module");
    halt(ERROR_CODE_GNSS_INIT_FAILED);  // Halt the system if the GNSS tracker fails to initialize
  }
  tracker.setOnStabilized(onGnssStabilized);       // Set the callback for when the  GNSS module is stabilized
  tracker.setOnLocationUpdate(onLocationUpdate);   // Set the callback for when a new location update is available
  tracker.setUpdateInterval(LOG_UPDATE_INTERVAL);  // Set the default update interval to 15 seconds

  // enable builtin LED pin
  pinMode(PIN_USER_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  // enable GNSS module
  pinMode(PIN_GPS_WUP, OUTPUT);  // HIGH to enable, LOW to disable
  enableGnssModule(true);

  if (!logStore.begin(LOG_ENTRY_SIZE, WRITE_BUF_DEPTH, READ_BUF_DEPTH)) {  // Initialize the log storage
    Serial.println("[SYS.err] Failed to initialize log storage");
    halt(ERROR_CODE_FLASH_INIT_FAILED);  // Halt the system if the log storage fails to initialize
  }

  // set up the buttons
  buttons.begin();
  buttons.add(new PushButton(PIN_BUTTON1, onButton1Pressed));
  buttons.add(new PushButton(PIN_BUTTON2, onButton2Pressed));
  buttons.add(new PushButton(PIN_BUTTON3, onButton3Pressed));

  // beep to indicate that the system has started successfully
  beep(BEEP_FREQ_LOW, 1, 500);
}

void loop() {
  digitalWrite(PIN_USER_LED, LOW);  // Turn off the status LED to indicate processing

  // Read data from the GNSS module and feed it to the TinyGPS,
  // then check for stabilization and location updates
  tracker.process();
  buttons.process();

  digitalWrite(PIN_USER_LED, HIGH);  // Turn on the status LED after processing
  delay(10);
}