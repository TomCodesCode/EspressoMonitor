#include "CurrentManager.h"

// real threshold is calculated dynamically
float dynamicThreshold = 0.0;

CurrentManager::CurrentManager(int pinNumber) {
    pin = pinNumber;
    zeroPoint = 1950;
    lastCheck = 0;
    lastPumpState = false;
}

void CurrentManager::init() {
    pinMode(pin, INPUT);
    calibrate();
}

void CurrentManager::calibrate() {
    Serial.println("--- CALIBRATING NOISE FLOOR ---");
    Serial.println("Keep Pump OFF for 1 second...");
    
    // Find the DC zero-point
    long total = 0;
    int samples = 0;
    unsigned long start = millis();
    while (millis() - start < 500) {
        total += analogRead(pin);
        samples++;
        delay(1);
    }
    if (samples > 0) zeroPoint = total / samples;
    
    // Measure the background noise- Static
    float maxNoise = 0.0;
    start = millis();
    while (millis() - start < 1000) {
        float currentReading = readStrength();
        if (currentReading > maxNoise) {
            maxNoise = currentReading;
        }
        delay(10);
    }

    // If noise is 13.0, Threshold becomes 16.0
    dynamicThreshold = maxNoise + 3.0; 
    
    Serial.print("Zero Point: "); Serial.println(zeroPoint);
    Serial.print("Noise Floor: "); Serial.println(maxNoise);
    Serial.print("Auto-Threshold Set To: "); Serial.println(dynamicThreshold);
    Serial.println("-------------------------------");
}

// get the raw strength of the signal
float CurrentManager::readStrength() {
    long sumSquared = 0;
    int samples = 0;
    unsigned long startSample = millis();

    // Sample for 30ms
    while (millis() - startSample < 30) {
        int raw = analogRead(pin);
        long shifted = raw - zeroPoint; 
        sumSquared += (shifted * shifted);
        samples++;
    }

    float meanSquare = (float)sumSquared / samples;
    float rms = sqrt(meanSquare);
    return rms * 0.50; // Keep the High Sensitivity
}

bool CurrentManager::isPumpOn() {
    // Check every 100ms
    if (millis() - lastCheck < 100) return lastPumpState;
    lastCheck = millis();

    float strength = readStrength();

    // Debug: for the wire loop trick. to be tested.
    // Serial.print("Str: "); Serial.print(strength);
    // Serial.print(" / Thr: "); Serial.println(dynamicThreshold);

    // Sticky Trigger
    // If ON, stay ON until it drops well below threshold
    // If OFF, wait until it jumps well above
    if (lastPumpState) {
        // To stop, must drop 1.0 below threshold
        if (strength < (dynamicThreshold - 1.0)) lastPumpState = false;
    } else {
        // To start, must hit threshold
        if (strength > dynamicThreshold) lastPumpState = true;
    }
    
    return lastPumpState;
}