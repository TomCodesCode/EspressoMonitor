// ============================================================================
// Grinder control — ESP32-2424S012C (round GC9A01 C3 board)
//
// PHASE B: DISPLAY BRING-UP TEST (pure TFT_eSPI, no LVGL/touch yet).
// Goal: confirm the panel driver, pins, and backlight are correct. If the round
// screen shows a green ring + "GRINDER" + a blinking dot, the config is right.
//
// Next (once this works):
//   - Phase C: CST816 capacitive touch (I2C SDA=4 SCL=5 INT=0 RST=1)
//   - Phase D: LVGL + SquareLine UI (240x240 round): target weight, live weight,
//              start/stop, status
//   - Phase E: ESP-NOW receive from the scale (shared/esp_now_protocol.h) +
//              SSR relay (control on GPIO20) to auto-stop grind at target weight
// ============================================================================

#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n--- GRINDER round display bring-up ---");

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  // Ring (fits the round panel) + centered text.
  tft.fillCircle(120, 120, 119, TFT_DARKGREEN);
  tft.fillCircle(120, 120, 108, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("GRINDER", 120, 108, 4);
  tft.drawString("display OK", 120, 138, 2);

  Serial.println("Drew test pattern. Green ring + text = pins/driver/backlight OK.");
}

void loop() {
  // Liveness: a small dot cycles colors so you can see the sketch is running.
  static uint32_t t = 0;
  static const uint16_t cols[] = { TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW };
  static uint8_t i = 0;
  if (millis() - t > 500) {
    t = millis();
    tft.fillCircle(120, 176, 8, cols[i]);
    i = (i + 1) & 3;
  }
}
