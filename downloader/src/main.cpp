#include <Arduino.h>
#include <M5AtomS3.h>
#include <SPI.h>

#include "BleSerialConnection.h"
#include "FS.h"
#include "SD.h"
#include "StatusIndicator.h"

constexpr int PIN_SPI_MOSI = 6;
constexpr int PIN_SPI_SCK = 7;
constexpr int PIN_SPI_MISO = 8;
constexpr int PIN_SD_CS = 4;
constexpr int PIN_BUZZER = 38;

constexpr int SD_ACCESS_SPEED = 15000000;  // SD card access speed in Hz

constexpr uint32_t COLOR_WHITE = 0xFFFFFF;   // White color for general indication
constexpr uint32_t COLOR_YELLOW = 0xFFFF00;  // Yellow color for general indication
constexpr uint32_t COLOR_RED = 0xFF0000;     // Red color for error indication
constexpr uint32_t COLOR_GREEN = 0x00FF00;   // Green color for success indication
constexpr uint32_t COLOR_BLUE = 0x0000FF;    // Blue color for indication
constexpr uint32_t COLOR_NONE = 0x000000;    // No color (LED off)

// Beep configuration constants
static const uint16_t BEEP_FREQ_HIGH = 2000;
static const uint16_t BEEP_FREQ_LOW = 1000;
static const uint16_t BEEP_DURATION = 75;
static const uint16_t BEEP_INTERVAL = 100;

BleSerialConnection *bleClient;
StatusIndicator indicator;
hw_timer_t *timer = nullptr;
volatile bool indicatorUpdateRequested = false;
uint32_t buttonPressStart = 0;
bool longPressNotified = false;
bool longPressProcessed = false;

void onBleConnect(NimBLEConnInfo &info);
void onBleConnecting(const NimBLEAdvertisedDevice &advertisedDevice);
void onBleConnectFail(NimBLEConnInfo &info, int reason);
void onBleDisconnect(NimBLEConnInfo &info, int reason);
void onBleReceive(NimBLEConnInfo &info, const char *data, size_t len);
void IRAM_ATTR onTimer();
void halt(uint8_t errorCode);

void setup() {
  Serial.begin(115200);
  delay(1000);  // Small delay to allow serial to initialize

  AtomS3.begin(true);  // Init M5AtomS3Lite.

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, false);
  timerAlarmWrite(timer, 10000, true);
  timerAlarmEnable(timer);

  indicator.begin(32, PIN_BUZZER);

  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SD_CS);
  if (!SD.begin(PIN_SD_CS, SPI, SD_ACCESS_SPEED)) {
    Serial.println("TF card mount failed");
    halt(0x02);
  }

  const uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No TF card detected");
    halt(0x03);
  }
  Serial.printf("TF card type: %u, size: %llu MB\n", cardType, SD.cardSize() / (1024ULL * 1024ULL));

  bleClient = new BleSerialConnection(BleSerialConnection::ConnectionMode::MODE_CLIENT);
  bleClient->setOnConnect(onBleConnect);
  bleClient->setOnConnecting(onBleConnecting);
  bleClient->setOnConnectFail(onBleConnectFail);
  bleClient->setOnDisconnect(onBleDisconnect);
  bleClient->setOnReceive(onBleReceive);
  bleClient->start();

  indicator.setStatus(StatusIndicator::Mode::BLINK, COLOR_GREEN, 800);
  indicator.beep(BEEP_FREQ_HIGH, 1, 800);
}

void loop() {
  AtomS3.update();
  bleClient->process();

  if (indicatorUpdateRequested) {
    indicatorUpdateRequested = false;
    indicator.update();
  }

  if (AtomS3.BtnA.wasPressed()) {
    indicator.setStatus(StatusIndicator::Mode::SOLID, COLOR_GREEN);
    indicator.update();

    buttonPressStart = millis();

  } else if (AtomS3.BtnA.isHolding()) {
    uint32_t buttonPressDuration = millis() - buttonPressStart;
    if ((!longPressProcessed) && (buttonPressDuration >= 6000)) {
      indicator.notify(StatusIndicator::Mode::BLINK, COLOR_WHITE, 80, 2);
      indicator.beep(BEEP_FREQ_HIGH, 1, 1000);

      longPressProcessed = true;

    } else if ((!longPressNotified) && (buttonPressDuration >= 1000)) {
      indicator.beep(BEEP_FREQ_LOW, 16, 500, 300);
      indicator.notify(StatusIndicator::Mode::SOLID, COLOR_YELLOW, 6000);

      longPressNotified = true;
    }

  } else if (AtomS3.BtnA.wasReleased()) {
    uint32_t buttonPressDuration = millis() - buttonPressStart;

    if (buttonPressDuration < 1000) {
      indicator.notify(StatusIndicator::Mode::BLINK, COLOR_WHITE, 80, 2);
      indicator.beep(BEEP_FREQ_HIGH);

    } else if (!longPressProcessed) {
      indicator.notify(StatusIndicator::Mode::BLINK, COLOR_RED, 80, 2);
      indicator.beep(500, 3, 75, 50);
    }

    buttonPressStart = 0;
    longPressProcessed = false;
    longPressNotified = false;
  }

  delay(10);
}

void halt(uint8_t errorCode) {
  Serial.printf("[SYS.err] System halted with error code: 0x%02X\n", errorCode);

  indicator.beep(BEEP_FREQ_LOW, 5);  // Beep to indicate an error state
  indicator.setStatus(StatusIndicator::Mode::BLINK, COLOR_RED, 800);

  while (true) {
    indicator.update();
    indicatorUpdateRequested = false;
    delay(10);
  }
}

void onBleConnect(NimBLEConnInfo &info) {
  indicator.setStatus(StatusIndicator::Mode::BLINK, COLOR_BLUE, 200);
}

void onBleConnecting(const NimBLEAdvertisedDevice &advertisedDevice) {
  indicator.setStatus(StatusIndicator::Mode::BLINK, COLOR_BLUE, 10);
}

void onBleConnectFail(NimBLEConnInfo &info, int reason) {
  indicator.notify(StatusIndicator::Mode::BLINK, COLOR_RED, 100, 3);
  indicator.setStatus(StatusIndicator::Mode::BLINK, COLOR_GREEN, 800);
}

void onBleDisconnect(NimBLEConnInfo &info, int reason) {
  indicator.notify(StatusIndicator::Mode::BLINK, COLOR_RED, 100, 3);
  indicator.setStatus(StatusIndicator::Mode::BLINK, COLOR_GREEN, 800);
}

void onBleReceive(NimBLEConnInfo &info, const char *data, size_t len) {
}

void IRAM_ATTR onTimer() {
  // This function is called periodically by a timer.
  indicatorUpdateRequested = true;
}
