#include "ServerManager.h"
#include <WiFi.h>
#include <ESPmDNS.h>

static const char INDEX_HTML[] = R"HTML(
<!DOCTYPE html>
<html>
<head>
<title>Espresso Monitor</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
body{font-family:sans-serif;max-width:500px;margin:20px auto;padding:0 16px;color:#111}
h1{font-size:1.3em;margin-bottom:4px}
.temps{display:flex;gap:12px;margin:12px 0}
.card{background:#f0f0f0;border-radius:8px;padding:14px;flex:1;text-align:center}
.val{font-size:2.2em;font-weight:bold;line-height:1}
.lbl{font-size:.75em;color:#666;margin-top:4px}
.state{font-size:.85em;padding:3px 10px;border-radius:10px;background:#e0e0e0;display:inline-block;margin-bottom:8px}
h2{font-size:1em;margin:16px 0 6px}
button{font-size:.8em;padding:3px 10px;cursor:pointer;margin-left:8px}
table{width:100%;border-collapse:collapse}
th,td{padding:6px 8px;text-align:left;border-bottom:1px solid #eee}
th{font-size:.8em;color:#888}
</style>
</head>
<body>
<h1>Espresso Monitor</h1>
<span class="state" id="st">--</span>
<div class="temps">
  <div class="card"><div class="val" id="bl">--</div><div class="lbl">Boiler (C)</div></div>
  <div class="card"><div class="val" id="gh">--</div><div class="lbl">Grouphead (C)</div></div>
</div>
<h2>Brew Log <button onclick="loadLog()">Refresh</button></h2>
<table>
<thead><tr><th>#</th><th>Duration (s)</th><th>Points</th></tr></thead>
<tbody id="log"><tr><td colspan="3">Loading...</td></tr></tbody>
</table>
<script>
function poll(){
  fetch('/api/temps').then(function(r){return r.json();}).then(function(d){
    document.getElementById('bl').textContent=d.boiler.toFixed(1);
    document.getElementById('gh').textContent=d.grouphead.toFixed(1);
    document.getElementById('st').textContent=d.state;
  }).catch(function(){});
}
function loadLog(){
  fetch('/api/log').then(function(r){return r.text();}).then(function(t){
    var rows=t.trim().split('\n').filter(function(l){return l.length>0;});
    var tb=document.getElementById('log');
    if(!rows.length){tb.innerHTML='<tr><td colspan="3">No brews logged yet.</td></tr>';return;}
    tb.innerHTML=rows.map(function(r,i){
      var p=r.split(',');
      return '<tr><td>'+(i+1)+'</td><td>'+parseFloat(p[1]).toFixed(1)+'</td><td>'+p[2]+'</td></tr>';
    }).join('');
  }).catch(function(){});
}
poll();
loadLog();
setInterval(poll,1000);
</script>
</body>
</html>
)HTML";

ServerManager::ServerManager() : _server(80) {}

void ServerManager::begin(const char* ssid, const char* password) {
    _ssid     = ssid;
    _password = password;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("WiFi connecting to: ");
    Serial.println(ssid);
}

void ServerManager::setDataSources(TempSnapshot* temps, portMUX_TYPE* tempMux,
                                    const SystemState* state, SDManager* sd) {
    _temps   = temps;
    _tempMux = tempMux;
    _state   = state;
    _sd      = sd;
}

void ServerManager::handleClient() {
    if (!_ssid) return;

    // Lazy start: register routes and begin serving once WiFi is up.
    if (!_started && WiFi.status() == WL_CONNECTED) {
        tryStartServer();
    }

    if (_started) {
        _server.handleClient();
    }
}

bool ServerManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void ServerManager::tryStartServer() {
    if (MDNS.begin("espresso")) {
        Serial.println("mDNS started: espresso.local");
    }
    _server.on("/",          [this]() { handleRoot(); });
    _server.on("/api/temps", [this]() { handleApiTemps(); });
    _server.on("/api/log",   [this]() { handleApiLog(); });
    _server.begin();
    _started = true;
    Serial.print("Web server started. IP: ");
    Serial.println(WiFi.localIP());
}

void ServerManager::handleRoot() {
    _server.send(200, "text/html", INDEX_HTML);
}

void ServerManager::handleApiTemps() {
    TempSnapshot snap;
    portENTER_CRITICAL(_tempMux);
    snap = *_temps;
    portEXIT_CRITICAL(_tempMux);

    SystemState st = _state ? *_state : WARMUP;

    char json[80];
    snprintf(json, sizeof(json),
             "{\"boiler\":%.1f,\"grouphead\":%.1f,\"state\":\"%s\"}",
             snap.boiler, snap.grouphead, stateToString(st));
    _server.send(200, "application/json", json);
}

void ServerManager::handleApiLog() {
    if (!_sd || !_sd->isInitialized()) {
        _server.send(503, "text/plain", "SD not ready");
        return;
    }
    _server.send(200, "text/plain", _sd->readLogString("/brew_log.csv"));
}

const char* ServerManager::stateToString(SystemState s) {
    switch (s) {
        case WARMUP:  return "Warming Up";
        case READY:   return "Ready";
        case BREWING: return "Brewing";
        case DONE:    return "Done";
        default:      return "Unknown";
    }
}
