#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Preferences.h>
#include <atomic>
#include "DisplayManager.h"
#include "SoundManager.h"
#include "SensorManager.h"
#include "CurrentManager.h"
#include "TimeManager.h"
// #include "InputManager.h"
#include "SystemState.h"
#include "SDManager.h"

// PIN DEFINITIONS
#define CURRENT_PIN 34  // Pin for SCT sensor
// #define BUTTON_PIN  15  // Pin for the button (InputManager)
#define BUZZER_PIN 4    // Pin for the passive buzzer. used to transmit the sounds.
// (use ~120. at the moment- the values change for testing)
#define BOILER_BREW_TEMP 117    // default desired brewing start temp (of the boiler- the grouphead will always be much cooler)
#define GROUPHEAD_BREW_TEMP 90 // desired grouphead brew temp (will result in actual textbook 93-97 C brewing temp)
#define HEAT_SOAK_TIME 812000 // time needed for the E61 grouphead to heat up AFTER the boiler is at temp.

// SYSTEM
Preferences sysPrefs;

SPIClass sharedSPI(HSPI);
// OBJECTS
DisplayManager display;
SensorManager sensor(&sharedSPI);
CurrentManager pumpSensor(CURRENT_PIN);
TimerManager timer;
SoundManager sound (BUZZER_PIN);
SDManager sdCard (&sharedSPI);

SystemState currentState = WARMUP; // Start in WARMUP mode
unsigned long stateChangeTime = 0; // To track how long we've been in a state
// If waiting for too long, should check if temp hasn't dropped (i.e.- maybe a smart-home switch turned off by timer)
unsigned long peripheralsStatusCheckTime = 0;
unsigned long readyTime = 0;
unsigned long heatSoakStartTime = 0;
bool isBoilerReady = false;

std::atomic<float> sharedBoilerTemp = 20.0;
std::atomic<float> sharedGroupheadTemp = 50.0;
std::atomic<bool> sharedPumpRunning = false;
std::atomic<bool> requestCsvSave = false;
std::atomic<bool> csvSaveComplete = false;

// TESTING vars
float mockTempBoiler = 20.0;
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
    sharedSPI.begin(21, 22, 17, -1);
    sensor.init(); // 3 wire mode
    pumpSensor.init(); // IMPORTANT: Ensure pump is OFF when you turn the machine on! (good practice regardless)
    sdCard.init();

    sdCard.testReadWrite();
    
    xTaskCreatePinnedToCore(
        coreZeroWorkerTask, 
        "WorkerTask",       
        8192,               
        NULL,               
        1,                  // Priority 1
        NULL,               
        0                   // Pin to Core 0
    );

    Serial.println("-------------------------------");
    Serial.println("System Initialized.");
}

void coreZeroWorkerTask(void * parameter) {
    unsigned long prevMeasure = 0;
    for(;;) {
        sensor.update(); 
        sharedBoilerTemp = sensor.getTemp();
        
        unsigned long boilerReadyTime = isBoilerReady ? millis() - heatSoakStartTime : 0;
        sharedGroupheadTemp = sensor.getEstimatedGroupheadTemp(boilerReadyTime);
        
        // sharedPumpRunning = pumpSensor.isPumpOn();
        sharedPumpRunning = false;
        
        // SD Card Handshake (Triggered by Core 1)
        if (currentState == DONE && requestCsvSave) {
            sdCard.appendLog("/brew_log.csv", "TEST DATA"); // Replace with actual data
            requestCsvSave = false;
            csvSaveComplete = true; // Signal Core 1 to proceed
        }

        // TODO: build the Server and add here.
        // server.handleClient(); // Add this when your web server is built

        // yield to the OS
        delay(10);
    }
}

