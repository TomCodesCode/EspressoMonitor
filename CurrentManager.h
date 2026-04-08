#ifndef CURRENT_MANAGER_H
#define CURRENT_MANAGER_H

#include <Arduino.h>

class CurrentManager {
private:
    int pin;
    int zeroPoint;           // discoverrd at startup
    unsigned long lastCheck; // prevent blocking the CPU
    bool lastPumpState;      // prevent flickering
    
    // Debounce Variables
    unsigned long pumpStartTime;
    unsigned long pumpStopTime;
    bool pumpIsActuallyRunning; // Clean, filtered state

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