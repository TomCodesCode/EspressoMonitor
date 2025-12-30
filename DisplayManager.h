#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class DisplayManager {
private:
    Adafruit_SSD1306 display; // Pointer to the driver object

public:
    DisplayManager();
    void init();
    void showStartupScreen();
    void showStatus(const char* label, const char* value);
    void showStatus(const char* label, float value, const char* unit); // for temperature
    void showDoneSpam();
    void showDown();
};

#endif