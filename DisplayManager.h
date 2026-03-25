#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>

class DisplayManager {
private:
    TFT_eSPI tft; // Pointer to the driver object
    TFT_eSprite sprite;

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