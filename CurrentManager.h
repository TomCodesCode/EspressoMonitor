#ifndef CURRENT_MANAGER_H
#define CURRENT_MANAGER_H

#include <Arduino.h>

class CurrentManager {
private:
    int pin;
    int zeroPoint;           // discoverrd at startup
    unsigned long lastCheck; // prevent blocking the CPU
    bool lastPumpState;      // prevent flickering

public:
    CurrentManager(int pinNumber);
    void init();
    
    // Auto-discovers the "Silence" value
    void calibrate();
    float readStrength();

    // Returns TRUE if pump is ON
    bool isPumpOn();
};

#endif