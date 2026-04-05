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
    uint8_t draw_buf[screenWidth * screenHeight / 10 * 2];

    // LVGL UI Elements
    lv_obj_t * screen_warmup;
    lv_obj_t * screen_ready;
    lv_obj_t * screen_brewing;
    lv_obj_t * screen_done;
    
    lv_obj_t * label_warmup_temp;
    lv_obj_t * label_ready_temp;
    lv_obj_t * label_brew_timer;
    lv_obj_t * label_brew_temp;
    lv_obj_t * chart_brew;
    lv_chart_series_t * chart_series_temp;

    void preloadScreenWarmup();
    void preloadScreenReady();
    void preloadScreenBrewing();
    void preloadScreenDone();

    // LVGL v9 Touch Variables
    lv_indev_t * indev_touchpad;
    static void my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data);
    
    lv_obj_t * btn_test;
    lv_obj_t * label_btn;
    lv_obj_t * label_timer;
    lv_obj_t * icon_sd;
    static void btn_event_cb(lv_event_t * e);

    // The v9 Bridge
    static void my_disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
    void flush_impl(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

    uint32_t lastTickMillis;

public:
    DisplayManager();
    void init();
    void update(); 
    
    void showStartupScreen();

    void loadScreen(SystemState state); 
    void updateWarmupData(float temp);
    void updateReadyData(float temp);
    void updateBrewData(float timer, float temp);
    void updateDoneData(float timer, float temp);
};

#endif