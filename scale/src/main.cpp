// ============================================================================
// V2 Scale node — ESP32-C3 Super Mini + HX711
//
// Mode: WEIGHT READOUT + CALIBRATION. Brings up a WiFi AP + web page showing
// live grams (calibration factor applied) AND the raw counts (so you can still
// re-calibrate). No OLED needed.
//
// AP is named per SCALE_ID (V2_Scale_1 / V2_Scale_2). SCALE_ID comes from
// platformio.ini build_flags.
//
// TODO (V2): add the ESP-NOW send path using shared/esp_now_protocol.h — send
// EspNowMessage{MSG_WEIGHT, grams, ...} to the espresso-monitor hub. The header
// is already on the include path via lib_extra_dirs, so: #include "esp_now_protocol.h".
//
// Design notes preserved from bring-up:
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

// ---- Calibration factor: raw counts per gram ----
// Each load cell differs, so calibrate PER scale: tare, put a known mass on,
// read the raw value on the page, then factor = raw / grams. The negative sign
// is normal (this cell reads more-negative under load); get_units() divides by
// the factor, so grams come out POSITIVE automatically — no extra *-1 needed.
// (Later this could live in NVS instead of being compiled in.)
#if SCALE_ID == 2
  #define CAL_FACTOR -1.0f         // TODO: grinder scale — not calibrated yet
#else
  #define CAL_FACTOR -621.0f       // scale #1: measured 53.3 g -> -33100 raw
#endif

// ---- Pins (C3 Super Mini) ----
#define HX711_DOUT 4               // data (input). GPIO2 avoided: strapping pin.
#define HX711_SCK  3               // clock (output). Normal GPIO, safe.

HX711 scale;
WebServer server(80);

// Cached readings, refreshed in loop(), served by /data (so the HTTP handler
// never blocks on the load cell).
volatile long  g_lastRaw   = 0;
volatile float g_lastGrams = 0.0f;
volatile bool  g_hxOnline  = false;

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>V2 Scale</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; margin-top: 34px; background-color: #121212; color: #ffffff; }
    h1 { font-size: 1.5rem; color: #4CAF50; }
    .grams { font-size: 4rem; font-weight: bold; margin: 14px 0 4px; color: #ffffff; }
    .raw { font-size: 1rem; color: #888; margin-bottom: 18px; }
    button { font-size: 1.1rem; padding: 12px 28px; margin-top: 6px; border: none; border-radius: 8px; background:#4CAF50; color:#fff; }
    button:active { background:#3a8f3f; }
    .off { color:#ff5252; }
    .note { font-size: .8rem; color:#666; margin-top: 22px; padding: 0 20px; }
  </style>
</head>
<body>
  <h1>V2 Scale</h1>
  <div class="grams" id="g">Loading...</div>
  <div class="raw" id="raw">raw: --</div>
  <button onclick="tare()">TARE (zero)</button>
  <p class="note">To re-calibrate: tare, place a known mass, read <b>raw</b>,
     set factor = raw / grams in firmware (CAL_FACTOR).</p>

  <script>
    setInterval(function() {
      fetch('/data')
        .then(r => r.text())
        .then(d => {
          var g = document.getElementById("g");
          var raw = document.getElementById("raw");
          if (d.indexOf("DISCONNECT") >= 0) {
            g.innerHTML = "—"; g.className = "grams off";
            raw.innerHTML = "HX711 disconnected";
            return;
          }
          var p = d.split(",");          // "grams,raw"
          g.innerHTML = p[0] + " g"; g.className = "grams";
          raw.innerHTML = "raw: " + p[1];
        })
        .catch(_ => {});
    }, 300);

    function tare() { fetch('/tare').catch(_ => {}); }
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

// Serves cached "grams,raw" (or "DISCONNECTED") — never blocks on the HX711.
void handleData() {
  if (g_hxOnline) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f,%ld", g_lastGrams, g_lastRaw);
    server.send(200, "text/plain", buf);
  } else {
    server.send(200, "text/plain", "DISCONNECTED");
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
  Serial.printf("\n\n--- V2 SCALE (ID %d) BOOTING ---\n", SCALE_ID);
  Serial.printf("Calibration factor: %.1f raw/g\n", (float)CAL_FACTOR);

  // 1) BRING UP WIFI AP FIRST — nothing here blocks on the load cell.
  WiFi.mode(WIFI_AP);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);   // max power (before softAP)

  char ssid[16];
  snprintf(ssid, sizeof(ssid), "V2_Scale_%d", SCALE_ID);
  bool apOk = WiFi.softAP(ssid);         // open network, per-ID name
  WiFi.setTxPower(WIFI_POWER_19_5dBm);   // re-apply: some cores reset it on softAP()

  Serial.printf("softAP start: %s\n", apOk ? "OK" : "FAILED (radio/power problem)");
  Serial.printf("Connect to open WiFi: %s\n", ssid);
  Serial.print("Then browse to: http://");
  Serial.println(WiFi.softAPIP());       // default 192.168.4.1

  // 2) WEB SERVER
  server.on("/",     handleRoot);
  server.on("/data", handleData);
  server.on("/tare", handleTare);
  server.begin();
  Serial.println("Web server running.");

  // 3) HX711 — init + apply the calibration factor so get_units()/grams work.
  scale.begin(HX711_DOUT, HX711_SCK);
  scale.set_scale(CAL_FACTOR);           // grams = raw / CAL_FACTOR

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

  // Refresh cached readings only when the HX711 has data ready (non-blocking).
  // One read gives both: raw counts and grams (raw / factor, sign handled).
  if (scale.is_ready()) {
    long raw = scale.get_value(1);       // tared raw counts
    g_lastRaw   = raw;
    g_lastGrams = (float)raw / CAL_FACTOR;
    g_hxOnline  = true;
  }
}
