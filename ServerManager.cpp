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
body{font-family:sans-serif;max-width:520px;margin:20px auto;padding:0 16px;color:#111}
h1{font-size:1.3em;margin-bottom:4px}
.temps{display:flex;gap:12px;margin:12px 0}
.card{background:#f0f0f0;border-radius:8px;padding:14px;flex:1;text-align:center}
.val{font-size:2.2em;font-weight:bold;line-height:1}
.lbl{font-size:.75em;color:#666;margin-top:4px}
.state{font-size:.85em;padding:3px 10px;border-radius:10px;background:#e0e0e0;display:inline-block;margin-bottom:8px}
h2{font-size:1em;margin:16px 0 6px}
button{font-size:.8em;padding:3px 10px;cursor:pointer;margin-left:8px}
table{width:100%;border-collapse:collapse}
th,td{padding:5px 7px;text-align:left;border-bottom:1px solid #eee;font-size:.82em}
th{color:#888;font-weight:600}
.vbtn{font-size:.75em;padding:2px 7px;cursor:pointer;background:#333;color:#fff;border:none;border-radius:3px}
#modal{display:none;position:fixed;inset:0;background:rgba(0,0,0,.55);z-index:10;align-items:center;justify-content:center}
#modal.open{display:flex}
#mbox{background:#fff;border-radius:8px;padding:14px;width:320px;max-width:95vw}
#mtitle{font-size:.85em;font-weight:bold;margin-bottom:6px}
#mclose{float:right;cursor:pointer;background:none;border:none;font-size:1.1em;padding:0;line-height:1}
#mchart{width:100%;display:block}
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
<thead><tr><th>#</th><th>Time</th><th>Duration (s)</th><th>Pts</th><th></th></tr></thead>
<tbody id="log"><tr><td colspan="5">Loading...</td></tr></tbody>
</table>
<div id="modal">
  <div id="mbox">
    <button id="mclose" onclick="closeChart()">&#10005;</button>
    <div id="mtitle"></div>
    <svg id="mchart" viewBox="0 0 300 120" xmlns="http://www.w3.org/2000/svg"></svg>
  </div>
</div>
<script>
var TMIN=85,TMAX=125,CW=300,CH=100,PAD=12;
function fmtTime(ms,unix){
  if(unix&&unix>0) return new Date(unix*1000).toLocaleString();
  var s=Math.floor(ms/1000),m=Math.floor(s/60),h=Math.floor(m/60);
  m=m%60;s=s%60;
  return 'boot+'+(h?h+'h ':'')+m+'m '+s+'s';
}
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
    if(!rows.length){tb.innerHTML='<tr><td colspan="5">No brews logged yet.</td></tr>';return;}
    tb.innerHTML=rows.map(function(r,i){
      var p=r.split(',');
      var ms=parseInt(p[0]),dur=parseFloat(p[1]),pts=parseInt(p[2]),unix=p.length>=4?parseInt(p[3]):0;
      return '<tr>'
        +'<td>'+(i+1)+'</td>'
        +'<td>'+fmtTime(ms,unix)+'</td>'
        +'<td>'+dur.toFixed(1)+'</td>'
        +'<td>'+pts+'</td>'
        +'<td><button class="vbtn" onclick="showChart('+ms+','+dur.toFixed(1)+')">View</button></td>'
        +'</tr>';
    }).join('');
  }).catch(function(){});
}
function showChart(id,dur){
  fetch('/api/brew?id='+id).then(function(r){
    if(!r.ok)throw new Error();
    return r.text();
  }).then(function(t){
    var vals=t.trim().split('\n').filter(function(l){return l.length>0;}).map(Number);
    if(!vals.length)return;
    var n=vals.length;
    function tx(i){return PAD+(CW-2*PAD)*i/(n>1?n-1:1);}
    function ty(v){
      var y=PAD+(CH-2*PAD)*(1-(v-TMIN)/(TMAX-TMIN));
      return Math.max(PAD,Math.min(CH+PAD,y));
    }
    var pts=vals.map(function(v,i){return tx(i).toFixed(1)+','+ty(v).toFixed(1);}).join(' ');
    var grids=[90,100,110,120].map(function(t){
      var y=ty(t).toFixed(1);
      return '<line x1="'+PAD+'" y1="'+y+'" x2="'+(CW-PAD)+'" y2="'+y
            +'" stroke="#ddd" stroke-width="0.5"/>'
            +'<text x="1" y="'+(parseFloat(y)+3)+'" font-size="6" fill="#aaa">'+t+'</text>';
    }).join('');
    document.getElementById('mchart').innerHTML=grids
      +'<polyline points="'+pts+'" fill="none" stroke="#c00" stroke-width="1.5" stroke-linejoin="round"/>';
    document.getElementById('mtitle').textContent='Brew '+id+' — '+dur+'s, '+n+' pts';
    document.getElementById('modal').className='open';
  }).catch(function(){alert('Chart data not on SD card.');});
}
function closeChart(){document.getElementById('modal').className='';}
poll();loadLog();setInterval(poll,1000);
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
    // Sync time via NTP (UTC); JS converts to local timezone in the browser.
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");

    if (MDNS.begin("espresso")) {
        Serial.println("mDNS started: espresso.local");
    }
    _server.on("/",          [this]() { handleRoot(); });
    _server.on("/api/temps", [this]() { handleApiTemps(); });
    _server.on("/api/log",   [this]() { handleApiLog(); });
    _server.on("/api/brew",  [this]() { handleApiBrewTemps(); });
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

void ServerManager::handleApiBrewTemps() {
    String idStr = _server.arg("id");
    if (idStr.isEmpty() || !_sd || !_sd->isInitialized()) {
        _server.send(400, "text/plain", "missing id or SD not ready");
        return;
    }
    unsigned long brewId = strtoul(idStr.c_str(), nullptr, 10);
    String data = _sd->readBrewTempsString(brewId);
    if (data.isEmpty()) {
        _server.send(404, "text/plain", "brew not found");
        return;
    }
    _server.send(200, "text/plain", data);
}

const char* ServerManager::stateToString(SystemState s) {
    switch (s) {
        case WARMUP:   return "Warming Up";
        case READY:    return "Ready";
        case BREWING:  return "Brewing";
        case DONE:     return "Done";
        case SETTINGS: return "Settings";
        default:       return "Unknown";
    }
}
