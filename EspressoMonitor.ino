/*
Required Notice: Copyright 2026 TomCodesCode ([https://github.com/tomcodescode/espressomonitor](https://github.com/tomcodescode/espressomonitor))
This project is made by www.github.com/TomCodesCode.
You are free to treat it as an open source and experiment with it if you want.
Please leave this credit when using and cloning the repo.
Happy brewing!
*/

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
// legacy: if a button is added, change the InputManager files and uncomment.
// #include "InputManager.h"
#include "SystemState.h"
#include "version.h"
#include "SDManager.h"
#include "BrewSession.h"
#include "ServerManager.h"
#include "secrets.h"
#include <time.h>
#include "esp_task_wdt.h"

// PIN DEFINITIONS
#define CURRENT_PIN 34  // Pin for SCT sensor
// #define BUTTON_PIN  15  // Pin for the button (InputManager)
#define BUZZER_PIN 4    // Pin for the passive buzzer. used to transmit the sounds.
// (use ~117. at the moment- the values change for testing)
// Target temps are runtime-adjustable via Settings dropdowns, persisted in NVS.
// Boiler range 115-125, GH range 85-93 (matching dropdown options in SquareLine UI).

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
ServerManager webServer;

SystemState currentState  = WARMUP;
SystemState previousState = WARMUP;
bool mockMode = false; // true when sensor fault 0x68 detected at boot (no machine connected)
unsigned long stateChangeTime = 0; // To track how long we've been in a state
// If waiting for too long, should check if temp hasn't dropped (i.e.- maybe a smart-home switch turned off by timer)
unsigned long peripheralsStatusCheckTime = 0;
unsigned long readyTime = 0;
// Written by Core 1 (loop), read by Core 0 (worker) for the GH estimate - atomic
// to make the cross-core access explicit. (Aligned 32-bit access is atomic on
// Xtensa anyway, but this documents intent and adds proper memory ordering.)
std::atomic<unsigned long> heatSoakStartTime{0};
std::atomic<bool> isBoilerReady{false};
bool calibWindow  = false; // true from boiler-ready through end of READY; loop()-only, single-core
bool savePending = false;  // true while waiting for Core 0 to finish the SD save

std::atomic<float> sharedBoilerTemp = 20.0;
std::atomic<float> sharedGroupheadTemp = 50.0;
std::atomic<bool> sharedPumpRunning = false;
std::atomic<bool> requestCsvSave = false;
std::atomic<bool> csvSaveComplete = false;
std::atomic<bool> requestLogClear = false;
std::atomic<uint32_t> sharedSDFreeMB{0};
std::atomic<int>      sharedBrewCount{0};
std::atomic<int>      sharedBrewRating{-1};
std::atomic<int>      sharedMusicSelect{0};  // 0=Helldivers, 1=Doom, 2=Mute
std::atomic<int>      sharedBoilerTarget{117};
std::atomic<int>      sharedGHTarget{90};
std::atomic<int>      sharedCalibTemp{50};    // GH temp observed by user at calibration time
std::atomic<bool>     calibAvailable{false};  // true during first 60 s of READY (shown in settings)
std::atomic<float>    sharedTau{592.0f};      // Newton's Law time constant (seconds)
std::atomic<bool>     requestNotify{false};   // set by Core 1 on WARMUP→READY; Core 0 fires ntfy.sh

// shared brew history (written by Core 1, read by Core 0)
BrewSession brewSession;
portMUX_TYPE brewMux = portMUX_INITIALIZER_UNLOCKED;

// consistent live-temperature snapshot for the web server.
// Two separate atomics can't be read as a coherent pair, so publish them
// together under one lock. See TempSnapshot in BrewSession.h.
portMUX_TYPE tempMux = portMUX_INITIALIZER_UNLOCKED;
TempSnapshot latestTemps = { 20.0, 50.0 };

// serialize access to the shared HSPI bus (MAX31865 + SD)
SemaphoreHandle_t spiMutex = NULL;

