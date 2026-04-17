#include "esp32-hal-spi.h"
#include "esp32-hal.h"
#include <sys/_types.h>
#include "SensorManager.h"
#include <SPI.h>

// VBM Domobar Junior is an E61 machine, so after the boiler reaches its target temp, the brass needs to heat up the grouphead (takes 11 - 15 minutes usually)
const float targetGroupheadTemp = 91.0 + 14.0; // 91c target + 14 to account for calculation asymptote
const float initialGroupheadTemp = 50.0; // assumed grouphead temp when boiler is ready

// The "sluggishness" factor of the brass. The calculation used Newton's Law of Heating to estimate grouphead temp.
// calculation: 13.5 minutes to heat the grouphead after boiler is at temp. using Newton's Law of Heating, we never reach the max asymptote (91c target),
// so we calculate for tau: 91 = 105 - (105 - 50)*e^(-812/tau) -> tau = ~592.
const float tau = 592.0;

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

#include <math.h>

float SensorManager::getEstimatedGroupheadTemp(unsigned long timeSinceBoilerReadyMs) {
    // Convert elapsed time to seconds
    float timeSeconds = timeSinceBoilerReadyMs / 1000.0;

    // Newton's law of heating / cooling
    float estimatedTemp = targetGroupheadTemp - (targetGroupheadTemp - initialGroupheadTemp) * exp(-timeSeconds / tau);

    return estimatedTemp;
}

float SensorManager::getTemp() { return currentTemp; }
bool SensorManager::checkFaults() { return (thermo->readFault() != 0); }
