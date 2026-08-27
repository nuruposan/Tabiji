#include <ButtonManager.h>

PushButton::PushButton(uint8_t pinId, void (*onPress)(), void (*onRelease)())
    : _pinId(pinId), _onPress(onPress), _onRelease(onRelease), _prevState(HIGH), _lastState(HIGH), _isBegun(false) {
  // Initialize the button pin as input with pull-up resistor
  begin();
}

PushButton::~PushButton() {
  // Change the button pin back to input mode without pull-up resistor
  end();
}

void PushButton::begin() {
  // Set the button pin as input with pull-up resistor
  pinMode(_pinId, INPUT_PULLUP);
  // Read the initial state of the button
  bool initialState = digitalRead(_pinId);
  _prevState = initialState;
  _lastState = initialState;
}

/**
 * Change the button pin back to input mode without pull-up resistor.
 * This is used for cleanup when the button is no longer needed or the device will be sleeping.
 */
void PushButton::end() {
  // Change the button pin back to input mode without pull-up resistor
  pinMode(_pinId, INPUT);
}

/*
 * Update the button state and check for a press event.
 * Returns true if the button state has changed, false otherwise.
 */
bool PushButton::update() {
  // update the button state and check for a press event
  bool currentState = digitalRead(_pinId);
  if (currentState == _lastState) {
    return false;
  }

  // Update the previous and last states
  _prevState = _lastState;
  _lastState = currentState;

  // call the onPress callback if the button was pressed
  if ((_onPress) && (isPressed())) {
    _onPress();
  }

  // call the onRelease callback if the button was released
  if ((_onRelease) && (isReleased())) {
    _onRelease();
  }

  return true;
}

/**
 * Check if the button was pressed (transition from HIGH to LOW).
 * Returns true if the button was pressed, false otherwise.
 */
bool PushButton::isPressed() {
  return (_prevState == HIGH && _lastState == LOW);
}

/**
 * Check if the button was released (transition from LOW to HIGH).
 * Returns true if the button was released, false otherwise.
 */
bool PushButton::isReleased() {
  return (_prevState == LOW && _lastState == HIGH);
}

ButtonManager::ButtonManager(size_t maxButtons) : _buttons(nullptr), _count(0), _maxButtons(maxButtons) {
  // Allocate memory for the buttons array based on the specified button count
  _buttons = (PushButton **)malloc(sizeof(PushButton) * maxButtons);
}

ButtonManager::~ButtonManager() {
  if (!_buttons) {
    return;
  }

  // Call the destructor for each button to clean up
  for (size_t i = 0; i < _count; ++i) {
    _buttons[i]->~PushButton();
  }

  // Free the allocated memory for the buttons array
  free(_buttons);
  _buttons = nullptr;
}

int8_t ButtonManager::add(PushButton *button) {
  if (_count >= _maxButtons) {
    return BID_NULL;  // Maximum button count reached, do not add more buttons
  }

  // Add the button to the array and increment the count
  _buttons[_count] = button;
  _count += 1;

  return (_count - 1);  // Return the index of the added button
}

void ButtonManager::begin() {
  // Initialize each button in the array
  for (size_t i = 0; i < _count; ++i) {
    _buttons[i]->begin();
  }
}

void ButtonManager::clear() {
  // Call the destructor for each button to clean up
  for (size_t i = 0; i < _count; ++i) {
    _buttons[i]->~PushButton();
  }

  _count = 0;  // Reset the button count to zero
}

void ButtonManager::end() {
  // Call the destructor for each button to clean up
  for (size_t i = 0; i < _count; ++i) {
    _buttons[i]->end();
  }
}

int8_t ButtonManager::process() {
  // Update the state of each button and check for press/release events
  // Note: If one button state changes, exit the loop to avoid processing further buttons
  // in the current update cycle.
  int8_t stateChangedId = ButtonManager::BID_NULL;  // Initialize to indicate no button state change
  for (size_t i = 0; i < _count; ++i) {
    // Update the button state and check for a press/release event
    if (_buttons[i]->update()) {
      stateChangedId = i;  // set the index of the button that changed state
      break;
    }
  }

  // Return the index of the button that changed state, or BID_NULL(-1) if no change detected
  return stateChangedId;
}