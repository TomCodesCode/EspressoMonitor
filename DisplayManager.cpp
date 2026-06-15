#include "DisplayManager.h"
#include "src/ui/ui.h"

// Dropdown index <-> temperature mapping: option N corresponds to base + N.
// Defined once so the set (temp->index) and get (index->temp) directions can't
// drift apart if the dropdown's lowest option ever changes.
static constexpr int BOILER_TEMP_BASE = 115;
static constexpr int GH_TEMP_BASE     = 85;

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
    lv_obj_add_event_cb(ui_DoneSliderRate, done_rating_slider_cb, LV_EVENT_RELEASED, this);

    // Settings button on Warmup and Ready only
    lv_obj_add_event_cb(ui_WarmupBtnSettings, settings_btn_event_cb,    LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_ReadyBtnSettings,  settings_btn_event_cb,    LV_EVENT_CLICKED, this);
    lv_obj_add_flag(ui_BrewBtnSettings, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_DoneBtnSettings, LV_OBJ_FLAG_HIDDEN);

    // Settings screen buttons
    lv_obj_add_event_cb(ui_SettingsButtonExit,      settings_exit_btn_event_cb,       LV_EVENT_CLICKED,        this);
    lv_obj_add_event_cb(ui_SettingsButtonClearLogs, settings_clear_logs_btn_event_cb, LV_EVENT_CLICKED,        this);
    lv_obj_add_event_cb(ui_SettingsDropdownMusicSelect,  settings_music_cb,       LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(ui_SettingsDropdownBoilerTemp,   settings_boiler_temp_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(ui_SettingsDropdownGHTemp,       settings_gh_temp_cb,     LV_EVENT_VALUE_CHANGED, this);

    // Heatsoak calibration spinbox — range 40–70°C, integer display
    lv_spinbox_set_range(ui_SettingsSpinboxHeatsoak, 41, 70);  // min 41: at 40 (==T_0) calibration's T_sel>T_0 guard fails
    lv_spinbox_set_digit_format(ui_SettingsSpinboxHeatsoak, 2, 0);
    lv_spinbox_set_value(ui_SettingsSpinboxHeatsoak, 50);
    lv_obj_set_width(ui_SettingsSpinboxHeatsoak, 170);
    lv_obj_add_flag(ui_SettingsSpinboxHeatsoak, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(ui_SettingsSpinboxHeatsoak, settings_spinbox_calib_cb, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t* parent = lv_obj_get_parent(ui_SettingsSpinboxHeatsoak);
    _spbMinus = lv_button_create(parent);
    _spbPlus  = lv_button_create(parent);
    lv_obj_set_size(_spbMinus, 35, 35);
    lv_obj_set_size(_spbPlus,  35, 35);
    lv_obj_align_to(_spbMinus, ui_SettingsSpinboxHeatsoak, LV_ALIGN_OUT_LEFT_MID,  -5, 0);
    lv_obj_align_to(_spbPlus,  ui_SettingsSpinboxHeatsoak, LV_ALIGN_OUT_RIGHT_MID,  5, 0);
    lv_obj_center(lv_label_create(_spbMinus)); lv_label_set_text(lv_obj_get_child(_spbMinus, 0), "-");
    lv_obj_center(lv_label_create(_spbPlus));  lv_label_set_text(lv_obj_get_child(_spbPlus,  0), "+");
    lv_obj_add_flag(_spbMinus, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_spbPlus,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(_spbMinus, settings_spinbox_dec_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(_spbPlus,  settings_spinbox_inc_cb, LV_EVENT_CLICKED, this);

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

    // Z pressure threshold for a valid touch. TFT_eSPI defaults to 600 (stiff,
    // needs a firm press). Lower = lighter/more responsive presses, but too low
    // invites noise/ghost touches. ~350 is a good resistive-panel starting point.
    const uint16_t TOUCH_THRESHOLD = 250;
    bool touched = manager->tft.getTouch(&touchX, &touchY, TOUCH_THRESHOLD);

    if (!touched) {
        data->state = LV_INDEV_STATE_RELEASED;
    } else {
        data->state = LV_INDEV_STATE_PRESSED;
        
        // Feed the coordinates to LVGL
        data->point.x = touchX;
        data->point.y = touchY;
    }
}

void DisplayManager::applyChartAutoRange(lv_obj_t* chart, lv_obj_t* yScale, const float* temps, int n) {
    const int STEP     = 5;   // round the range out to clean multiples of this (also drives label spacing)
    const int PAD      = 2;   // degrees of breathing room above/below the data
    const int MIN_SPAN = 10;  // never zoom tighter than this, so a flat shot doesn't magnify noise

    int lo, hi;
    if (temps == nullptr || n <= 0) {
        lo = 90; hi = 120;  // sensible default before any data exists
    } else {
        float mn = temps[0], mx = temps[0];
        for (int i = 1; i < n; i++) {
            if (temps[i] < mn) mn = temps[i];
            if (temps[i] > mx) mx = temps[i];
        }
        // pad, then round outward to the STEP grid (this rounding also gives the
        // range natural hysteresis- it only changes when a reading crosses a multiple of STEP)
        lo = (int)floorf((mn - PAD) / STEP) * STEP;
        hi = (int)ceilf ((mx + PAD) / STEP) * STEP;
        // enforce the minimum span, expanding symmetrically while staying on the grid
        while (hi - lo < MIN_SPAN) {
            lo -= STEP;
            if (hi - lo < MIN_SPAN) hi += STEP;
        }
    }

    // The plotted line and the printed numbers are driven by two separate objects-
    // update both or the labels will lie.
    lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, lo, hi);
    if (yScale != nullptr) {
        lv_scale_set_range(yScale, lo, hi);
        lv_scale_set_total_tick_count(yScale, (hi - lo) + 1);  // a tick per degree
        lv_scale_set_major_tick_every(yScale, STEP);           // labelled tick every STEP degrees
    }
}

void DisplayManager::loadScreen(SystemState state) {
    switch(state) {
        case WARMUP:
            lv_screen_load(ui_ScreenWarmup);
            // NOTE: arc min is NOT reset here- loadScreen(WARMUP) also fires when returning from the SETTINGS overlay, and resetting would re-anchor the
            // arc to the current (hotter) temp. Genuine warmup entries call resetWarmupArc() explicitly.
            break;
        case READY:   lv_screen_load(ui_ScreenReady);  break;
        case BREWING:{
            lv_screen_load(ui_ScreenBrew);

            // Start a fresh recording in the shared session.
            if (session != nullptr) {
                portENTER_CRITICAL(sessionMux);
                session->pointCount = 0;
                session->isComplete = false;
                portEXIT_CRITICAL(sessionMux);
            }
            lastChartUpdate = millis();

            lv_chart_set_point_count(ui_BrewChart, 2); // grows each second with actual data
            lv_chart_set_all_value(ui_BrewChart, brew_ser, LV_CHART_POINT_NONE);
            applyChartAutoRange(ui_BrewChart, ui_BrewChart_Yaxis1, nullptr, 0); // reset to default until data arrives
            break;
        }
        case SETTINGS:{
            lv_screen_load(ui_ScreenSettings);
            lv_label_set_text(ui_SettingsLabelFWVerNum, _fwVersion);
            if (_calibTemp) lv_spinbox_set_value(ui_SettingsSpinboxHeatsoak, _calibTemp->load());
            // Reset cache so labels refresh with new SD stats from Core 0
            _lastSettingsFreeMB    = UINT32_MAX;
            _lastSettingsBrewCount = -1;
            break;
        }
        case DONE:{
            lv_screen_load(ui_ScreenDone);
            lv_slider_set_value(ui_DoneSliderRate, 4, LV_ANIM_OFF);
            _doneRatingDirty = false;
            lv_obj_add_flag(ui_DoneLabelUploading, LV_OBJ_FLAG_HIDDEN);

            // Grab the point count once; the array is no longer being written (brewing has stopped) so we can copy it without holding the lock
            int count = 0;
            if (session != nullptr) {
                portENTER_CRITICAL(sessionMux);
                count = session->pointCount;
                portEXIT_CRITICAL(sessionMux);
            }

            int displayPoints = (count < 2) ? 2 : count;
            lv_chart_set_point_count(ui_DoneChart, displayPoints);

            // Crash Protection
            if (done_ser != NULL && session != nullptr) {
                // Wipe the chart clean of junk memory
                lv_chart_set_all_value(ui_DoneChart, done_ser, LV_CHART_POINT_NONE);

                // Safely shift the entire history into the chart
                for(int i = 0; i < count; i++) {
                    lv_chart_set_next_value(ui_DoneChart, done_ser, (int)session->temperatures[i]);
                }
                // Match the final live-brew scaling (identical helper, identical result)
                applyChartAutoRange(ui_DoneChart, ui_DoneChart_Yaxis1, session->temperatures, count);
            }
            break;
        }
    }
}

int DisplayManager::getDoneRating() {
    return _doneRatingDirty ? (int)lv_slider_get_value(ui_DoneSliderRate) : -1;
}

void DisplayManager::done_rating_slider_cb(lv_event_t* e) {
    DisplayManager* dm = (DisplayManager*)lv_event_get_user_data(e);
    dm->_doneRatingDirty = true;
}

void DisplayManager::updateWarmupData(float boilerTemp, float estGroupheadTemp, bool boilerReady) {
    char boilerStr[16];
    snprintf(boilerStr, sizeof(boilerStr), "%.1f C", boilerTemp);
    lv_label_set_text(ui_WarmupLabelBoilerTemp, boilerStr);

    if (!boilerReady) {
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
    const float maxBoilerTemp = _boilerTarget ? (float)_boilerTarget->load() : 117.0f;
    const float minTemp       = 40.0;
    const float maxTemp       = _ghTarget     ? (float)_ghTarget->load()     : 90.0f;

    if (_warmupArcMin < 0) _warmupArcMin = (int32_t)boilerTemp;
    lv_arc_set_range(ui_WarmupArcBoiler, _warmupArcMin, (int32_t)maxBoilerTemp);
    
    float clampedTemp = estGroupheadTemp;
    if (clampedTemp < minTemp) clampedTemp = minTemp;
    if (clampedTemp > maxTemp) clampedTemp = maxTemp;

    // Calculate how "full" the tank should be (0.0 to 1.0)
    float heatPercentage = (clampedTemp - minTemp) / (maxTemp - minTemp);
    float boilerHeatPercentage = (boilerTemp - _warmupArcMin) / (maxBoilerTemp - _warmupArcMin);
    
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

    // recommend a hot flush at 1.03 * target GH temp
    int maxWantedGHTemp = (int)((_ghTarget ? _ghTarget->load() : 91) * 1.03f);
    if (estGroupheadTemp <= maxWantedGHTemp){
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

    // SYNCHRONIZED LOGGING: Only fire when the 'seconds' timer rolls over
    static char lastSecond = 'X';
    if (seconds[1] != lastSecond) {
        lastSecond = seconds[1];

        // Save to the shared session (guarded; Core 0 may read it).
        if (session != nullptr) {
            portENTER_CRITICAL(sessionMux);
            if (session->pointCount < BREW_MAX_POINTS) {
                session->temperatures[session->pointCount] = temp;
                session->pointCount++;
            }
            portEXIT_CRITICAL(sessionMux);
        }

        // Resize chart to actual sample count so X axis matches brew duration
        if (brew_ser != nullptr && session != nullptr) {
            int n = session->pointCount;
            int displayN = (n < 2) ? 2 : n;
            lv_chart_set_point_count(ui_BrewChart, displayN);
            lv_chart_set_all_value(ui_BrewChart, brew_ser, LV_CHART_POINT_NONE);
            for (int i = 0; i < n; i++) {
                lv_chart_set_next_value(ui_BrewChart, brew_ser, (int)session->temperatures[i]);
            }
            applyChartAutoRange(ui_BrewChart, ui_BrewChart_Yaxis1, session->temperatures, n);
        }
    }
}

void DisplayManager::updateBrewSCT(float strength, float threshold) {
    char sctStr[24];
    snprintf(sctStr, sizeof(sctStr), "%.1f / %.1f", strength, threshold);
    lv_label_set_text(ui_BrewLabelSCTCurrent, sctStr);
}

void DisplayManager::setBrewSession(BrewSession * s, portMUX_TYPE * mux) {
    session = s;
    sessionMux = mux;
}

void DisplayManager::setSettingsPointers(SystemState* cur, SystemState* prev, std::atomic<bool>* clearFlag) {
    _currentState    = cur;
    _previousState   = prev;
    _requestLogClear = clearFlag;
}

void DisplayManager::updateSettingsData(uint32_t freeMB, int brewCount) {
    bool calib = _calibAvailable && _calibAvailable->load();
    auto setHidden = [](lv_obj_t* o, bool hide) {
        if (!o) return;
        hide ? lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN) : lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    };
    setHidden(ui_SettingsSpinboxHeatsoak, !calib);
    setHidden(_spbPlus,  !calib);
    setHidden(_spbMinus, !calib);

    if (freeMB != _lastSettingsFreeMB) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%lu MB", freeMB);
        lv_label_set_text(ui_SettingsLabelSDFree, buf);
        _lastSettingsFreeMB = freeMB;
    }
    if (brewCount != _lastSettingsBrewCount) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", brewCount);
        lv_label_set_text(ui_SettingsLabelBrewCountNum, buf);
        _lastSettingsBrewCount = brewCount;
    }
}

void DisplayManager::settings_btn_event_cb(lv_event_t* e) {
    DisplayManager* dm = (DisplayManager*)lv_event_get_user_data(e);
    if (!dm->_currentState || !dm->_previousState) return;
    *dm->_previousState = *dm->_currentState;
    *dm->_currentState  = SETTINGS;
    dm->loadScreen(SETTINGS);
}

void DisplayManager::settings_exit_btn_event_cb(lv_event_t* e) {
    DisplayManager* dm = (DisplayManager*)lv_event_get_user_data(e);
    if (!dm->_currentState || !dm->_previousState) return;
    SystemState ret = *dm->_previousState;
    *dm->_currentState = ret;
    dm->loadScreen(ret);
}

void DisplayManager::settings_clear_logs_btn_event_cb(lv_event_t* e) {
    DisplayManager* dm = (DisplayManager*)lv_event_get_user_data(e);
    if (dm->_requestLogClear) *dm->_requestLogClear = true;
}

void DisplayManager::settings_music_cb(lv_event_t* e) {
    DisplayManager* dm = (DisplayManager*)lv_event_get_user_data(e);
    if (dm->_musicSelect) {
        dm->_musicSelect->store((int)lv_dropdown_get_selected((lv_obj_t*)lv_event_get_target(e)));
    }
}

void DisplayManager::setMusicSelectPointer(std::atomic<int>* sel) {
    _musicSelect = sel;
}

void DisplayManager::setMusicDropdown(int index) {
    lv_dropdown_set_selected(ui_SettingsDropdownMusicSelect, (uint16_t)index);
}

void DisplayManager::setTempTargetPointers(std::atomic<int>* boiler, std::atomic<int>* gh) {
    _boilerTarget = boiler;
    _ghTarget     = gh;
}

void DisplayManager::setBoilerTargetDropdown(int temp) {
    lv_dropdown_set_selected(ui_SettingsDropdownBoilerTemp, (uint16_t)(temp - BOILER_TEMP_BASE));
}

void DisplayManager::setGHTargetDropdown(int temp) {
    lv_dropdown_set_selected(ui_SettingsDropdownGHTemp, (uint16_t)(temp - GH_TEMP_BASE));
}

void DisplayManager::settings_boiler_temp_cb(lv_event_t* e) {
    DisplayManager* dm = (DisplayManager*)lv_event_get_user_data(e);
    int target = BOILER_TEMP_BASE + (int)lv_dropdown_get_selected((lv_obj_t*)lv_event_get_target(e));
    if (dm->_boilerTarget) dm->_boilerTarget->store(target);
    Serial.print("new boiler temp target: ");
    Serial.println(target);
}

void DisplayManager::settings_gh_temp_cb(lv_event_t* e) {
    DisplayManager* dm = (DisplayManager*)lv_event_get_user_data(e);
    int target = GH_TEMP_BASE + (int)lv_dropdown_get_selected((lv_obj_t*)lv_event_get_target(e));
    if (dm->_ghTarget) dm->_ghTarget->store(target);
    Serial.print("new grouphead temp target: ");
    Serial.println(target);
}

void DisplayManager::settings_spinbox_inc_cb(lv_event_t* e) {
    lv_spinbox_increment(ui_SettingsSpinboxHeatsoak);
}

void DisplayManager::settings_spinbox_dec_cb(lv_event_t* e) {
    lv_spinbox_decrement(ui_SettingsSpinboxHeatsoak);
}

void DisplayManager::settings_spinbox_calib_cb(lv_event_t* e) {
    DisplayManager* dm = (DisplayManager*)lv_event_get_user_data(e);
    if (dm->_calibTemp)
        dm->_calibTemp->store((int)lv_spinbox_get_value(ui_SettingsSpinboxHeatsoak));
}

void DisplayManager::setCalibPointers(std::atomic<int>* temp, std::atomic<bool>* available) {
    _calibTemp      = temp;
    _calibAvailable = available;
}

void DisplayManager::updateDoneData(const char* seconds, const char* tenths) {
    lv_label_set_text(ui_DoneLabelTimeTenths, tenths);
    lv_label_set_text(ui_DoneLabelTimeSeconds, seconds);
}

void DisplayManager::showDoneUploading(bool show) {
    if (show) {
        lv_obj_remove_flag(ui_DoneLabelUploading, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ui_DoneLabelUploading, LV_OBJ_FLAG_HIDDEN);
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