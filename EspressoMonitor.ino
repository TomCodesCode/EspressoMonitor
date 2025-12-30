#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "DisplayManager.h"
#include "SoundManager.h"
#include "SensorManager.h"
#include "CurrentManager.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "SystemState.h"

// PIN DEFINITIONS
#define CURRENT_PIN 34  // Pin for SCT sensor
#define BUTTON_PIN  15  // Pin for the button (InputManager)
#define BUZZER_PIN 4    // Pin for the passive buzzer. used to transmit the sounds.
// (use ~120. at the moment- the values change for testing)
#define BREW_TEMP 25    // default desired brewing start temp (of the boiler- the grouphead will always be much cooler)

// --- OBJECTS ---
DisplayManager display;
SensorManager sensor;
CurrentManager pumpSensor(CURRENT_PIN);
TimerManager timer;
SoundManager sound (BUZZER_PIN);

SystemState currentState = WARMUP; // Start in WARMUP mode
unsigned long stateChangeTime = 0; // To track how long we've been in a state
// If waiting for too long, should check if temp hasn't dropped (i.e.- maybe a smart-home switch turned off by timer)
unsigned long readyTime = 0;

void setup() {
    Serial.begin(115200);

    display.init();
    display.showStartupScreen();
    sensor.init(); // 2 wire mode
    pumpSensor.init(); // IMPORTANT: Ensure pump is OFF when you turn the machine on! (good practice regardless)

    Serial.println("System Initialized.");
}

void loop() {
    // UPDATE INPUTS
    sensor.update();
    Serial.println(pumpSensor.readStrength());
    
    // Check pump status
    bool isPumpRunning = pumpSensor.isPumpOn();

    switch (currentState) {
        
        // CASE: WARMING UP
        // Waiting for the boiler to reach steaming temp (approx 120C+ when PT100 is attached to the boiler)
        case WARMUP:
            // Display Status
            display.showStatus("Warming Up", sensor.getTemp(), " C");

            if (sensor.getTemp() > BREW_TEMP) {
                currentState = READY;
                sound.playDoom();
                readyTime = millis();
                Serial.println("State: READY");
            }
            // Allow brewing even if cold (Manual Override)
            if (isPumpRunning) {
                timer.start();
                currentState = BREWING;
            }
            break;

        // CASE: READY
        // Machine is hot. Waiting for a brew.
        case READY:
            display.showStatus("Ready", sensor.getTemp(), " C");
            // ADD LATER HERE: notify on phone / ip.
            // Transition -> BREWING
            if (isPumpRunning) {
                timer.reset();
                timer.start();
                currentState = BREWING;
                Serial.println("State: BREWING");
            }
            // Wating for a minute before testing the temp again. When graph math is ready- use here.
            if (millis() - readyTime > 60000 && sensor.getTemp() < BREW_TEMP) {
                currentState = WARMUP;
                Serial.println("WARMUP: Temp dropped while waiting");
            }
            break;

        // CASE: BREWING
        // Pump is running; Timer is counting.
        case BREWING:
            display.showStatus("Brewing", timer.getFormattedTime());

            // Transition -> DONE (Pump Stopped)
            if (!isPumpRunning) {
                timer.stop();
                stateChangeTime = millis(); // Record when we finished
                currentState = DONE;
                Serial.println("State: DONE");
            }
            break;

        // CASE: DONE
        // Shot finished. Show the final time for a few seconds.
        case DONE:
            display.showStatus("Done", timer.getFormattedTime());
            if (isPumpRunning) {
                currentState = BREWING;
                Serial.println("BREWING again");
                break;
            }
            if (millis() - stateChangeTime > 10000){
                if (sensor.getTemp() >= BREW_TEMP) {
                    currentState = READY;
                    Serial.println("READY (again)- still warm enough");
                } else {
                    currentState = WARMUP;
                    Serial.println("WARMUP (again)");
                }
            }
            break;
    }
    delay(50); // Small delay to prevent screen flickering/CPU hogging
}