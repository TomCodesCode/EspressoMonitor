// ============================================================================
// Grinder control — ESP32-S3   (SKELETON)
//
// Role: receive weight/flow from the grinder scale over ESP-NOW, control the
// grinder, and relay data onward to the espresso-monitor hub.
//
// This is a stub that stands up ESP-NOW receive using the SHARED wire format
// so the mesh is wired end-to-end; grinder-driving logic comes later.
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "esp_now_protocol.h"   // shared struct — same bytes on every node

// Called on every inbound ESP-NOW packet.
void onEspNowRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len != (int)ESPNOW_MSG_SIZE) return;                 // wrong size -> ignore
  EspNowMessage msg;
  memcpy(&msg, data, sizeof(msg));
  if (msg.version != ESPNOW_PROTOCOL_VERSION) return;      // version mismatch -> ignore

  if (msg.msgType == MSG_WEIGHT) {
    Serial.printf("[node %u] %.2f g  (%.2f g/s)\n",
                  msg.srcNode, msg.grams, msg.gramsPerSec);
    // TODO: grinder control logic + relay to espresso-monitor hub.
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n--- GRINDER CONTROL (S3) BOOTING ---");

  // ESP-NOW needs WiFi in STA mode, but not connected to an AP.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("FATAL: esp_now_init failed");
    return;
  }
  esp_now_register_recv_cb(onEspNowRecv);

  Serial.print("My MAC: "); Serial.println(WiFi.macAddress());  // peers need this
  Serial.println("--- Ready (listening for ESP-NOW) ---");
}

void loop() {
  // Event-driven via the recv callback; nothing to poll yet.
  delay(10);
}
