#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <Arduino.h>

class InputManager {
  private:
  int pin;
  int state;        // current stable state (HIGH/ LOW)
  int lastReading;  // immediate reading from hardware
  unsigned long lastDebounceTime;
  unsigned long debounceDelay; // how long to wait for noise to settle
  bool wasPressedLastFrame = false;

  public:
  InputManager(int pinNumber);
  void init();
  void update();
  bool isPressed();
  bool isHeld();
};

#endif