// TESTING vars
float mockTempBoiler = 20.0;
float mockTempGH = 0.0;
unsigned long lastUpdateT = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    sysBoots(); // Number of boots

    // --------------confirm 32-bit atomics are lock-free on this chip (informational)------------
    // is_always_lock_free is a compile-time constant, so it avoids the runtime
    // __atomic_is_lock_free symbol that the Xtensa runtime doesn't provide.
    // Serial.print("Atomic float lock-free: ");
    // Serial.println(std::atomic<float>::is_always_lock_free ? "yes" : "no (spinlock)");

    // create the shared-bus mutex BEFORE the worker task starts.
    spiMutex = xSemaphoreCreateMutex();
    if (spiMutex == NULL) {
        Serial.println("FATAL: Could not create SPI mutex!");
    }
    sensor.setSpiMutex(spiMutex);
    sdCard.setSpiMutex(spiMutex);

    // give the display a handle to the shared brew history.
    display.setBrewSession(&brewSession, &brewMux);
    display.setSettingsPointers(&currentState, &previousState, &requestLogClear);
    display.setMusicSelectPointer(&sharedMusicSelect);
    display.setTempTargetPointers(&sharedBoilerTarget, &sharedGHTarget);
    display.setCalibPointers(&sharedCalibTemp, &calibAvailable);
    display.setFirmwareVersion(FW_VERSION);
    sensor.setGroupheadTargetRef(&sharedGHTarget);
    sensor.setTauRef(&sharedTau);

    display.init();
    {
        sysPrefs.begin("system", true);
        int   savedMusic   = sysPrefs.getInt("music",        0);
        int   savedBoiler  = sysPrefs.getInt("boilerTarget", 117);
        int   savedGH      = sysPrefs.getInt("ghTarget",     90);
        float savedTau     = sysPrefs.getFloat("tau",        592.0f);
        sysPrefs.end();
        if (savedTau < 100.0f || savedTau > 2400.0f) {
            Serial.printf("NVS tau out of range (%.1f s) — reset to default 592 s\n", savedTau);
            savedTau = 592.0f;
        }
        sharedMusicSelect.store(savedMusic);
        sharedBoilerTarget.store(savedBoiler);
        sharedGHTarget.store(savedGH);
        sharedTau.store(savedTau);
        display.setMusicDropdown(savedMusic);
        display.setBoilerTargetDropdown(savedBoiler);
        display.setGHTargetDropdown(savedGH);
    }
    display.showStartupScreen();
    display.loadScreen(WARMUP);
    sharedSPI.begin(21, 22, 17, -1);
    sensor.init(); // 3 wire mode
    pumpSensor.init(); // IMPORTANT: Ensure pump is OFF when you turn the machine on! (good practice regardless)
    sdCard.init();

    // purely for SD testing. no need in actual runs. un-comment if you suspect anything
    // sdCard.testReadWrite();

    webServer.setDataSources(&latestTemps, &tempMux, &currentState, &sdCard);
    webServer.begin(WIFI_SSID, WIFI_PASSWORD);

    // Extend WDT to 12 s- individual SD card ops (flash erase, wear-leveling)
    // can legitimately take several seconds; 12 s still catches true hangs.
    {
        esp_task_wdt_config_t wdtCfg = { .timeout_ms = 12000, .idle_core_mask = 0, .trigger_panic = true };
        esp_task_wdt_reconfigure(&wdtCfg);
    }

    xTaskCreatePinnedToCore(
        coreZeroWorkerTask,
        "WorkerTask",
        16384,              // Increased stack for stability
        NULL,
        1,                  // Priority 1
        NULL,
        0                   // Pin to Core 0
    );

    Serial.println("-------------------------------");
    Serial.println("System Initialized.");

    // Check sensor fault after full init. Bits 5+3 (0x28) = REFIN/RTDIN out of range
    // = nothing connected. Appears as 0x28 early or 0x68 once the chip latches bit 6.
    // detectFault() uses spiMutex so it's safe to call with the worker task running.
    {
        uint8_t fault = sensor.detectFault();
        if ((fault & 0x28) == 0x28) {
            mockMode = true;
            Serial.print("Sensor fault 0x"); Serial.print(fault, HEX);
            Serial.println(" — mock mode enabled (no machine connected).");
        } else if (fault != 0) {
            Serial.print("Sensor fault 0x"); Serial.print(fault, HEX);
            Serial.println(" — real mode, check wiring.");
        } else {
            Serial.println("Sensor OK — real mode.");
        }
    }
}

