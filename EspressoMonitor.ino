#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Preferences.h>
#include "DisplayManager.h"
#include "SoundManager.h"
#include "SensorManager.h"
#include "CurrentManager.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "SystemState.h"
#include "SDManager.h"

// PIN DEFINITIONS
#define CURRENT_PIN 34  // Pin for SCT sensor
#define BUTTON_PIN  15  // Pin for the button (InputManager)
#define BUZZER_PIN 4    // Pin for the passive buzzer. used to transmit the sounds.
// (use ~120. at the moment- the values change for testing)
#define BOILER_BREW_TEMP 118    // default desired brewing start temp (of the boiler- the grouphead will always be much cooler)
#define GROUPHEAD_BREW_TEMP 91 // desired grouphead brew temp (will result in actual textbook 93-97 C brewing temp)
#define HEAT_SOAK_TIME 812000 // time needed for the E61 grouphead to heat up AFTER the boiler is at temp.

// SYSTEM
Preferences sysPrefs;

// OBJECTS
DisplayManager display;
SensorManager sensor;
CurrentManager pumpSensor(CURRENT_PIN);
TimerManager timer;
SoundManager sound (BUZZER_PIN);
SDManager sdCard;

SystemState currentState = WARMUP; // Start in WARMUP mode
unsigned long stateChangeTime = 0; // To track how long we've been in a state
// If waiting for too long, should check if temp hasn't dropped (i.e.- maybe a smart-home switch turned off by timer)
unsigned long peripheralsStatusCheckTime = 0;
unsigned long readyTime = 0;
unsigned long heatSoakStartTime = 0;
bool isBoilerReady = false;

// TESTING vars
float mockTempBoiler = 25.0;
float mockTempGH = 0.0;
unsigned long lastUpdateT = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    sysBoots(); // Number of boots

    display.init();
    display.showStartupScreen();
    display.loadScreen(WARMUP);
    sensor.init(); // 3 wire mode
    pumpSensor.init(); // IMPORTANT: Ensure pump is OFF when you turn the machine on! (good practice regardless)
    sdCard.init();

    sdCard.testReadWrite();

    Serial.println("-------------------------------");
    Serial.println("System Initialized.");
}

void loop() {
    // UPDATE INPUTS
    sensor.update();

    sound.update();

    unsigned long currentTime = millis();

    // check the peripherals' status every 10 seconds to update the display icons.
    if (currentTime - peripheralsStatusCheckTime > 10000) {
        display.setSDState(sdCard.isReady);
        // TODO: update when server is ready!
        display.setWifiState(false);
    }
    
    // Check pump status
    bool isPumpRunning = pumpSensor.isPumpOn();

    switch (currentState) {
        // CASE: WARMING UP
        // Waiting for the boiler to reach steaming temp (approx 120C+ when PT100 is attached to the boiler)
        case WARMUP:{
            // *TESTING*

            static float lastDrawnBoiler = -1.0;
            static float lastDrawnGH = -1.0;

            if (abs(mockTempBoiler - lastDrawnBoiler) > 0.1 || abs(mockTempGH - lastDrawnGH) > 0.1) {
                display.updateWarmupData(mockTempBoiler, mockTempGH);
                lastDrawnBoiler = mockTempBoiler;
                lastDrawnGH = mockTempGH;
            }
            if (millis() - lastUpdateT > 100) {
                lastUpdateT = millis();
                
                mockTempBoiler += 0.5; // Heat up by 0.5 degrees
                
                if (mockTempBoiler > 120.0 && mockTempGH < 40.0) {
                    mockTempGH = 50.0;
                }
                if (mockTempGH >= 50.0) {
                        mockTempGH += 0.5;
                }
                if (mockTempGH > 91.0) {
                    mockTempBoiler = 25.0;
                    mockTempGH = 0.0;
                }
            } // END OF TESTING
        /*
            // Display Status
            
                unsigned long boilerReadyTime = isBoilerReady ? currentTime - heatSoakStartTime : 0;
                display.updateWarmupData(sensor.getTemp(), sensor.getEstimatedGroupheadTemp(boilerReadyTime));

                if (sensor.getTemp() > BOILER_BREW_TEMP && !isBoilerReady) {
                    isBoilerReady = true;
                    heatSoakStartTime = currentTime;
                    Serial.println("Boiler at temp. Starting 13.5 min Grouphead Heat Soak.");
                }

                if (isBoilerReady && (sensor.getEstimatedGroupheadTemp(boilerReadyTime) >= GROUPHEAD_BREW_TEMP)) {
                    currentState = READY;
                    isBoilerReady = false;
                    display.loadScreen(READY);
                    sound.playDoom();
                    readyTime = currentTime;
                    Serial.println("State: READY");
                }
                // Allow brewing even if cold (Manual Override)
                if (isPumpRunning) {
                    isBoilerReady = false; // if brewing cold- override heat soak
                    timer.start();
                    currentState = BREWING;
                    display.loadScreen(BREWING);
                    Serial.println("State: BREWING");
                }
                */
                break;
            }
            

        // CASE: READY
        // Machine is hot. Waiting for a brew.
        case READY:
            display.updateReadyData(sensor.getTemp());
            // ADD LATER HERE: notify on phone / ip.
            // Transition -> BREWING
            if (isPumpRunning) {
                timer.reset();
                timer.start();
                currentState = BREWING;
                display.loadScreen(BREWING);
                Serial.println("State: BREWING");
            }
            // Wating for a minute before testing the temp again. When graph math is ready- use here.
            if (currentTime - readyTime > 60000 && sensor.getTemp() < BOILER_BREW_TEMP) {
                currentState = WARMUP;
                display.loadScreen(WARMUP);
                Serial.println("WARMUP: Temp dropped while waiting");
            }
            break;

        // CASE: BREWING
        // Pump is running; Timer is counting.
        case BREWING:
            display.updateBrewData(timer.getSeconds(), sensor.getTemp());

            // Transition -> DONE (Pump Stopped)
            if (!isPumpRunning) {
                timer.stop();
                stateChangeTime = currentTime; // Record when we finished
                currentState = DONE;
                display.loadScreen(DONE);
                Serial.println("State: DONE");
            }
            break;

        // CASE: DONE
        // Shot finished. Show the final time for a few seconds.
        case DONE:
            display.updateDoneData(timer.getSeconds(), sensor.getTemp());
            if (isPumpRunning) {
                timer.reset();
                timer.start();
                currentState = BREWING;
                display.loadScreen(BREWING);
                Serial.println("BREWING (again)");
                break;
            } else {
                if (currentTime - stateChangeTime > 10000){
                    if (sensor.getTemp() >= BOILER_BREW_TEMP) {
                        currentState = READY;
                        display.loadScreen(READY);
                        Serial.println("READY (again)- still warm enough");
                    } else {
                        currentState = WARMUP;
                        display.loadScreen(WARMUP);
                        Serial.println("WARMUP (again)");
                    }
                }
            }
            break;
    }
    display.update();

    delay(10); // FreeRTOS watchdog timer anti starvation
}

void sysBoots(){
    sysPrefs.begin("system", false);
    unsigned int boots = sysPrefs.getUInt("boots", 0) + 1;
    sysPrefs.putUInt("boots", boots);
    sysPrefs.end();
    Serial.print("Total Machine Boots: ");
    Serial.println(boots);
}