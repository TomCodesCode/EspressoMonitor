#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Adafruit_MAX31865.h>

// Define HSPI Pins
#define MAX_CS   25
// #define MAX_DI   25
// #define MAX_DO   13
// #define MAX_CLK  16

// The value of the reference resistor on the board in ohms.
// Should be 430 (+- 1%) from factory. Measured ~423.8.
#define RREF      424.0
// The 'nominal' resistance of the sensor at 0C (100 for PT100)
#define RNOMINAL  100.0

class SensorManager {
private:
    Adafruit_MAX31865* thermo; // Pointer to the library object
    SPIClass* maxSPI; // Shared bus with SD
    unsigned long lastReadTime;
    float currentTemp;
    bool isMeasuring; 
    unsigned long measureStartTime;

public:
    SensorManager(SPIClass* sharedSPI);
    void init();
    void update();
    float getTemp();
    float getEstimatedGroupheadTemp(unsigned long timeSinceBoilerReadyMs);
    
    // Helper to check if the sensor is actually connected
    bool checkFaults(); 
};

#endif