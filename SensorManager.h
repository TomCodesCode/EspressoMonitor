#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Adafruit_MAX31865.h>

// Define SPI Pins
#define MAX_CS   5
#define MAX_DI   23
#define MAX_DO   19
#define MAX_CLK  18

// The value of the reference resistor on the board
#define RREF      430.0
// The 'nominal' resistance of the sensor at 0C (100 for PT100)
#define RNOMINAL  100.0

class SensorManager {
private:
    Adafruit_MAX31865* thermo; // Pointer to the library object
    unsigned long lastReadTime;
    float currentTemp;

public:
    SensorManager();
    void init();
    void update();
    float getTemp();
    
    // Helper to check if the sensor is actually connected
    bool checkFaults(); 
};

#endif