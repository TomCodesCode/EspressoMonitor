#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Adafruit_MAX31865.h>
#include <atomic>

// Define HSPI Pins
#define MAX_CS   25
// #define MAX_DI   25
// #define MAX_DO   13
// #define MAX_CLK  16

// The value of the reference resistor on the board in ohms.
// Should be 430 (+- 1%) from factory. Measured ~427.1.
#define RREF      427.1
// The 'nominal' resistance of the sensor at 0C (100 for PT100)
#define RNOMINAL  100.0

class SensorManager {
private:
    Adafruit_MAX31865* thermo; // Pointer to the library object
    SPIClass* maxSPI; // Shared bus with SD
    SemaphoreHandle_t spiMutex = NULL;
    std::atomic<int>*   _ghTargetRef = nullptr;
    std::atomic<float>* _tauRef      = nullptr;
    unsigned long lastReadTime;
    float currentTemp;
    bool isMeasuring;
    unsigned long measureStartTime;
    float _initialGroupheadTemp = 40.0f; // assumed GH temp when boiler first reaches target

public:
    SensorManager(SPIClass* sharedSPI);
    void init();
    void setSpiMutex(SemaphoreHandle_t m);
    void setGroupheadTargetRef(std::atomic<int>* ref);
    void setTauRef(std::atomic<float>* ref);
    float getInitialGroupheadTemp() const { return _initialGroupheadTemp; }
    void update();
    float getTemp();
    float getEstimatedGroupheadTemp(unsigned long timeSinceBoilerReadyMs);
    
    bool checkFaults();
    uint8_t detectFault(); // blocking one-shot check for use at startup only
};

#endif