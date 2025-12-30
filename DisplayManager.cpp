#include "DisplayManager.h"
#include <Arduino.h>
#include <Wire.h> // Needed for I2C

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

DisplayManager::DisplayManager() : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1) {}

void DisplayManager::init() {
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
        Serial.println(F("Display Alloc Failed"));
        for(;;); // Error. Cannot proceed! Re-plug the device or troubleshoot.
    }
    display.clearDisplay();
}

void DisplayManager::showStartupScreen() {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.println("VBM");
    display.println("Domobar");
    display.display();

    delay(2000);
    display.clearDisplay();
}

void DisplayManager::showStatus(const char* label, const char* value) {
    display.clearDisplay();
    
    display.setTextColor(SSD1306_WHITE);
    
    // Smaller text at the top
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println(label);

    display.drawLine(0, 20, 128, 20, SSD1306_WHITE);
    
    // Larger text in the center
    display.setTextSize(3);
    display.setCursor(0, 35); // center the cursor. 
    display.println(value);
    
    // Push the buffer to the physical hardware
    display.display();
}

void DisplayManager::showStatus(const char* label, float value, const char* unit) {
    display.clearDisplay();
    
    // Label
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print(label);

    display.drawLine(0, 20, 128, 20, SSD1306_WHITE);
    
    // Large Value
    display.setTextSize(3);
    display.setCursor(0, 35);
    display.print(value, 1); // 1 decimal place
    display.print(unit);
    
    display.display();
}

void DisplayManager::showDoneSpam() {
    display.clearDisplay();
    display.setTextSize(4);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.println("DONE");
    display.display();
}

void DisplayManager::showDown() {
    display.clearDisplay();
    display.display();
}