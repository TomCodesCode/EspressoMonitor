#include "InputManager.h"

InputManager::InputManager(int pinNumber) : pin(pinNumber){
  state = HIGH; // not pressed
  lastReading = HIGH;
  lastDebounceTime = 0;
  debounceDelay = 50;
}

void InputManager::init() {
  pinMode(pin, INPUT_PULLUP);
}

void InputManager::update() {
  int reading = digitalRead(pin);

  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if (millis() - lastDebounceTime > debounceDelay) {
    if (reading != state){
      state = reading; // the button state was changed
    }
  }

  lastReading = reading;
}

bool InputManager::isPressed() {
  if (state == LOW && !wasPressedLastFrame) {
    wasPressedLastFrame = true;
    return true;
  }

  if (state == HIGH) {
    wasPressedLastFrame = false;
  }

  return false;
}

bool InputManager::isHeld() {
  wasPressedLastFrame = false;
  return (state == LOW);
}