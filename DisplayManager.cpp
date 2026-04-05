#include "widgets/label/lv_label.h"
#include "core/lv_obj.h"
#include "misc/lv_types.h"
#include "misc/lv_area.h"
#include "layouts/flex/lv_flex.h"
#include "DisplayManager.h"

DisplayManager::DisplayManager() : tft(TFT_eSPI()), lastTickMillis(0) {}

void DisplayManager::init() {
    tft.init();
    tft.setRotation(1);

    // touch calibraion for this specific ILI9341
    uint16_t calData[5] = { 248, 3501, 346, 3358, 1 };
    tft.setTouch(calData);

    lv_init();

    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(disp, this); 

    // initialize Touch
    indev_touchpad = lv_indev_create();
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touchpad, my_touchpad_read);
    lv_indev_set_user_data(indev_touchpad, this); // Pass the manager object so the static callback can access 'tft'

    // --- STARTUP SCREEN ----------
    

    // --- PRE-BUILD SCREENS IN RAM BY STATES
    // --- WARMUP screen ----------
    preloadScreenWarmup();
    // --- READY screen ----------
    preloadScreenReady();
    // --- BREWING screen ----------
    preloadScreenBrewing();
    // --- DONE screen ----------
    preloadScreenDone();

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

void DisplayManager::btn_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if(code == LV_EVENT_CLICKED) {
        Serial.println(">>> LVGL TOUCH DETECTED! <<<");
        
        // Grab the button and its label to change the text
        lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);
        lv_obj_t * label = lv_obj_get_child(btn, 0);
        lv_label_set_text(label, "IT WORKS!");
    }
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
        case WARMUP: lv_screen_load(screen_warmup); break;
        case READY:  lv_screen_load(screen_ready); break;
        case BREWING: 
            // Clear the old chart data before showing the screen!
            lv_chart_set_all_value(chart_brew, chart_series_temp, LV_CHART_POINT_NONE);
            lv_screen_load(screen_brewing); 
            break;
        case DONE:   lv_screen_load(screen_done); break;
    }
}

void DisplayManager::updateWarmupData(float temp) {
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1f C", temp);
    lv_label_set_text(label_warmup_temp, tempStr);
}

void DisplayManager::updateReadyData(float temp) {
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1f C", temp);
    lv_label_set_text(label_ready_temp, tempStr);
}

void DisplayManager::updateBrewData(float timer, float temp) {
    // Format the numbers
    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%.1fs", timer);
    
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1f C", temp);

    // Push to the UI
    lv_label_set_text(label_brew_timer, timeStr);
    lv_label_set_text(label_brew_temp, tempStr);
    
    // Add the next point to the live graph
    lv_chart_set_next_value(chart_brew, chart_series_temp, (int32_t)temp);
}

void DisplayManager::updateDoneData(float timer, float temp) {}