void coreZeroWorkerTask(void * parameter) {
    esp_task_wdt_add(NULL);  // register this task with the watchdog (default 5 s timeout)
    unsigned long lastHeartbeat = 0;
    for(;;) {
        esp_task_wdt_reset();

        if (!mockMode) {
            sensor.update();

            float boilerNow = sensor.getTemp();
            sharedBoilerTemp = boilerNow;

            // GH is modeled, not measured. Run the model only in WARMUP: this
            // reproduces the original curve (40 C while the boiler arc fills,
            // then climbing during heat soak). In READY/DONE the estimate is
            // left frozen at its last WARMUP value (~GH target) rather than
            // recomputing with elapsed=0, which snapped it back to 40 C.
            if (currentState == WARMUP) {
                unsigned long boilerReadyTime = isBoilerReady ? millis() - heatSoakStartTime : 0;
                sharedGroupheadTemp = sensor.getEstimatedGroupheadTemp(boilerReadyTime);
            } else {
                // READY/DONE: no live GH model. Show the current GH target so it
                // reflects settings changes instead of a stale frozen estimate.
                sharedGroupheadTemp = (float)sharedGHTarget.load();
            }

            portENTER_CRITICAL(&tempMux);
            latestTemps.boiler    = boilerNow;
            latestTemps.grouphead = sharedGroupheadTemp.load();
            portEXIT_CRITICAL(&tempMux);
        }

        // NOTE: pump detection runs in loop() on Core 1, NOT here. The WiFi/lwIP
        // stack lives on Core 0 and preempts readStrength()'s delay(1) sampling
        // loop, jittering the samples and inflating the RMS into phantom current.

        // SD Card Handshake (Triggered by Core 1)
        if (requestCsvSave) {
            // read the finished brew from the shared session and log it.
            portENTER_CRITICAL(&brewMux);
            float dur = brewSession.durationSeconds;
            int pts = brewSession.pointCount;
            portEXIT_CRITICAL(&brewMux);
            // only save brew logs that are of valid duration.
            if (10 <= dur && dur <= 120) {
                unsigned long brewId = millis(); // unique ID for both log line and temp file
                time_t unixTime = getUnixTime(); // 0 if NTP not yet synced; JS uses millis fallback
                int rating = sharedBrewRating.load();
                char logLine[96];
                snprintf(logLine, sizeof(logLine), "%lu,%.1f,%d,%ld,%d", brewId, dur, pts, (long)unixTime, rating);
                sdCard.appendLog("/brew_log.csv", logLine);
                // temperatures[] safe without lock: isComplete=true means Core 1 stopped writing
                sdCard.saveBrewTemps(brewId, brewSession.temperatures, pts);
            }

            requestCsvSave = false;
            csvSaveComplete = true; // Signal Core 1 to proceed
        }

        // Populate SD stats once each time the settings screen is opened.
        static SystemState lastWorkerState = WARMUP;
        if (currentState == SETTINGS && lastWorkerState != SETTINGS) {
            sharedSDFreeMB.store(sdCard.getFreeSpaceMB());
            sharedBrewCount.store(sdCard.getBrewCount());
        }
        lastWorkerState = currentState;

        // Clear logs on request, then refresh the stats for the settings screen.
        if (requestLogClear) {
            sdCard.clearLogs();
            sharedBrewCount.store(0);
            sharedSDFreeMB.store(sdCard.getFreeSpaceMB());
            requestLogClear = false;
        }

        esp_task_wdt_reset();
        webServer.handleClient();
        esp_task_wdt_reset();

        if (requestNotify.load()) {
            requestNotify.store(false);
            webServer.notifyReady();
        }

        if (millis() - lastHeartbeat > 30000) {
            lastHeartbeat = millis();
            Serial.printf("Core0 alive. Free heap: %u B  Min free: %u B\n",
                          ESP.getFreeHeap(), ESP.getMinFreeHeap());
        }

        // yield to the OS
        delay(10);
    }
}

