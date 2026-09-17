#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <atomic>
#include "SystemState.h"
#include "BrewSession.h"

class DisplayManager {
private:
    TFT_eSPI tft; 
    
    // LVGL v9 Display & Buffer
    static const uint16_t screenWidth  = 320;
    static const uint16_t screenHeight = 240;
    lv_display_t * disp;
    
    // v9 uses a standard byte array for the buffer (16-bit color = 2 bytes per pixel)
    // uint8_t draw_buf[screenWidth * screenHeight / 10 * 2];
    // uint8_t draw_buf_1[screenWidth * screenHeight / 10 * 2];
    uint8_t * draw_buf_1;
    uint8_t * draw_buf_2;

    bool SDStatus = true;
    bool WifiStatus = true;

    // LVGL v9 Touch Variables
    lv_indev_t * indev_touchpad;
    static void my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data);

    // The v9 Bridge
    static void my_disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
    void flush_impl(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

    // Ready screen utils
    bool showingBoilerTemp = false; // Default to Grouphead temp
    static void temp_btn_event_cb(lv_event_t * e);

    // Auto scale chart variables.
    // Brew history now lives in the shared BrewSession (see setBrewSession);
    // this is just a borrowed pointer plus the lock that guards it.
    BrewSession * session = nullptr;
    portMUX_TYPE * sessionMux = nullptr;
    unsigned long lastChartUpdate = 0;
    lv_chart_series_t * brew_ser;
    lv_chart_series_t * done_ser;

    // Settings state
    SystemState*          _currentState    = nullptr;
    SystemState*          _previousState   = nullptr;
    std::atomic<bool>*    _requestLogClear = nullptr;
    std::atomic<int>*     _musicSelect     = nullptr;
    std::atomic<int>*     _boilerTarget    = nullptr;
    std::atomic<int>*     _ghTarget        = nullptr;
    std::atomic<int>*     _calibTemp       = nullptr;
    std::atomic<bool>*    _calibAvailable  = nullptr;
    lv_obj_t*             _spbPlus         = nullptr;
    lv_obj_t*             _spbMinus        = nullptr;
    uint32_t              _lastSettingsFreeMB  = UINT32_MAX;
    int                   _lastSettingsBrewCount = -1;
    const char*           _fwVersion       = "?";  // set from the main sketch (always-recompiled TU)

    // helpers
    void animateWarmupWave();
    // Auto-scale a chart's Y axis (and its numeric scale) to fit the given data.
    void applyChartAutoRange(lv_obj_t* chart, lv_obj_t* yScale, const float* temps, int n);
    int32_t _warmupArcMin = -1;  // set to actual boiler temp on first updateWarmupData call; -1 = unset
    bool _doneRatingDirty = false;
    static void done_rating_slider_cb(lv_event_t* e);
    static void settings_btn_event_cb(lv_event_t* e);
    static void settings_exit_btn_event_cb(lv_event_t* e);
    static void settings_clear_logs_btn_event_cb(lv_event_t* e);
    static void settings_music_cb(lv_event_t* e);
    static void settings_boiler_temp_cb(lv_event_t* e);
    static void settings_gh_temp_cb(lv_event_t* e);
    static void settings_spinbox_inc_cb(lv_event_t* e);
    static void settings_spinbox_dec_cb(lv_event_t* e);
    static void settings_spinbox_calib_cb(lv_event_t* e);

    uint32_t lastTickMillis;

public:
    DisplayManager();
    void init();
    void update(); 
    
    void showStartupScreen();

    void setBrewSession(BrewSession * s, portMUX_TYPE * mux);
    void setSettingsPointers(SystemState* cur, SystemState* prev, std::atomic<bool>* clearFlag);
    void setMusicSelectPointer(std::atomic<int>* sel);
    void setMusicDropdown(int index);
    void setTempTargetPointers(std::atomic<int>* boiler, std::atomic<int>* gh);
    void setBoilerTargetDropdown(int temp);
    void setGHTargetDropdown(int temp);
    void setCalibPointers(std::atomic<int>* temp, std::atomic<bool>* available);

    void loadScreen(SystemState state);
    void resetWarmupArc() { _warmupArcMin = -1; }  // call on genuine warmup entry, not settings-return
    void setFirmwareVersion(const char* v) { _fwVersion = v; }
    void updateSettingsData(uint32_t freeMB, int brewCount);
    void updateWarmupData(float boilerTemp, float estGroupheadTemp, bool boilerReady);
    void updateReadyData(float boilerTemp, float estGroupheadTemp, const char * minutes, const char * seconds);
    void updateBrewData(const char* seconds, const char* tenths, float temp);
    void updateBrewSCT(float strength, float threshold);  // live SCT current readout on the brew screen
    void updateDoneData(const char* seconds, const char* tenths);
    void showDoneUploading(bool show);
    int  getDoneRating();
    bool isShowingBoilerTemp() const { return showingBoilerTemp; }

    void setSDState(bool isConnected);
    void setWifiState(bool isConnected);
};

#endif