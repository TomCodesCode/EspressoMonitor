#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
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

    // helpers
    void animateWarmupWave();

    uint32_t lastTickMillis;

public:
    DisplayManager();
    void init();
    void update(); 
    
    void showStartupScreen();

    void setBrewSession(BrewSession * s, portMUX_TYPE * mux);

    void loadScreen(SystemState state);
    void updateWarmupData(float boilerTemp, float estGroupheadTemp);
    void updateReadyData(float boilerTemp, float estGroupheadTemp, const char * minutes, const char * seconds);
    void updateBrewData(const char* seconds, const char* tenths, float temp);
    void updateDoneData(const char* seconds, const char* tenths);

    void setSDState(bool isConnected);
    void setWifiState(bool isConnected);
};

#endif