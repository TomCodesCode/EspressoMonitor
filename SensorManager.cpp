#include "esp32-hal-spi.h"
#include "esp32-hal.h"
#include <sys/_types.h>
#include "SensorManager.h"
#include <SPI.h>

SensorManager::SensorManager() {
    thermo = new Adafruit_MAX31865(MAX_CS);
    lastReadTime = 0;
    currentTemp = 0.0;
    isMeasuring = false;
    measureStartTime = 0;
}

void SensorManager::init() {
    pinMode(MAX_CS, OUTPUT);
    digitalWrite(MAX_CS, HIGH); // Deselect chip

    // ignores the startup noise
    Serial.println("Initializing Sensor in Robust Mode...");
    thermo->begin(MAX31865_3WIRE);
    thermo->clearFault();
}

void SensorManager::update() {
    unsigned long currentMillis = millis();

    // Measure every 1 second.
    // TODO: do we want 500ms instead? maybe later
    if (!isMeasuring) {
        if (currentMillis - lastReadTime > 1000) {
            lastReadTime = currentMillis;
            thermo->enableBias(true);
            measureStartTime = currentMillis;   // Start the stopwatch
            isMeasuring = true;
        }
    
    } else {
        // 100ms of hardware stabilization
        if (currentMillis - measureStartTime >= 100) {

        float temp = thermo->temperature(RNOMINAL, RREF);
        uint8_t fault = thermo->readFault();
        
        if (fault) {
            Serial.print("Fault 0x"); Serial.println(fault, HEX);
            thermo->clearFault();
            thermo->enableBias(false);
        } else {
            // CALIBRATION
            // 0.385ohms per 1C. 100ohms at 0C. Redundant 3WIRE mode .
            float calibratedTemp = temp - 0.25; // Account for cable length + plugs resistance.
            
            Serial.print("Stable Temp: "); Serial.println(calibratedTemp);
            currentTemp = calibratedTemp;
            
            thermo->enableBias(false);
            isMeasuring = false;
        }
    }
    }
}

float SensorManager::getTemp() { return currentTemp; }
bool SensorManager::checkFaults() { return (thermo->readFault() != 0); }
