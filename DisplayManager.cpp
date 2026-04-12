#include "font/lv_font.h"
#include "misc/lv_color.h"
#include "widgets/label/lv_label.h"
#include "core/lv_obj.h"
#include "misc/lv_types.h"
#include "misc/lv_area.h"
#include "layouts/flex/lv_flex.h"
#include "DisplayManager.h"
#include "src/ui/ui.h"

DisplayManager::DisplayManager() : tft(TFT_eSPI()), lastTickMillis(0) {}

void DisplayManager::init() {
    tft.init();
    tft.setRotation(1);

    // touch calibration
    uint16_t calData[5] = { 248, 3501, 346, 3358, 1 };
    tft.setTouch(calData);

    lv_init();

    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(disp, this);

    indev_touchpad = lv_indev_create();
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touchpad, my_touchpad_read);
    lv_indev_set_user_data(indev_touchpad, this);

    // BOOT THE SQUARELINE UI
    ui_init();

    lv_label_set_text(ui_LabelSD, LV_SYMBOL_SD_CARD);
    lv_label_set_text(ui_LabelWiFi, LV_SYMBOL_WIFI);
    
    animateWarmupWave();

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
    lv_obj_t * screen_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_main, lv_color_black(), 0);

    lv_obj_t * label_top = lv_label_create(screen_main);
    lv_obj_set_style_text_color(label_top, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_top, &lv_font_montserrat_24, 0); // Big font

    static lv_point_precise_t line_points[] = { {20, 90}, {300, 90} };
    lv_obj_t * line_div = lv_line_create(screen_main);
    lv_line_set_points(line_div, line_points, 2);
    lv_obj_set_style_line_color(line_div, lv_color_white(), 0);
    lv_obj_set_style_line_width(line_div, 2, 0);

    lv_obj_t * label_main = lv_label_create(screen_main);
    lv_obj_set_style_text_color(label_main, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_main, &lv_font_montserrat_48, 0); // Massive font

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

void DisplayManager::my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data) {
    // Cast the user_data back to DisplayManager instance
    DisplayManager* manager = (DisplayManager*)lv_indev_get_user_data(indev);
    
    uint16_t touchX, touchY;
    
    bool touched = manager->tft.getTouch(&touchX, &touchY);

    if (!touched) {
        data->state = LV_INDEV_STATE_RELEASED;
    } else {
        data->state = LV_INDEV_STATE_PRESSED;
        
        // Feed the coordinates to LVGL
        data->point.x = touchX;
        data->point.y = touchY;
    }
}

void DisplayManager::loadScreen(SystemState state) {
    switch(state) {
        case WARMUP: lv_screen_load(ui_ScreenWarmup); break; // Use the SquareLine pointer!
        // case READY:  lv_screen_load(screen_ready); break;
        // case BREWING: 
        //    lv_screen_load(screen_brewing); 
        //    break;
        // case DONE:   lv_screen_load(screen_done); break;
    }
}

void DisplayManager::updateWarmupData(float temp) {
    // 1. Update the Text Label
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1f C", temp);
    lv_label_set_text(ui_LabelTemp, tempStr); 

    // 2. Thermodynamics Math (Clamp the temperature)
    float minTemp = 25.0;  
    float maxTemp = 118.0; 
    
    float clampedTemp = temp;
    if (clampedTemp < minTemp) clampedTemp = minTemp;
    if (clampedTemp > maxTemp) clampedTemp = maxTemp;

    // Calculate how "full" the tank should be (0.0 to 1.0)
    float heatPercentage = (clampedTemp - minTemp) / (maxTemp - minTemp);
    
    int waveHeight = (screenHeight + 18) / 2; // (screen height + wave height) / 2

    // The solid water box grows from the bottom up based on temperature
    int waterHeight = (int)(heatPercentage * screenHeight);
    
    // Calculate where the top of that water box is
    // down by subtracting the water height and its own height from the total.
    int waveY = screenHeight - waterHeight - waveHeight;

    // Prevent the wave from clipping through the bottom of the screen when totally cold
    if (waveY > (screenHeight - waveHeight)) {
        waveY = screenHeight - waveHeight;
    }

    lv_obj_set_height(ui_PanelWater, waterHeight);
    lv_obj_set_y(ui_ImgWave, waveY);

    // Color Blending (Blue to Red)
    uint8_t mixRatio = (uint8_t)(heatPercentage * 255.0);
    lv_color_t fluidColor = lv_color_mix(lv_color_hex(0xFF0000), lv_color_hex(0x0000FF), mixRatio);

    // Apply the exact same tint to both the solid box and the white wave cap
    lv_obj_set_style_bg_color(ui_PanelWater, fluidColor, 0);
    lv_obj_set_style_image_recolor(ui_ImgWave, fluidColor, 0);
}

void DisplayManager::updateReadyData(float temp) {}

void DisplayManager::updateBrewData(float timer, float temp) {}

void DisplayManager::updateDoneData(float timer, float temp) {}

void DisplayManager::animateWarmupWave() {
    lv_anim_t a;
    lv_anim_init(&a);
    
    lv_anim_set_var(&a, ui_ImgWave);
    
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    
    lv_anim_set_values(&a, 80, -80); 
    
    lv_anim_set_duration(&a, 3000); 
    
    // constant speed
    lv_anim_set_path_cb(&a, lv_anim_path_linear); 
    
    // loop infinitely
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE); 
    
    // Start the engine
    lv_anim_start(&a);
}

void DisplayManager::setSDState(bool isConnected) {
    this->SDStatus = isConnected;

    lv_color_t color = isConnected ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xFF0000);

    lv_obj_set_style_text_color(ui_LabelSD, color, 0);
}

void DisplayManager::setWifiState(bool isConnected) {
    this->WifiStatus = isConnected;

    lv_color_t color = isConnected ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xFF0000);

    lv_obj_set_style_text_color(ui_LabelWiFi, color, 0);
}