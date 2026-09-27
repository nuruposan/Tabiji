#include "StatusIndicator.h"

#include <M5AtomS3.h>

void StatusIndicator::begin(uint32_t brightness) {
  AtomS3.dis.setBrightness(brightness);
  AtomS3.dis.drawpix(0x000000);
}

void StatusIndicator::begin(uint32_t brightness, uint8_t buzzerPin) {
  begin(brightness);
  _buzzerPin = buzzerPin;

  if (_buzzerPin != 0) {
    pinMode(_buzzerPin, OUTPUT);
  }
}

void StatusIndicator::setStatus(Mode mode, uint32_t color, uint32_t intervalMs) {
  _color = color;
  _mode = mode;
  _intervalMs = intervalMs;
}

void StatusIndicator::notify(Mode mode, uint32_t color, uint32_t intervalMs, uint16_t count) {
  _notificationColor = color;
  _notificationMode = mode;
  _notificationIntervalMs = intervalMs;
  _notificationCount = count;
  _notificationCompletedCount = 0;
  _notificationActive = (count != 0);
}

void StatusIndicator::beep(uint16_t frequency, uint16_t repeat, uint16_t durationMs, uint16_t intervalMs) {
  if (repeat == 0 || durationMs == 0) {
    return;
  }

  _beepFrequency = frequency;
  _beepRemaining = repeat;
  _beepDurationMs = durationMs;
  _beepIntervalMs = intervalMs;
  _beepNextMs = millis();
  _beepPlaying = false;
  _beepActive = true;
}

void StatusIndicator::update() {
  const uint32_t now = millis();

  if (_beepActive) {
    if (_beepPlaying && static_cast<int32_t>(now - _beepNextMs) >= 0) {
      noTone(_buzzerPin);
      _beepPlaying = false;
      --_beepRemaining;
      if (_beepRemaining == 0) {
        _beepActive = false;
      } else {
        _beepNextMs = now + _beepIntervalMs;
      }
    } else if (!_beepPlaying && static_cast<int32_t>(now - _beepNextMs) >= 0) {
      tone(_buzzerPin, _beepFrequency, _beepDurationMs);
      _beepPlaying = true;
      _beepNextMs = now + _beepDurationMs;
    }
  }

  const bool notificationActive = _notificationActive;
  const uint32_t color = notificationActive ? _notificationColor : _color;
  const Mode mode = notificationActive ? _notificationMode : _mode;
  const uint32_t intervalMs = notificationActive ? _notificationIntervalMs : _intervalMs;

  if (notificationActive != _lastNotificationActive) {
    _lastNotificationActive = notificationActive;
    _lastChangeMs = now;
    _isLit = false;
    AtomS3.dis.drawpix(0x000000);
    return;
  }

  if (color != _lastColor || mode != _lastMode || intervalMs != _lastIntervalMs) {
    _lastColor = color;
    _lastMode = mode;
    _lastIntervalMs = intervalMs;
    _lastChangeMs = now;
    _isLit = false;
  }

  if (mode == Mode::OFF) {
    if (_isLit) {
      AtomS3.dis.drawpix(0x000000);
      _isLit = false;
    }
    return;
  }

  if (mode == Mode::SOLID) {
    if (!_isLit) {
      AtomS3.dis.drawpix(color);
      _isLit = true;
    }
    if (notificationActive && now - _lastChangeMs >= intervalMs) {
      _lastChangeMs = now;
      ++_notificationCompletedCount;
      if (_notificationCompletedCount >= _notificationCount) {
        _notificationActive = false;
      }
    }
    return;
  }

  if (now - _lastChangeMs >= intervalMs) {
    _lastChangeMs = now;
    _isLit = !_isLit;
    AtomS3.dis.drawpix(_isLit ? color : 0x000000);

    if (notificationActive && !_isLit) {
      ++_notificationCompletedCount;
      if (_notificationCompletedCount >= _notificationCount) {
        _notificationActive = false;
      }
    }
  }
}