void loop() {
    sound.update();

    unsigned long currentTime = millis();

    // check the peripherals' status every 10 seconds to update the display icons.
    if (currentTime - peripheralsStatusCheckTime > 10000) {
        display.setSDState(sdCard.isReady);
        // TODO: update when server is ready!
        display.setWifiState(false);
        peripheralsStatusCheckTime = currentTime;
    }
    
    // Check pump status
    //bool isPumpRunning = pumpSensor.isPumpOn();

    switch (currentState) {
        // CASE: WARMING UP
        // Waiting for the boiler to reach steaming temp (approx 120C+ when PT100 is attached to the boiler)
        case WARMUP: {
            // *TESTING*
            
            static float lastDrawnBoiler = -1.0;
            static float lastDrawnGH = -1.0;
            static unsigned long lastUiUpdate = 0;

            if (abs(mockTempBoiler - lastDrawnBoiler) > 0.1 || abs(mockTempGH - lastDrawnGH) > 0.1) {
                display.updateWarmupData(mockTempBoiler, mockTempGH);
                lastDrawnBoiler = mockTempBoiler;
                lastDrawnGH = mockTempGH;
            }
            
            if (millis() - lastUpdateT > 500) {
                lastUpdateT = millis();
                
                mockTempBoiler += 2.5; // Heat up by 0.5 degrees
                
                if (mockTempBoiler > 120.0 && mockTempGH < 40.0) {
                    mockTempGH = 50.0;
                }
                if (mockTempGH >= 50.0) {
                        mockTempGH += 0.5;
                }
                if (mockTempGH > 91.0) {
                    mockTempGH = 88;
                    timer.start();
                    currentState = READY;
                    isBoilerReady = false;
                    display.loadScreen(READY);
                    // sound.playDoom();
                    sound.playHelldivers();
                    readyTime = currentTime;
                    Serial.println("State: READY");
                }
            } // END OF TESTING
            
            /*
            unsigned long boilerReadyTime = isBoilerReady ? currentTime - heatSoakStartTime : 0;
            display.updateWarmupData(sensor.getTemp(), sensor.getEstimatedGroupheadTemp(boilerReadyTime));

            if (sharedBoilerTemp > BOILER_BREW_TEMP && !isBoilerReady) {
                isBoilerReady = true;
                heatSoakStartTime = currentTime;
                Serial.println("Boiler at temp. Starting 13.5 min Grouphead Heat Soak.");
            }

            if (isBoilerReady && (sharedGroupheadTemp >= GROUPHEAD_BREW_TEMP)) {
                currentState = READY;
                isBoilerReady = false;
                display.loadScreen(READY);
                sound.playDoom();
                readyTime = currentTime;
                Serial.println("State: READY");
            }
            // Allow brewing even if cold (Manual Override)
            if (sharedPumpRunning) {
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
        case READY:{

            /*TESTING*/
            auto [minutes, seconds] = timer.getFormattedTime(TimerManager::MINUTES);
            display.updateReadyData(mockTempBoiler, mockTempGH, minutes, seconds);
            if (millis() - lastUpdateT > 1000){
                lastUpdateT = millis();
                mockTempGH += 0.5;
            }
            /*
            display.updateReadyData(sharedBoilerTemp, sharedGroupheadTemp);
            // ADD LATER HERE: notify on phone / ip.
            // Transition -> BREWING
            if (sharedPumpRunning) {
                timer.reset();
                timer.start();
                currentState = BREWING;
                display.loadScreen(BREWING);
                Serial.println("State: BREWING");
            }
            // Wating for a minute before testing the temp again. If machine got colder for some reason (machine no bueno?).
            if (currentTime - readyTime > 60000 && sharedBoilerTemp < BOILER_BREW_TEMP) {
                currentState = WARMUP;
                display.loadScreen(WARMUP);
                Serial.println("WARMUP: Temp dropped while waiting");
                
            }*/
            break;
        }

        // CASE: BREWING
        // Pump is running; Timer is counting.
        case BREWING:
            display.updateBrewData(timer.getSeconds(), sharedBoilerTemp);

            // Transition -> DONE (Pump Stopped)
            if (!sharedPumpRunning) {
                timer.stop();
                stateChangeTime = currentTime; // Record when we finished
                requestCsvSave = true; // Trigger core 0 to start saving.
                csvSaveComplete = false;
                currentState = DONE;
                display.loadScreen(DONE);
                Serial.println("State: DONE");
            }
            break;

        // CASE: DONE
        // Shot finished. Show the final time for a few seconds.
        case DONE:
            display.updateDoneData(timer.getSeconds(), sharedBoilerTemp);
            if (sharedPumpRunning) {
                timer.reset();
                timer.start();
                currentState = BREWING;
                display.loadScreen(BREWING);
                Serial.println("BREWING (again)");
                break;
            } else {
                if (currentTime - stateChangeTime > 10000){
                    if (sharedBoilerTemp >= BOILER_BREW_TEMP) {
                        currentState = READY;
                        display.loadScreen(READY);
                        Serial.println("READY- still warm enough");
                    } else {
                        currentState = WARMUP;
                        display.loadScreen(WARMUP);
                        Serial.println("WARMUP");
                    }
                }
            }
            break;
    }
    display.update();

    delay(33); // FreeRTOS watchdog timer anti starvation
}

void sysBoots(){
    sysPrefs.begin("system", false);
    unsigned int boots = sysPrefs.getUInt("boots", 0) + 1;
    sysPrefs.putUInt("boots", boots);
    sysPrefs.end();
    Serial.print("Total Machine Boots: ");
    Serial.println(boots);
}