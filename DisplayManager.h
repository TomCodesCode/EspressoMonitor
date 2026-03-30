#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>
#include <lvgl.h>

class DisplayManager {
private:
    TFT_eSPI tft; 
    
    // --- LVGL v9 Display & Buffer ---
    static const uint16_t screenWidth  = 320;
    static const uint16_t screenHeight = 240;
    lv_display_t * disp;
    
    // v9 uses a standard byte array for the buffer (16-bit color = 2 bytes per pixel)
    uint8_t draw_buf[screenWidth * screenHeight / 10 * 2]; 

    // --- LVGL UI Elements ---
    lv_obj_t * screen_main;
    lv_obj_t * label_top;
    lv_obj_t * label_main;
    lv_obj_t * line_div;

    // --- The v9 Bridge ---
    static void my_disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
    void flush_impl(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

    uint32_t lastTickMillis;

public:
    DisplayManager();
    void init();
    void update(); 
    
    void showStartupScreen();
    void showStatus(const char* label, const char* value);
    void showStatus(const char* label, float value, const char* unit); 
    void showDoneSpam();
    void showDown();
};

#endif