void DisplayManager::preloadScreenWarmup(){
    screen_warmup = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_warmup, lv_color_black(), 0);
    
    lv_obj_set_flex_flow(screen_warmup, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_color(screen_warmup, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_pad_all(screen_warmup, 0, 0); 
    lv_obj_set_style_pad_column(screen_warmup, 0, 0);

    lv_obj_t * warmup_panel_main = lv_obj_create(screen_warmup);
    lv_obj_set_flex_flow(warmup_panel_main, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(warmup_panel_main, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_pad_all(warmup_panel_main, 0, 0); 
    lv_obj_set_style_pad_column(warmup_panel_main, 0, 0);

    lv_obj_t * warmup_box_status = lv_obj_create(warmup_panel_main);
    lv_obj_set_size(warmup_box_status, lv_pct(100), lv_pct(30));
    lv_obj_set_style_bg_opa(warmup_box_status, LV_OPA_TRANSP, 0);

    lv_obj_t * warmup_label_status = lv_label_create(warmup_box_status);
    lv_label_set_text(warmup_label_status, "WARMUP");
    lv_obj_center(warmup_label_status);

    lv_obj_t * warmup_box_temp = lv_obj_create(warmup_panel_main);
    lv_obj_set_size(warmup_box_temp, lv_pct(100), lv_pct(70));
    lv_obj_set_style_bg_opa(warmup_box_temp, LV_OPA_TRANSP, 0);

    label_warmup_temp = lv_label_create(warmup_box_temp);
    lv_label_set_text(label_warmup_temp, "--.--C");
    lv_obj_center(label_warmup_temp);
}

void DisplayManager::preloadScreenReady(){
    screen_ready = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_ready, lv_color_black(), 0);
    
    lv_obj_set_flex_flow(screen_ready, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_color(screen_ready, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_pad_all(screen_ready, 0, 0); 
    lv_obj_set_style_pad_column(screen_ready, 0, 0);

    lv_obj_t * ready_panel_main = lv_obj_create(screen_ready);
    lv_obj_set_flex_flow(ready_panel_main, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(ready_panel_main, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_pad_all(ready_panel_main, 0, 0); 
    lv_obj_set_style_pad_column(ready_panel_main, 0, 0);

    lv_obj_t * ready_box_status = lv_obj_create(ready_panel_main);
    lv_obj_set_size(ready_box_status, lv_pct(100), lv_pct(50));
    lv_obj_set_style_bg_opa(ready_box_status, LV_OPA_TRANSP, 0);

    lv_obj_t * ready_label_status = lv_label_create(ready_box_status);
    lv_label_set_text(ready_label_status, "READY");
    lv_obj_center(ready_label_status);

    lv_obj_t * ready_box_temp = lv_obj_create(ready_panel_main);
    lv_obj_set_size(ready_box_temp, lv_pct(100), lv_pct(50));
    lv_obj_set_style_bg_opa(ready_box_temp, LV_OPA_TRANSP, 0);

    label_ready_temp = lv_label_create(ready_box_temp);
    lv_label_set_text(label_ready_temp, "??.??C");
    lv_obj_center(label_ready_temp);
}

void DisplayManager::preloadScreenBrewing(){
    screen_brewing = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_brewing, lv_color_black(), 0);

    // --- ROOT SCREEN (Vertical Split) ---
    lv_obj_set_flex_flow(screen_brewing, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(screen_brewing, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_pad_all(screen_brewing, 0, 0); 
    lv_obj_set_style_pad_column(screen_brewing, 0, 0);

    // TOP PANEL (40% Height)- status + temp + time
    lv_obj_t * brewing_top_panel = lv_obj_create(screen_brewing);
    lv_obj_set_size(brewing_top_panel, lv_pct(100), lv_pct(40));
    lv_obj_set_flex_flow(brewing_top_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(brewing_top_panel, 0, 0);
    lv_obj_set_style_bg_opa(brewing_top_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(brewing_top_panel, 0, 0);

    // Status Area (left. 30% width)
    lv_obj_t * box_status = lv_obj_create(brewing_top_panel);
    lv_obj_set_size(box_status, lv_pct(30), lv_pct(100));
    lv_obj_set_style_border_color(box_status, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(box_status, LV_OPA_TRANSP, 0);
    
    lv_obj_t * brewing_label_status = lv_label_create(box_status);
    lv_label_set_text(brewing_label_status, "BREWING");
    lv_obj_center(brewing_label_status);

    // Temp Area (Center, 20%)
    lv_obj_t * brewing_box_temp = lv_obj_create(brewing_top_panel);
    lv_obj_set_size(brewing_box_temp, lv_pct(20), lv_pct(100));
    lv_obj_set_style_border_color(brewing_box_temp, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(brewing_box_temp, LV_OPA_TRANSP, 0);

    label_brew_temp = lv_label_create(brewing_box_temp);
    lv_label_set_text(label_brew_temp, "--00.0C");
    lv_obj_center(label_brew_temp);

    // Timer Area (Right. 50%)
    lv_obj_t * brewing_box_timer = lv_obj_create(brewing_top_panel);
    lv_obj_set_width(brewing_box_timer, lv_pct(100));
    lv_obj_set_flex_grow(brewing_box_timer, 1);
    lv_obj_set_style_border_color(brewing_box_timer, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(brewing_box_timer, LV_OPA_TRANSP, 0);
    
    label_brew_timer = lv_label_create(brewing_box_timer);
    lv_label_set_text(label_brew_timer, "00.0s");
    lv_obj_center(label_brew_timer);

    // BOTTOM PANEL (60% Height)
    lv_obj_t * bottom_panel = lv_obj_create(screen_brewing);
    lv_obj_set_size(bottom_panel, lv_pct(100), lv_pct(60));
    lv_obj_set_style_pad_all(bottom_panel, 0, 0);
    lv_obj_set_style_bg_opa(bottom_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bottom_panel, 0, 0);

    // Chart
    chart_brew = lv_chart_create(bottom_panel);
    lv_obj_set_size(chart_brew, lv_pct(100), lv_pct(100)); // Fill the entire bottom panel
    lv_chart_set_type(chart_brew, LV_CHART_TYPE_LINE);
    lv_obj_set_style_border_color(chart_brew, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(chart_brew, LV_OPA_TRANSP, 0);

    chart_series_temp = lv_chart_add_series(chart_brew, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y);

    // The Icon Taskbar
    lv_obj_t * icon_bar = lv_obj_create(bottom_panel);
    lv_obj_set_size(icon_bar, 100, 30); // Hardcoded size
    // Pin it to the bottom right of the bottom_panel, with a 5px margin
    lv_obj_align(icon_bar, LV_ALIGN_BOTTOM_RIGHT, -5, -5); 
    
    lv_obj_set_style_bg_color(icon_bar, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(icon_bar, LV_OPA_80, 0); // 80% opaque
    lv_obj_set_style_border_width(icon_bar, 1, 0); // Optional border around the HUD
    lv_obj_set_style_border_color(icon_bar, lv_color_white(), 0);
    
    // Create the SD icon text label inside the icon bar
    icon_sd = lv_label_create(icon_bar);
    lv_label_set_text(icon_sd, LV_SYMBOL_SD_CARD " " LV_SYMBOL_WIFI);
    lv_obj_center(icon_sd); 
    lv_obj_set_style_text_color(icon_sd, lv_color_white(), 0);
}

void DisplayManager::preloadScreenDone(){
    screen_done = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_done, lv_color_black(), 0);

    // --- ROOT SCREEN (Vertical Split) ---
    lv_obj_set_flex_flow(screen_done, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(screen_done, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_pad_all(screen_done, 0, 0); 
    lv_obj_set_style_pad_column(screen_done, 0, 0); // No gaps between panels

    // TOP PANEL (40% Height)- status + temp + time
    lv_obj_t * done_top_panel = lv_obj_create(screen_done);
    lv_obj_set_size(done_top_panel, lv_pct(100), lv_pct(40));
    lv_obj_set_flex_flow(done_top_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(done_top_panel, 0, 0);
    lv_obj_set_style_bg_opa(done_top_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(done_top_panel, 0, 0);

    // Status Area (left. 30% width)
    lv_obj_t * box_status = lv_obj_create(done_top_panel);
    lv_obj_set_size(box_status, lv_pct(30), lv_pct(100));
    lv_obj_set_style_border_color(box_status, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(box_status, LV_OPA_TRANSP, 0);
    
    lv_obj_t * brewing_label_status = lv_label_create(box_status);
    lv_label_set_text(brewing_label_status, "BREWING");
    lv_obj_center(brewing_label_status);

    // Temp Area (Center, 20%)
    lv_obj_t * brewing_box_temp = lv_obj_create(done_top_panel);
    lv_obj_set_size(brewing_box_temp, lv_pct(20), lv_pct(100));
    lv_obj_set_style_border_color(brewing_box_temp, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(brewing_box_temp, LV_OPA_TRANSP, 0);

    label_brew_temp = lv_label_create(brewing_box_temp);
    lv_label_set_text(label_brew_temp, "--00.0C");
    lv_obj_center(label_brew_temp);

    // Timer Area (Right. 50%)
    lv_obj_t * brewing_box_timer = lv_obj_create(done_top_panel);
    lv_obj_set_width(brewing_box_timer, lv_pct(100));
    lv_obj_set_flex_grow(brewing_box_timer, 1);
    lv_obj_set_style_border_color(brewing_box_timer, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(brewing_box_timer, LV_OPA_TRANSP, 0);
    
    label_brew_timer = lv_label_create(brewing_box_timer);
    lv_label_set_text(label_brew_timer, "00.0s");
    lv_obj_center(label_brew_timer);

    // BOTTOM PANEL (60% Height)
    lv_obj_t * bottom_panel = lv_obj_create(screen_done);
    lv_obj_set_size(bottom_panel, lv_pct(100), lv_pct(60));
    lv_obj_set_style_pad_all(bottom_panel, 0, 0);
    lv_obj_set_style_bg_opa(bottom_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bottom_panel, 0, 0);

    // Chart
    chart_brew = lv_chart_create(bottom_panel);
    lv_obj_set_size(chart_brew, lv_pct(100), lv_pct(100)); // Fill the entire bottom panel
    lv_chart_set_type(chart_brew, LV_CHART_TYPE_LINE);
    lv_obj_set_style_border_color(chart_brew, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(chart_brew, LV_OPA_TRANSP, 0);

    chart_series_temp = lv_chart_add_series(chart_brew, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y);

    // The Icon Taskbar
    lv_obj_t * icon_bar = lv_obj_create(bottom_panel);
    lv_obj_set_size(icon_bar, 100, 30); // Hardcoded size
    // Pin it to the bottom right of the bottom_panel, with a 5px margin
    lv_obj_align(icon_bar, LV_ALIGN_BOTTOM_RIGHT, -5, -5); 
    
    lv_obj_set_style_bg_color(icon_bar, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(icon_bar, LV_OPA_80, 0); // 80% opaque
    lv_obj_set_style_border_width(icon_bar, 1, 0); // Optional border around the HUD
    lv_obj_set_style_border_color(icon_bar, lv_color_white(), 0);
    
    // Create the SD icon text label inside the icon bar
    icon_sd = lv_label_create(icon_bar);
    lv_label_set_text(icon_sd, LV_SYMBOL_SD_CARD " " LV_SYMBOL_WIFI);
    lv_obj_center(icon_sd); 
    lv_obj_set_style_text_color(icon_sd, lv_color_white(), 0);
}