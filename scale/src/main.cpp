// ============================================================================
// V2 Scale node — ESP32-C3 Super Mini + HX711
//
// Current mode: CALIBRATION NODE. Brings up a WiFi AP + web page to read live
// HX711 values so you can find the calibration factor WITHOUT the OLED.
// (Ported from the standalone calibration sketch into the monorepo.)
//
// The AP is named per SCALE_ID (V2_Scale_1 / V2_Scale_2) so the two scales
// don't collide. SCALE_ID comes from platformio.ini build_flags.
//
// TODO (V2): replace/augment this with the ESP-NOW send path using
// shared/esp_now_protocol.h — send EspNowMessage{MSG_WEIGHT, grams, ...} to the
// espresso-monitor hub. The header is already on the include path via
// lib_extra_dirs, so: #include "esp_now_protocol.h".
//
// Design notes preserved from the calibration bring-up:
//  - DOUT on GPIO4 (GPIO2 is a C3 strapping pin — must stay clear); SCK on GPIO3.
//  - WiFi + web server come up FIRST and are never blocked; the HX711 is read
//    lazily in loop(), so a missing/slow cell can never freeze boot.
//  - TX power forced to max (brownout was ruled out; throttle only cost range).
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HX711.h>
// #include "esp_now_protocol.h"   // <- shared wire format, for the ESP-NOW step

#ifndef SCALE_ID
#define SCALE_ID 0                 // 0 = unspecified (set via platformio env)
#endif

// ---- Pins (C3 Super Mini) ----
#define HX711_DOUT 4               // data (input). GPIO2 avoided: strapping pin.
#define HX711_SCK  3               // clock (output). Normal GPIO, safe.

HX711 scale;
WebServer server(80);

// Latest reading, refreshed in loop() and served by /data (cached so the HTTP
// handler never blocks on the load cell).
volatile long g_lastRaw  = 0;
volatile bool g_hxOnline = false;

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>V2 Scale Calibration</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; margin-top: 40px; background-color: #121212; color: #ffffff; }
    h1 { font-size: 1.8rem; color: #4CAF50; }
    .weight { font-size: 3.5rem; font-weight: bold; margin: 20px 0; color: #ffffff; }
    p { font-size: 1rem; color: #aaaaaa; padding: 0 20px; }
    button { font-size: 1.1rem; padding: 12px 28px; margin-top: 10px; border: none; border-radius: 8px; background:#4CAF50; color:#fff; }
    button:active { background:#3a8f3f; }
    .off { color:#ff5252; }
  </style>
</head>
<body>
  <h1>Scale Calibration Node</h1>
  <p>Raw Load Cell Value (tared):</p>
  <div class="weight" id="raw_val">Loading...</div>
  <button onclick="tare()">TARE (zero)</button>
  <p>Place a known weight on the scale. Divide the number above by the item's
     weight in grams to get your calibration factor.</p>

  <script>
    setInterval(function() {
      fetch('/data')
        .then(r => r.text())
        .then(d => {
          var el = document.getElementById("raw_val");
          el.innerHTML = d;
          el.className = (d.indexOf("DISCONNECT") >= 0) ? "weight off" : "weight";
        })
        .catch(_ => {});
    }, 300);

    function tare() {
      fetch('/tare').catch(_ => {});
    }
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

// Serves the CACHED value — never blocks on the HX711.
void handleData() {
  if (g_hxOnline) {
    server.send(200, "text/plain", String(g_lastRaw));
  } else {
    server.send(200, "text/plain", "HX711 DISCONNECTED");
  }
}

// Tare on demand, with a timeout so a missing cell can't hang the request.
void handleTare() {
  if (scale.wait_ready_timeout(300)) {
    scale.tare(10);
    server.send(200, "text/plain", "TARED");
  } else {
    server.send(200, "text/plain", "HX711 NOT READY");
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n\n--- V2 SCALE (ID %d) CALIBRATION NODE BOOTING ---\n", SCALE_ID);

  // 1) BRING UP WIFI AP FIRST — nothing here blocks on the load cell.
  WiFi.mode(WIFI_AP);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);   // max power (before softAP)

  char ssid[16];
  snprintf(ssid, sizeof(ssid), "V2_Scale_%d", SCALE_ID);
  bool apOk = WiFi.softAP(ssid);         // open network, per-ID name
  WiFi.setTxPower(WIFI_POWER_19_5dBm);   // re-apply: some cores reset it on softAP()
  Serial.printf("TX power set to: %d (0.25 dBm units)\n", WiFi.getTxPower());

  Serial.printf("softAP start: %s\n", apOk ? "OK" : "FAILED (radio/power problem)");
  Serial.printf("Connect to open WiFi: %s\n", ssid);
  Serial.print("Then browse to: http://");
  Serial.println(WiFi.softAPIP());       // default 192.168.4.1
  Serial.print("AP MAC: "); Serial.println(WiFi.softAPmacAddress());

  // 2) WEB SERVER
  server.on("/",     handleRoot);
  server.on("/data", handleData);
  server.on("/tare", handleTare);
  server.begin();
  Serial.println("Web server running.");

  // 3) HX711 — init only. NO blocking tare() in setup.
  scale.begin(HX711_DOUT, HX711_SCK);
  scale.set_scale(1.0f);                 // raw mode for calibration

  if (scale.wait_ready_timeout(500)) {
    scale.tare(10);
    Serial.println("Initial tare done.");
  } else {
    Serial.println("HX711 not detected at boot (will keep trying in loop).");
  }

  Serial.println("--- Ready ---");
}

void loop() {
  server.handleClient();

  // Refresh the cached reading only when the HX711 has data ready (non-blocking).
  if (scale.is_ready()) {
    g_lastRaw  = scale.get_value(1);
    g_hxOnline = true;
  }
}
