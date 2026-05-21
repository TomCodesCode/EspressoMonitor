#include "widgets/label/lv_label.h"
#include "core/lv_obj.h"
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

    uint32_t bufferSize = screenWidth * screenHeight / 10 * 2;
    draw_buf_1 = (uint8_t *)malloc(bufferSize);
    draw_buf_2 = (uint8_t *)malloc(bufferSize);

    if (draw_buf_1 == NULL || draw_buf_2 == NULL) {
        Serial.println("FATAL: Out of RAM!");
        return; 
    }

    lv_display_set_buffers(disp, draw_buf_1, draw_buf_2, bufferSize, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(disp, this);

    indev_touchpad = lv_indev_create();
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touchpad, my_touchpad_read);
    lv_indev_set_user_data(indev_touchpad, this);

    // BOOT THE SQUARELINE UI
    ui_init();

    brew_ser = lv_chart_add_series(ui_BrewChart, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y);
    done_ser = lv_chart_add_series(ui_DoneChart, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y);

    lv_label_set_text(ui_WarmupLabelSD, LV_SYMBOL_SD_CARD);
    lv_label_set_text(ui_WarmupLabelWiFi, LV_SYMBOL_WIFI);
    lv_label_set_text(ui_ReadyLabelSD, LV_SYMBOL_SD_CARD);
    lv_label_set_text(ui_ReadyLabelWiFi, LV_SYMBOL_WIFI);
    lv_label_set_text(ui_BrewLabelSD, LV_SYMBOL_SD_CARD);
    lv_label_set_text(ui_BrewLabelWiFi, LV_SYMBOL_WIFI);
    lv_label_set_text(ui_DoneLabelSD, LV_SYMBOL_SD_CARD);
    lv_label_set_text(ui_DoneLabelWiFi, LV_SYMBOL_WIFI);
    
    animateWarmupWave();

    // Ready screen temp toggle button init
    lv_obj_add_event_cb(ui_ReadyButtonTemp, temp_btn_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(ui_ReadyLabelFlush, LV_OBJ_FLAG_HIDDEN); // No flush recommendation at startup on Ready screen (to avoid 1st frame flicker)

    // Done screen
    lv_obj_add_flag(ui_DoneLabelUploading, LV_OBJ_FLAG_HIDDEN);

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
    lv_label_set_text(label_top, "ESPRESSO");
    lv_obj_align(label_top, LV_ALIGN_CENTER, 0, -30);
    
    lv_label_set_text(label_main, "MONITOR");
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
        case WARMUP:  lv_screen_load(ui_ScreenWarmup); break;
        case READY:   lv_screen_load(ui_ScreenReady);  break;
        case BREWING:{
            lv_screen_load(ui_ScreenBrew);
            currentChartPoint = 0;
            lastChartUpdate = millis(); 

            lv_chart_set_point_count(ui_BrewChart, 300); // 30 seconds
            lv_chart_set_all_value(ui_BrewChart, brew_ser, LV_CHART_POINT_NONE);
            break;
        } 
        case DONE:{
            lv_screen_load(ui_ScreenDone);
            
            int displayPoints = (currentChartPoint < 2) ? 2 : currentChartPoint;
            lv_chart_set_point_count(ui_DoneChart, displayPoints);
            
            // Crash Protection
            if (done_ser != NULL) {
                // Wipe the chart clean of junk memory
                lv_chart_set_all_value(ui_DoneChart, done_ser, LV_CHART_POINT_NONE);
                
                // Safely shift the entire history into the chart
                for(int i = 0; i < currentChartPoint; i++) {
                    lv_chart_set_next_value(ui_DoneChart, done_ser, (int)brewTemperatures[i]);
                }
            }
            break;
        }
    }
}