void playReadySound() {
    switch (sharedMusicSelect.load()) {
        case 1:  sound.playDoom();        break;
        case 2:  /* mute */               break;
        default: sound.playHelldivers();  break;
    }
}

void loop() {
    unsigned long frameStart = millis();
    sound.update();

    unsigned long currentTime = millis();
    static SystemState lastLoopState = WARMUP;
    static float       pendingTau    = -1.0f;
    static bool        settingsFreshEntry = false; // true on the first SETTINGS frame after entering
    static bool        nvsPending         = false; // a settings change is waiting to be written to NVS

    // Flush settings to NVS once when leaving SETTINGS. The in-screen save is
    // debounced by ~1 s; tapping Exit sooner would otherwise drop the write
    // (the SETTINGS case stops running the instant currentState changes), so
    // the live change would be lost on the next boot. Flush here to be safe.
    if (lastLoopState == SETTINGS && currentState != SETTINGS) {
        if (pendingTau > 0.0f) {
            sysPrefs.begin("system", false);
            sysPrefs.putFloat("tau", pendingTau);
            sysPrefs.end();
            Serial.printf("tau saved to NVS on settings exit: %.1f s\n", pendingTau);
            pendingTau = -1.0f;
        }
        if (nvsPending) {
            nvsPending = false;
            sysPrefs.begin("system", false);
            sysPrefs.putInt("music",        sharedMusicSelect.load());
            sysPrefs.putInt("boilerTarget", sharedBoilerTarget.load());
            sysPrefs.putInt("ghTarget",     sharedGHTarget.load());
            sysPrefs.end();
            Serial.println("Settings saved to NVS on settings exit");
        }
    }
    // Detect entry into SETTINGS so the case below can snapshot the current
    // values and avoid treating untouched controls as user changes.
    if (currentState == SETTINGS && lastLoopState != SETTINGS) settingsFreshEntry = true;
    lastLoopState = currentState;

    // check the peripherals' status every 10 seconds to update the display icons.
    if (currentTime - peripheralsStatusCheckTime > 10000) {
        display.setSDState(sdCard.isReady);
        display.setWifiState(webServer.isConnected());
        peripheralsStatusCheckTime = currentTime;
    }

    // Pump detection — runs here on Core 1 (loopTask), away from the WiFi/lwIP
    // stack on Core 0. isPumpOn() self-rate-limits to 100 ms internally, so
    // calling it every frame is cheap. This is where it lived (and worked)
    // before the 2-core refactor disabled it.
    if (!mockMode) sharedPumpRunning = pumpSensor.isPumpOn();

    switch (currentState) {
        // CASE: WARMING UP
        case WARMUP: {
            if (mockMode) {
                static float lastDrawnBoiler = -1.0;
                static float lastDrawnGH     = -1.0;
                if (abs(mockTempBoiler - lastDrawnBoiler) > 0.1 || abs(mockTempGH - lastDrawnGH) > 0.1) {
                    display.updateWarmupData(mockTempBoiler, mockTempGH, mockTempGH > 0.0f);
                    lastDrawnBoiler = mockTempBoiler;
                    lastDrawnGH     = mockTempGH;
                }
                if (currentTime - lastUpdateT > 500) {
                    lastUpdateT = currentTime;
                    mockTempBoiler += 2.5;
                    if (mockTempBoiler > sharedBoilerTarget.load() && mockTempGH < 40.0) mockTempGH = 50.0;
                    if (mockTempGH >= 50.0) mockTempGH += 0.5;
                    if (mockTempGH > sharedGHTarget.load()) {
                        mockTempGH = 88.0;
                        timer.start();
                        currentState = READY;
                        requestNotify = true;
                        display.loadScreen(READY);
                        playReadySound();
                        readyTime = currentTime;
                        Serial.println("State: READY (mock)");
                    }
                }
            } else {
                static float lastDrawnBoiler = -1.0;
                static float lastDrawnGH     = -1.0;
                float boilerNow = sharedBoilerTemp.load();
                float ghNow     = sharedGroupheadTemp.load();
                if (abs(boilerNow - lastDrawnBoiler) > 0.1 || abs(ghNow - lastDrawnGH) > 0.1) {
                    display.updateWarmupData(boilerNow, ghNow, isBoilerReady);
                    lastDrawnBoiler = boilerNow;
                    lastDrawnGH     = ghNow;
                }
                if (sharedBoilerTemp.load() >= sharedBoilerTarget.load() && !isBoilerReady) {
                    isBoilerReady = true;
                    calibWindow   = true;
                    heatSoakStartTime = currentTime;
                    Serial.println("Boiler at temp. Starting grouphead heat soak.");
                }
                if (isBoilerReady && sharedGroupheadTemp.load() >= sharedGHTarget.load()) {
                    currentState = READY;
                    isBoilerReady = false;
                    requestNotify = true;
                    display.loadScreen(READY);
                    playReadySound();
                    readyTime = currentTime;
                    timer.start();
                    Serial.println("State: READY");
                }
                if (sharedPumpRunning) {
                    isBoilerReady = false;
                    timer.reset();
                    timer.start();
                    currentState = BREWING;
                    display.loadScreen(BREWING);
                    Serial.println("State: BREWING (cold start)");
                }
            }
            break;
        }

        // CASE: READY
        case READY: {
            auto [minutes, seconds] = timer.getFormattedTime(TimerManager::MINUTES);
            if (mockMode) {
                display.updateReadyData(mockTempBoiler, mockTempGH, minutes, seconds);
                if (currentTime - lastUpdateT > 1000) {
                    lastUpdateT = currentTime;
                    mockTempGH += 0.5;
                }
                if (timer.getSeconds() > 30) {
                    mockTempBoiler = 108.0;
                    timer.reset();
                    timer.start();
                    currentState = BREWING;
                    display.loadScreen(BREWING);
                    Serial.println("State: BREWING (mock)");
                }
            } else {
                static float lastDrawnBoiler = -1.0;
                static float lastDrawnGH     = -1.0;
                static int   lastDrawnSec    = -1;
                static bool  lastShowBoiler  = false;
                float boilerNow  = sharedBoilerTemp.load();
                float ghNow      = sharedGroupheadTemp.load();
                int   nowSec     = (int)timer.getSeconds();
                bool  showBoiler = display.isShowingBoilerTemp();
                // Repaint on temp change, every ticked second, or a temp-toggle press.
                if (abs(boilerNow - lastDrawnBoiler) > 0.1 || abs(ghNow - lastDrawnGH) > 0.1
                        || nowSec != lastDrawnSec || showBoiler != lastShowBoiler) {
                    display.updateReadyData(boilerNow, ghNow, minutes, seconds);
                    lastDrawnBoiler = boilerNow;
                    lastDrawnGH     = ghNow;
                    lastDrawnSec    = nowSec;
                    lastShowBoiler  = showBoiler;
                }
                // TODO: phone notification here
                if (sharedPumpRunning) {
                    calibWindow = false;
                    timer.reset();
                    timer.start();
                    currentState = BREWING;
                    display.loadScreen(BREWING);
                    Serial.println("State: BREWING");
                }
                if (currentTime - readyTime > 60000 && sharedBoilerTemp.load() < sharedBoilerTarget.load()) {
                    calibWindow = false;
                    currentState = WARMUP;
                    display.resetWarmupArc();
                    display.loadScreen(WARMUP);
                    Serial.println("State: WARMUP (temp dropped)");
                }
            }
            break;
        }

        // CASE: BREWING
        case BREWING: {
            auto [seconds, tenths] = timer.getFormattedTime(TimerManager::SECONDS);
            if (mockMode) {
                timer.start();
                display.updateBrewData(seconds, tenths, mockTempBoiler);
                if (currentTime - lastUpdateT > 100) {
                    lastUpdateT = currentTime;
                    mockTempBoiler -= 0.1;
                }
                if (timer.getSeconds() > 25.0) {
                    timer.stop();
                    stateChangeTime = currentTime;
                    portENTER_CRITICAL(&brewMux);
                    brewSession.durationSeconds = timer.getSeconds();
                    brewSession.isComplete = true;
                    portEXIT_CRITICAL(&brewMux);
                    savePending = false;
                    currentState = DONE;
                    sharedBoilerTemp = mockTempBoiler;
                    display.loadScreen(DONE);
                    Serial.println("State: DONE (mock)");
                }
            } else {
                display.updateBrewData(seconds, tenths, sharedBoilerTemp.load());
                display.updateBrewSCT(pumpSensor.getLastStrength(), pumpSensor.getThreshold());
                if (!sharedPumpRunning) {
                    timer.stop();
                    stateChangeTime = currentTime;
                    portENTER_CRITICAL(&brewMux);
                    brewSession.durationSeconds = timer.getSeconds();
                    brewSession.isComplete = true;
                    portEXIT_CRITICAL(&brewMux);
                    savePending = false;
                    currentState = DONE;
                    display.loadScreen(DONE);
                    Serial.println("State: DONE");
                }
            }
            break;
        }

        // CASE: SETTINGS
        // UI overlay. Machine state is frozen; Core 0 keeps reading sensors.
        case SETTINGS:{
            // Calibration window: open from boiler-ready (WARMUP) through end of READY
            calibAvailable.store(calibWindow);

            display.updateSettingsData(sharedSDFreeMB.load(), sharedBrewCount.load());
            static int           lastSavedMusic  = -1;
            static int           lastSavedBoiler = -1;
            static int           lastSavedGH     = -1;
            static int           lastCalibTemp   = -1;
            static unsigned long lastChangeTime  = 0;  // nvsPending lives at loop scope so the exit flush can see it
            int curMusic  = sharedMusicSelect.load();
            int curBoiler = sharedBoilerTarget.load();
            int curGH     = sharedGHTarget.load();
            int curCalib  = sharedCalibTemp.load();

            // On entry, snapshot the current values so an untouched control is never
            // treated as a "change". Without this, the calibration spinbox's default
            // value (!= the -1 sentinel) would recompute tau the instant you open
            // settings, and any unrelated edit (e.g. music) would appear to "save"
            // the calibration too. Now only a control you actually move takes effect.
            if (settingsFreshEntry) {
                settingsFreshEntry = false;
                lastSavedMusic  = curMusic;
                lastSavedBoiler = curBoiler;
                lastSavedGH     = curGH;
                lastCalibTemp   = curCalib;
            }
            if (curMusic != lastSavedMusic || curBoiler != lastSavedBoiler || curGH != lastSavedGH) {
                lastChangeTime = millis();
                nvsPending = true;
            }
            if (nvsPending && millis() - lastChangeTime > 1000) {
                nvsPending = false;
                sysPrefs.begin("system", false);
                sysPrefs.putInt("music",        curMusic);
                sysPrefs.putInt("boilerTarget", curBoiler);
                sysPrefs.putInt("ghTarget",     curGH);
                sysPrefs.end();
                lastSavedMusic  = curMusic;
                lastSavedBoiler = curBoiler;
                lastSavedGH     = curGH;
            }
            if (curCalib != lastCalibTemp && calibAvailable.load()) {
                lastCalibTemp = curCalib;
                // T_sel = current GH temp observed on external thermometer (spinbox)
                // T_0   = actual GH temp at boot (captured from first cold sensor reading)
                // T_inf = Newton's Law asymptote (GH target + 14)
                float T_sel = (float)curCalib;
                float T_0   = sensor.getInitialGroupheadTemp();
                float T_inf = (float)sharedGHTarget.load() + 14.0f;
                float t     = (millis() - heatSoakStartTime) / 1000.0f;
                if (t > 0.0f && T_sel > T_0 && T_sel < T_inf) {
                    float newTau = -t / log((T_inf - T_sel) / (T_inf - T_0));
                    if (newTau >= 100.0f && newTau <= 2400.0f) {
                        sharedTau.store(newTau);
                        pendingTau = newTau;
                        Serial.printf("Heatsoak calibrated: tau = %.1f s (T_sel=%.0f C, t=%.0f s)\n", newTau, T_sel, t);
                    } else {
                        Serial.printf("Calibration rejected: tau = %.1f s out of range (100–2400 s)\n", newTau);
                    }
                }
            }
            break;
        }

        // CASE: DONE
        // Shot finished. Show the final time, then save to SD (with uploading label),
        // then transition once the save completes.
        case DONE:{
            auto [seconds, tenths] = timer.getFormattedTime(TimerManager::SECONDS);
            display.updateDoneData(seconds, tenths);

            if (sharedPumpRunning) {
                // New brew started before we saved — save in the background,
                // transition immediately (uploading label won't be seen, that's fine).
                if (!savePending) {
                    sharedBrewRating.store(display.getDoneRating());
                    requestCsvSave  = true;
                    csvSaveComplete = false;
                }
                savePending = false;
                timer.reset();
                timer.start();
                currentState = BREWING;
                display.loadScreen(BREWING);
                Serial.println("BREWING (again)");
                break;
            }

            // Wait 30 seconds before transitioning
            if (currentTime - stateChangeTime > 30000) {
                if (!savePending) {
                    // Trigger the save and show the uploading label.
                    sharedBrewRating.store(display.getDoneRating());
                    requestCsvSave  = true;
                    csvSaveComplete = false;
                    savePending = true;
                    display.showDoneUploading(true);
                } else if (csvSaveComplete) {
                    // Save finished — hide label and leave.
                    savePending = false;
                    display.showDoneUploading(false);
                    if (sharedBoilerTemp >= sharedBoilerTarget.load()) {
                        currentState = READY;
                        readyTime = currentTime;
                        timer.reset();
                        timer.start();
                        display.loadScreen(READY);
                        Serial.println("READY- still warm enough");
                    } else {
                        currentState = WARMUP;
                        display.resetWarmupArc();
                        display.loadScreen(WARMUP);
                        Serial.println("WARMUP");
                    }
                }
                // else: save in progress, stay on Done with label visible
            }
            break;
        }
    }
    if (mockMode) {
        portENTER_CRITICAL(&tempMux);
        latestTemps.boiler    = mockTempBoiler;
        latestTemps.grouphead = mockTempGH;
        portEXIT_CRITICAL(&tempMux);
    }

    display.update();

    unsigned long elapsed = millis() - frameStart;
    if (elapsed < 33) delay(33 - elapsed);
    else delay(1); // always yield to FreeRTOS scheduler
}

// Returns UTC Unix timestamp if NTP has synced, 0 otherwise.
time_t getUnixTime() {
    struct tm t;
    return getLocalTime(&t, 0) ? mktime(&t) : 0;
}

void sysBoots(){
    sysPrefs.begin("system", false);
    unsigned int boots = sysPrefs.getUInt("boots", 0) + 1;
    sysPrefs.putUInt("boots", boots);
    sysPrefs.end();
    Serial.print("Total Machine Boots: ");
    Serial.println(boots);
}