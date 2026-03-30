#include "DisplayManager.h"

DisplayManager::DisplayManager() : tft(TFT_eSPI()), lastTickMillis(0) {}

void DisplayManager::init() {
    tft.init();
    tft.setRotation(1);

    lv_init();

    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(disp, this); 

    screen_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_main, lv_color_black(), 0);

    // Top Label (Warming Up, Ready, etc.)
    label_top = lv_label_create(screen_main);
    lv_obj_set_style_text_color(label_top, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_top, &lv_font_montserrat_24, 0); // Bigger font

    static lv_point_precise_t line_points[] = { {20, 90}, {300, 90} };
    line_div = lv_line_create(screen_main);
    lv_line_set_points(line_div, line_points, 2);
    lv_obj_set_style_line_color(line_div, lv_color_white(), 0);
    lv_obj_set_style_line_width(line_div, 2, 0);

    // Main Value Label (Temperature, Timer)
    label_main = lv_label_create(screen_main);
    lv_obj_set_style_text_color(label_main, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_main, &lv_font_montserrat_48, 0); // Massive font

    Serial.println("LVGL v9 Display Manager Initialized.");
}

// The static bridge function (v9 signature)
void DisplayManager::my_disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    DisplayManager* manager = (DisplayManager*)lv_display_get_user_data(disp);
    manager->flush_impl(disp, area, px_map);
}

// The actual drawing function
void DisplayManager::flush_impl(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)px_map, w * h, true);
    tft.endWrite();

    lv_display_flush_ready(disp); // Tell LVGL v9 we are done
}

void DisplayManager::update() {
    uint32_t currentMillis = millis();
    lv_tick_inc(currentMillis - lastTickMillis);
    lastTickMillis = currentMillis;
    lv_timer_handler();
}

void DisplayManager::showStartupScreen() {
    lv_screen_load(screen_main);

    // Temporarily hide the dividing line
    lv_obj_add_flag(line_div, LV_OBJ_FLAG_HIDDEN);

    // Center the text for the splash screen
    lv_label_set_text(label_top, "VBM");
    lv_obj_align(label_top, LV_ALIGN_CENTER, 0, -30);
    
    lv_label_set_text(label_main, "Domobar");
    lv_obj_align(label_main, LV_ALIGN_CENTER, 0, 30);

    // Run the LVGL engine to draw the splash screen for 2 seconds
    for(int i = 0; i < 200; i++) {
        update();
        delay(10);
    }

    // Unhide the line and move the labels back to their normal positions!
    lv_obj_remove_flag(line_div, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_align(label_top, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_align(label_main, LV_ALIGN_TOP_MID, 0, 110);
    
    // Clear the text so it's ready for the sensor loop
    lv_label_set_text(label_top, "");
    lv_label_set_text(label_main, "");
}

void DisplayManager::showStatus(const char* label, const char* value) {
    if (lv_screen_active() != screen_main) lv_screen_load(screen_main);
    
    lv_label_set_text(label_top, label);
    lv_label_set_text(label_main, value);
}

void DisplayManager::showStatus(const char* label, float value, const char* unit) {
    if (lv_screen_active() != screen_main) lv_screen_load(screen_main);
    
    lv_label_set_text(label_top, label);
    
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.1f %s", value, unit);
    lv_label_set_text(label_main, buffer);
}

void DisplayManager::showDoneSpam() {
    lv_obj_set_style_bg_color(screen_main, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_label_set_text(label_top, "");
    lv_obj_set_style_text_color(label_main, lv_color_black(), 0);
    lv_label_set_text(label_main, "DONE");
}

void DisplayManager::showDown() {
    lv_obj_set_style_bg_color(screen_main, lv_color_black(), 0);
    lv_label_set_text(label_top, "");
    lv_label_set_text(label_main, "");
}