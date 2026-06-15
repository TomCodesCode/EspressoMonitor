#ifndef CURRENT_MANAGER_H
#define CURRENT_MANAGER_H

#include <Arduino.h>

class CurrentManager {
private:
    int pin;
    int zeroPoint;           // discoverrd at startup
    float dynamicThreshold;
    unsigned long lastCheck; // prevent blocking the CPU
    bool lastPumpState;      // prevent flickering
    
    // Debounce Variables
    unsigned long pumpStartTime;
    unsigned long pumpStopTime;
    bool pumpIsActuallyRunning; // Clean, filtered state

    float lastStrength = 0.0;  // most recent reading from isPumpOn(), for live display/debug

public:
    CurrentManager(int pinNumber);
    void init();

    // Auto-discovers the "Silence" value
    void calibrate();
    float readStrength();

    // Returns TRUE if pump is ON
    bool isPumpOn();

    // Live debug accessors- last sampled current strength and the active trip threshold
    float getLastStrength() const { return lastStrength; }
    float getThreshold()    const { return dynamicThreshold; }
};

#endif