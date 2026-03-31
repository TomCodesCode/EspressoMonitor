#include "CurrentManager.h"
#include <Preferences.h>

float dynamicThreshold = 0.0;
Preferences preferences; 

CurrentManager::CurrentManager(int pinNumber) {
    pin = pinNumber;
    zeroPoint = 1950;
    lastCheck = 0;
    lastPumpState = false;
    
    // Initialize debounce variables
    pumpStartTime = 0;
    pumpStopTime = 0;
    pumpIsActuallyRunning = false;
}

void CurrentManager::init() {
    pinMode(pin, INPUT);

    Serial.println("Waiting for SCT circuit to stabilize...");
    
    int lastReading = analogRead(pin);
    int stableCount = 0;
    unsigned long startWait = millis();
    bool isStable = false;
    
    while (millis() - startWait < 3000) {
        delay(10); 
        int currentReading = analogRead(pin);
        
        if (abs(currentReading - lastReading) < 50) {
            stableCount++;
        } else {
            stableCount = 0; 
        }
        
        if (stableCount >= 15) {
            isStable = true;
            Serial.print("Circuit stable. Took (ms): ");
            Serial.println(millis() - startWait);
            break; 
        }
        lastReading = currentReading;
    }

    if (isStable) {
        calibrate(); 
        preferences.begin("espresso", false); 
        preferences.putInt("zero", zeroPoint);
        preferences.putFloat("thresh", dynamicThreshold);
        preferences.end();
        Serial.println("Calibration saved to memory.");
    } else {
        Serial.println("WARNING: Circuit unstable! Loading historical calibration...");
        preferences.begin("espresso", true); 
        zeroPoint = preferences.getInt("zero", 1950); 
        dynamicThreshold = preferences.getFloat("thresh", 16.0); 
        preferences.end();

        Serial.print("Loaded Zero Point: "); Serial.println(zeroPoint);
        Serial.print("Loaded Threshold: "); Serial.println(dynamicThreshold);
        // Serial.println("-------------------------------");
    }
}

void CurrentManager::calibrate() {
    Serial.println("--- CALIBRATING NOISE FLOOR ---");
    Serial.println("Keep Pump OFF for 1 second...");
    
    long total = 0;
    int samples = 0;
    unsigned long start = millis();
    while (millis() - start < 500) {
        total += analogRead(pin);
        samples++;
        delay(1);
    }
    if (samples > 0) zeroPoint = total / samples;
    
    float maxNoise = 0.0;
    start = millis();
    while (millis() - start < 1000) {
        float currentReading = readStrength();
        if (currentReading > maxNoise) {
            maxNoise = currentReading;
        }
        delay(10);
    }

    dynamicThreshold = maxNoise + 20.0;
    
    Serial.print("Zero Point: "); Serial.println(zeroPoint);
    Serial.print("Noise Floor: "); Serial.println(maxNoise);
    Serial.print("Auto-Threshold Set To: "); Serial.println(dynamicThreshold);
    Serial.println("-------------------------------");
}

float CurrentManager::readStrength() {
    long sumSquared = 0;
    int samples = 0;
    unsigned long startSample = millis();

    while (millis() - startSample < 30) {
        int raw = analogRead(pin);
        long shifted = raw - zeroPoint; 
        sumSquared += (shifted * shifted);
        samples++;
    }

    float meanSquare = (float)sumSquared / samples;
    float rms = sqrt(meanSquare);
    return rms * 0.50; 
}

bool CurrentManager::isPumpOn() {
    // 1. Get the raw bouncy state (checking hardware every 100ms)
    if (millis() - lastCheck >= 100) {
        lastCheck = millis();
        float strength = readStrength();

        if (lastPumpState) {
            if (strength < (dynamicThreshold - 1.0)) lastPumpState = false;
        } else {
            if (strength > dynamicThreshold) lastPumpState = true;
        }
    }

    // 2. Apply Time-Based Debounce to smooth out EMI spikes
    if (lastPumpState && !pumpIsActuallyRunning) {
        if (pumpStartTime == 0) pumpStartTime = millis(); 
        if (millis() - pumpStartTime > 250) {             // Must be ON for 250ms
            pumpIsActuallyRunning = true;
            pumpStopTime = 0;                             
        }
    } else if (!lastPumpState && pumpIsActuallyRunning) {
        if (pumpStopTime == 0) pumpStopTime = millis();   
        if (millis() - pumpStopTime > 500) {              // Must be OFF for 500ms
            pumpIsActuallyRunning = false;
            pumpStartTime = 0;                            
        }
    } else {
        // Reset timers if the state bounces back
        if (!lastPumpState) pumpStartTime = 0;
        if (lastPumpState) pumpStopTime = 0;
    }

    return pumpIsActuallyRunning;
}