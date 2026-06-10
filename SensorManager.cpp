#include "esp32-hal-spi.h"
#include "esp32-hal.h"
#include <sys/_types.h>
#include "SensorManager.h"
#include <SPI.h>

// VBM Domobar Junior is an E61 machine, so after the boiler reaches its target temp, the brass needs to heat up the grouphead (takes 11 - 15 minutes usually)
// targetGroupheadTemp is computed dynamically from _ghTargetRef (+14 for asymptote offset); default 90°C
const float initialGroupheadTemp = 50.0; // assumed grouphead temp when boiler is ready

// tau (time constant) is set dynamically via _tauRef. default 592 s ≈ 13.5 min heat soak

SensorManager::SensorManager(SPIClass* sharedSPI) {
    maxSPI = sharedSPI;
    thermo = new Adafruit_MAX31865(MAX_CS, maxSPI);
    lastReadTime = 0;
    currentTemp = 0.0;
    isMeasuring = false;
    measureStartTime = 0;
}

void SensorManager::init() {
    pinMode(MAX_CS, OUTPUT);
    digitalWrite(MAX_CS, HIGH); // Deselect chip

    // maxSPI->begin(21, 22, 17, MAX_CS);

    // ignores the startup noise
    Serial.println("Initializing Sensor in Robust Mode...");
    thermo->begin(MAX31865_3WIRE);
    thermo->clearFault();
}

void SensorManager::setSpiMutex(SemaphoreHandle_t m) {
    spiMutex = m;
}

void SensorManager::setGroupheadTargetRef(std::atomic<int>* ref) {
    _ghTargetRef = ref;
}

void SensorManager::setTauRef(std::atomic<float>* ref) {
    _tauRef = ref;
}

void SensorManager::update() {
    unsigned long currentMillis = millis();

    // Measure every 1 second.
    // TODO: do we want 500ms instead? maybe later
    if (!isMeasuring) {
        if (currentMillis - lastReadTime > 1000) {
            lastReadTime = currentMillis;
            // Fix 6: hold the bus only for the actual SPI register write.
            if (spiMutex) xSemaphoreTake(spiMutex, portMAX_DELAY);
            thermo->enableBias(true);
            if (spiMutex) xSemaphoreGive(spiMutex);
            measureStartTime = currentMillis;   // Start the stopwatch
            isMeasuring = true;
        }

    } else {
        // 100ms of hardware stabilization
        if (currentMillis - measureStartTime >= 100) {

            // Fix 6: one bus transaction for the read + fault handling.
            if (spiMutex) xSemaphoreTake(spiMutex, portMAX_DELAY);
            float temp = thermo->temperature(RNOMINAL, RREF);
            uint8_t fault = thermo->readFault();

            if (fault) {
                Serial.print("Fault 0x"); Serial.println(fault, HEX);
                thermo->clearFault();
                thermo->enableBias(false);
            } else {
                // CALIBRATION
                // 0.385ohms per 1C. 100ohms at 0C. Redundant 3WIRE mode .
                // float calibratedTemp = temp - 0.25; // Account for cable length + plugs resistance.
                // float calibratedTemp = temp;

                Serial.print("Stable Temp: "); Serial.println(temp);
                currentTemp = temp;

                thermo->enableBias(false);
                isMeasuring = false;
            }
            if (spiMutex) xSemaphoreGive(spiMutex);
        }
    }
}

#include <math.h>

float SensorManager::getEstimatedGroupheadTemp(unsigned long timeSinceBoilerReadyMs) {
    float target  = (_ghTargetRef ? (float)_ghTargetRef->load() : 90.0f) + 14.0f;
    float tau     = _tauRef ? _tauRef->load() : 592.0f;
    float elapsed = timeSinceBoilerReadyMs / 1000.0f;
    return target - (target - initialGroupheadTemp) * exp(-elapsed / tau);
}

float SensorManager::getTemp() { return currentTemp; }
bool SensorManager::checkFaults() { return (thermo->readFault() != 0); }

uint8_t SensorManager::detectFault() {
    // Synchronous measurement + fault read. Only safe to call before the worker
    // task starts (no contention on the bus, so the 110ms delay is fine).
    if (spiMutex) xSemaphoreTake(spiMutex, portMAX_DELAY);
    thermo->enableBias(true);
    delay(110);
    uint8_t fault = thermo->readFault();
    thermo->clearFault();
    thermo->enableBias(false);
    if (spiMutex) xSemaphoreGive(spiMutex);
    return fault;
}
