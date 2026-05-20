#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>
#include <lvgl.h>
#include "SystemState.h"

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

    // Auto scale chart variables
    static const int MAX_BREW_TIME = 120; // 2 minutes max
    int brewTemperatures[MAX_BREW_TIME];
    int currentChartPoint = 0;
    unsigned long lastChartUpdate = 0;

    // helpers
    void animateWarmupWave();

    uint32_t lastTickMillis;

public:
    DisplayManager();
    void init();
    void update(); 
    
    void showStartupScreen();

    void loadScreen(SystemState state);
    void updateWarmupData(float boilerTemp, float estGroupheadTemp);
    void updateReadyData(float boilerTemp, float estGroupheadTemp, const char * minutes, const char * seconds);
    void updateBrewData(float timer, float temp);
    void updateDoneData(float timer, float temp);

    void setSDState(bool isConnected);
    void setWifiState(bool isConnected);
};

#endif