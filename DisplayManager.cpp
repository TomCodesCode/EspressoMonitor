#include "DisplayManager.h"
#include <Arduino.h>
// #include <Wire.h> // Needed for I2C

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

DisplayManager::DisplayManager() : tft(TFT_eSPI()), sprite(&tft) {}

void DisplayManager::init() {
    // TFT SPI display initialization
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    // Full screen canvas RAM allocation
    //sprite.setColorDepth(8);
    void *ptr = sprite.createSprite(320,240);
    
    if (ptr == nullptr) {
        Serial.println("CRITICAL ERROR: Not enough RAM for Sprite!");
    } else {
        Serial.println("Sprite RAM allocated successfully.");
    }
}

void DisplayManager::showStartupScreen() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString("VBM", 160, 80, 4);
    tft.drawCentreString("Domobar", 160, 120, 4);

    delay(2000);
    tft.fillScreen(TFT_BLACK);
}

void DisplayManager::showStatus(const char* label, const char* value) {
    sprite.fillSprite(TFT_BLACK);
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    
    sprite.drawCentreString(label, 160, 40, 4); // Top Label
    
    // Draw a dividing line
    sprite.drawLine(20, 90, 300, 90, TFT_WHITE);
    
    // Main Value
    sprite.drawCentreString(value, 160, 130, 6);

    sprite.pushSprite(0,0);
}

void DisplayManager::showStatus(const char* label, float value, const char* unit) {
    sprite.fillSprite(TFT_BLACK);
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    
    sprite.drawCentreString(label, 160, 40, 4);
    sprite.drawLine(20, 90, 300, 90, TFT_WHITE);
    
    // Combine the float and the unit into one string so we can center it easily
    char buffer[20];
    sprintf(buffer, "%.1f %s", value, unit);
    sprite.drawCentreString(buffer, 160, 130, 6);

    sprite.pushSprite(0,0);
}

void DisplayManager::showDoneSpam() {
    tft.fillScreen(TFT_GREEN);
    tft.setTextColor(TFT_BLACK, TFT_GREEN);
    tft.drawCentreString("DONE", 160, 100, 6);
}

void DisplayManager::showDown() {
    tft.fillScreen(TFT_BLACK);
}