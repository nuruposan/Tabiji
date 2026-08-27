#pragma once

#include <Arduino.h>

class PushButton {
 private:
  const uint8_t _pinId;
  void (*_onPress)();
  void (*_onRelease)();
  bool _prevState = HIGH;
  bool _lastState = HIGH;
  bool _isBegun = false;

 public:
  PushButton(uint8_t pinId, void (*onPress)() = nullptr, void (*onRelease)() = nullptr);
  ~PushButton();
  void begin();
  void end();
  bool isPressed();
  bool isReleased();
  bool update();
};

class ButtonManager {
 private:
  PushButton **_buttons;
  size_t _count;
  size_t _maxButtons;

 public:
  static const size_t DEFAULT_MAX_BUTTONS = 3;
  static const int8_t BID_NULL = -1;

  ButtonManager(size_t maxButtons = DEFAULT_MAX_BUTTONS);
  ~ButtonManager();
  int8_t add(PushButton *button);
  void begin();
  void clear();
  void end();
  bool isPressed(uint8_t pinId);
  bool isReleased(uint8_t pinId);
  int8_t process();
};