void DisplayManager::updateWarmupData(float boilerTemp, float estGroupheadTemp) {
    char boilerStr[16];
    snprintf(boilerStr, sizeof(boilerStr), "%.1f C", boilerTemp);
    lv_label_set_text(ui_WarmupLabelBoilerTemp, boilerStr);

    if (estGroupheadTemp == 0.0) {
        lv_obj_set_y(ui_WarmupPanelTemp, 20);
        lv_obj_add_flag(ui_WarmupBarWater, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_WarmupPanelGrouphead, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(ui_WarmupArcBoiler, LV_OBJ_FLAG_HIDDEN);
        lv_arc_set_value(ui_WarmupArcBoiler, (int)boilerTemp);
    } else {
        lv_obj_set_y(ui_WarmupPanelTemp, -30);
        lv_obj_remove_flag(ui_WarmupPanelGrouphead, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(ui_WarmupBarWater, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_WarmupArcBoiler, LV_OBJ_FLAG_HIDDEN);
        char ghStr[16];
        snprintf(ghStr, sizeof(boilerStr), "%.1f C", estGroupheadTemp);
        lv_label_set_text(ui_WarmupLabelGroupheadTemp, ghStr);
    }

    // Thermodynamics Math (Clamp the temperature)
    const float roomTemp = 20.0;
    const float maxBoilerTemp = 117.0;
    const float minTemp = 40.0;
    const float maxTemp = 90.0;
    
    float clampedTemp = estGroupheadTemp;
    if (clampedTemp < minTemp) clampedTemp = minTemp;
    if (clampedTemp > maxTemp) clampedTemp = maxTemp;

    // Calculate how "full" the tank should be (0.0 to 1.0)
    float heatPercentage = (clampedTemp - minTemp) / (maxTemp - minTemp);
    float boilerHeatPercentage = (boilerTemp - roomTemp) / (maxBoilerTemp - roomTemp);
    
    int waveHeight = (screenHeight + 18) / 2; // (screen height + wave height) / 2

    // The solid water box grows from the bottom up based on temperature
    int waterHeight = (int)(heatPercentage * screenHeight);

    lv_bar_set_value(ui_WarmupBarWater, waterHeight, LV_ANIM_OFF);
    
    // Calculate where the top of that water box is
    // down by subtracting the water height and its own height from the total.
    int waveY = screenHeight - waterHeight - waveHeight;

    // Prevent the wave from clipping through the bottom of the screen when totally cold
    if (waveY > (screenHeight - waveHeight)) {
        waveY = screenHeight - waveHeight;
    }

    // lv_obj_set_height(ui_WarmupPanelWater, waterHeight);
    lv_obj_set_y(ui_WarmupImgWave, waveY);

    // Color Blending (Blue to Red)
    uint8_t mixRatio = (uint8_t)(heatPercentage * 255.0);
    lv_color_t fluidColor = lv_color_mix(lv_color_hex(0xFF0000), lv_color_hex(0x0000FF), mixRatio);
    mixRatio = (uint8_t)(boilerHeatPercentage * 255.0);
    lv_color_t arcColor = lv_color_mix(lv_color_hex(0xFF0000), lv_color_hex(0x0000FF), mixRatio);

    lv_obj_set_style_arc_color(ui_WarmupArcBoiler, arcColor, LV_PART_INDICATOR);
    // Apply the exact same tint to both the solid box and the white wave cap
    lv_obj_set_style_bg_color(ui_WarmupBarWater, fluidColor, LV_PART_INDICATOR);
    lv_obj_set_style_image_recolor(ui_WarmupImgWave, fluidColor, 0);
}

void DisplayManager::updateReadyData(float boilerTemp, float estGroupheadTemp, const char* minutes, const char* seconds) {
    char tempStr[16];
    
    if (showingBoilerTemp) {
        snprintf(tempStr, sizeof(tempStr), "%.1f C", boilerTemp);
    } else {
        snprintf(tempStr, sizeof(tempStr), "%.1f C", estGroupheadTemp);
    }

    if (estGroupheadTemp <= 91){
        lv_obj_add_flag(ui_ReadyLabelFlush, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(ui_ReadyLabelFlush, LV_OBJ_FLAG_HIDDEN);
    }
    
    lv_label_set_text(ui_ReadyLabelTimeSeconds, seconds);
    lv_label_set_text(ui_ReadyLabelTimeMinutes, minutes);
    lv_label_set_text(ui_ReadyLabelTemp, tempStr);
}

void DisplayManager::updateBrewData(const char* seconds, const char* tenths, float temp) {
    lv_label_set_text(ui_BrewLabelTimeTenths, tenths);
    lv_label_set_text(ui_BrewLabelTimeSeconds, seconds);
    char boilerStr[16];
    snprintf(boilerStr, sizeof(boilerStr), "%.1f C", temp);
    lv_label_set_text(ui_BrewLabelTemp, boilerStr);

    // SYNCHRONIZED LOGGING: Only fire when the 'tenths' timer rolls over
    static char lastTenth = 'X'; 
    if (tenths[0] != lastTenth) {
        lastTenth = tenths[0];

        // Save to our array
        if (currentChartPoint < MAX_BREW_TIME) {
            brewTemperatures[currentChartPoint] = temp;
            currentChartPoint++;
        }

        // Shift the chart safely
        if (brew_ser != NULL) { 
            lv_chart_set_next_value(ui_BrewChart, brew_ser, (int)temp);
        }
    }
}

void DisplayManager::updateDoneData(const char* seconds, const char* tenths) {
    lv_label_set_text(ui_DoneLabelTimeTenths, tenths);
    lv_label_set_text(ui_DoneLabelTimeSeconds, seconds);

    if(this->WifiStatus){
        lv_obj_remove_flag(ui_DoneLabelUploading, LV_OBJ_FLAG_HIDDEN);
    }
}

void DisplayManager::animateWarmupWave() {
    lv_anim_t a;
    lv_anim_init(&a);
    
    lv_anim_set_var(&a, ui_WarmupImgWave);
    
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    
    lv_anim_set_values(&a, 80, -80); 
    
    lv_anim_set_duration(&a, 4000); 
    
    // constant speed
    lv_anim_set_path_cb(&a, lv_anim_path_linear); 
    
    // loop infinitely
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE); 
    
    // Start the engine
    lv_anim_start(&a);
}

void DisplayManager::temp_btn_event_cb(lv_event_t * e) {
    // Retrieve the DisplayManager instance
    DisplayManager* manager = (DisplayManager*)lv_event_get_user_data(e);
    
    // Toggle the state
    manager->showingBoilerTemp = !manager->showingBoilerTemp;
}

void DisplayManager::setSDState(bool isConnected) {
    this->SDStatus = isConnected;

    lv_color_t color = isConnected ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xFF0000);

    lv_obj_set_style_text_color(ui_WarmupLabelSD, color, 0);
    lv_obj_set_style_text_color(ui_ReadyLabelSD, color, 0);
    lv_obj_set_style_text_color(ui_BrewLabelSD, color, 0);
    lv_obj_set_style_text_color(ui_DoneLabelSD, color, 0);
}

void DisplayManager::setWifiState(bool isConnected) {
    this->WifiStatus = isConnected;

    lv_color_t color = isConnected ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xFF0000);

    lv_obj_set_style_text_color(ui_WarmupLabelWiFi, color, 0);
    lv_obj_set_style_text_color(ui_ReadyLabelWiFi, color, 0);
    lv_obj_set_style_text_color(ui_BrewLabelWiFi, color, 0);
    lv_obj_set_style_text_color(ui_DoneLabelWiFi, color, 0);
}