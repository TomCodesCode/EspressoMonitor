#include "SensorManager.h"
#include <SPI.h>

SensorManager::SensorManager() {
    thermo = new Adafruit_MAX31865(MAX_CS, MAX_DI, MAX_DO, MAX_CLK);
    lastReadTime = 0;
    currentTemp = 0.0;
}

void SensorManager::init() {
    pinMode(MAX_CS, OUTPUT);
    digitalWrite(MAX_CS, HIGH); // Deselect chip

    // ignores the startup noise
    Serial.println("Initializing Sensor in Robust Mode...");
    thermo->begin(MAX31865_2WIRE);
    thermo->clearFault();
}

void SensorManager::update() {
    // Read every 1 second
    if (millis() - lastReadTime > 1000) {
        lastReadTime = millis();

        // Turn on power to the probe to measure
        thermo->enableBias(true);
        delay(100); 
        
        float temp = thermo->temperature(RNOMINAL, RREF);
        uint8_t fault = thermo->readFault();
        
        if (fault) {
            Serial.print("Fault 0x"); Serial.println(fault, HEX);
            thermo->clearFault();
            thermo->enableBias(false);
        } else {
            // CALIBRATION
            // Subtract 0.3C to account for wire resistance ignored by 2-Wire mode
            float calibratedTemp = temp - 0.3;
            
            Serial.print("Stable Temp: "); Serial.println(calibratedTemp);
            currentTemp = calibratedTemp;
            
            thermo->enableBias(false);
        }
    }
}

float SensorManager::getTemp() { return currentTemp; }
bool SensorManager::checkFaults() { return (thermo->readFault() != 0); }
