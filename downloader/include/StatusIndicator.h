#pragma once

#include <Arduino.h>

class StatusIndicator {
 public:
  StatusIndicator() = default;
  StatusIndicator(const StatusIndicator &) = delete;
  StatusIndicator &operator=(const StatusIndicator &) = delete;

  enum class Mode : uint8_t {
    OFF,
    SOLID,
    BLINK,
  };

  void begin(uint32_t brightness);
  void begin(uint32_t brightness, uint8_t buzzerPin);
  void setStatus(Mode mode, uint32_t color = 0xA0A0A0, uint32_t intervalMs = 500);
  void notify(Mode mode, uint32_t color = 0xA0A0A0, uint32_t intervalMs = 200, uint16_t count = 1);
  void beep(uint16_t frequency, uint16_t repeat = 1, uint16_t durationMs = 75, uint16_t intervalMs = 100);
  void update();

 private:
  volatile uint32_t _color = 0;
  volatile Mode _mode = Mode::OFF;
  volatile uint32_t _intervalMs = 500;
  volatile bool _notificationActive = false;
  volatile uint32_t _notificationColor = 0;
  volatile Mode _notificationMode = Mode::OFF;
  volatile uint32_t _notificationIntervalMs = 500;
  volatile uint16_t _notificationCount = 0;
  volatile uint16_t _notificationCompletedCount = 0;
  bool _lastNotificationActive = false;
  uint32_t _lastColor = 0;
  Mode _lastMode = Mode::OFF;
  uint32_t _lastIntervalMs = 0;
  uint32_t _lastChangeMs = 0;
  bool _isLit = false;
  uint8_t _buzzerPin = 0;
  bool _beepActive = false;
  bool _beepPlaying = false;
  uint16_t _beepFrequency = 0;
  uint16_t _beepRemaining = 0;
  uint16_t _beepDurationMs = 0;
  uint16_t _beepIntervalMs = 0;
  uint32_t _beepNextMs = 0;
};