/*
 * IVTMS ESP-01 WiFi Configuration Panel
 * =======================================
 * Firmware for an ESP-01 (ESP8266) module connected to connector P2
 * on the IVTMS board.
 *
 * P2 is the operator UART port (UART1 of dsPIC30F4013, 115200 baud).
 * This firmware creates a WiFi access-point and serves a single-page
 * web application that replicates every function available via the
 * USB-Serial terminal — without any changes to the dsPIC firmware.
 *
 * Build settings (Arduino IDE)
 *   Board      : Generic ESP8266 Module
 *   Flash Size : 1MB (FS: none)
 *   Crystal Freq: 26 MHz
 *   Upload Speed: 115200
 *   CPU Freq   : 80 MHz
 *
 * Libraries needed (install via Library Manager or Board Manager)
 *   ESP8266 board package  https://arduino.esp8266.com/stable/package_esp8266com_index.json
 *
 * Wiring
 *   ESP-01 3V3 → P2 pin 1 (3.3 V)   *IMPORTANT* – ESP-01 is 3.3 V only
 *   ESP-01 GND → P2 pin 2 (GND)
 *   ESP-01 TX  → P2 pin 3 (dsPIC UART1 RX)
 *   ESP-01 RX  → P2 pin 4 (dsPIC UART1 TX)
 *   ESP-01 GPIO0 → 3V3 (pull-up, ensures normal boot — not flash mode)
 *   ESP-01 GPIO2 → 3V3 (pull-up, required for boot)
 *   ESP-01 EN   → 3V3
 *   ESP-01 RST  → 3V3 (or RC reset circuit)
 *
 * Usage
 *   1. Power on the IVTMS board (the ESP-01 boots automatically)
 *   2. On your phone/laptop connect to WiFi: IVTMS-Config / ivtms1234
 *   3. Open http://192.168.4.1 in any browser
 *   4. Use the web panel to monitor and configure the device
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// -----------------------------------------------------------------------
// Configuration – edit to suit your site
// -----------------------------------------------------------------------
static const char     AP_SSID[]   = "IVTMS-Config";
static const char     AP_PASS[]   = "ivtms1234";   // min 8 chars; set "" for open AP
static const uint8_t  AP_CHANNEL  = 6;
static const uint8_t  AP_MAX_CONN = 4;

static const IPAddress AP_IP (192, 168, 4, 1);
static const IPAddress AP_GW (192, 168, 4, 1);
static const IPAddress AP_SN (255, 255, 255, 0);

static const unsigned int CMD_TIMEOUT_MS = 3000;   // normal command reply window
static const unsigned int CAL_TIMEOUT_MS = 15000;  // extra time for calibration (cmd 0006)
static const unsigned int IDLE_TIMEOUT_MS = 600;   // inter-char silence = end of response

// -----------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------
ESP8266WebServer server(80);
static char rxbuf[1024];   // UART receive buffer

// -----------------------------------------------------------------------
// HTML page stored entirely in flash (PROGMEM)
// -----------------------------------------------------------------------
static const char PAGE_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>IVTMS Config</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#0d1117;color:#c9d1d9;min-height:100vh}
header{background:#161b22;border-bottom:1px solid #30363d;padding:12px 18px;display:flex;align-items:center;gap:10px}
header h1{font-size:1.1em;color:#58a6ff}
header small{font-size:0.75em;color:#8b949e}
nav{background:#161b22;border-bottom:1px solid #30363d;display:flex;overflow-x:auto;-webkit-overflow-scrolling:touch}
nav button{background:none;border:none;color:#8b949e;padding:9px 13px;cursor:pointer;font-size:0.8em;white-space:nowrap;border-bottom:2px solid transparent;touch-action:manipulation}
nav button.on{color:#58a6ff;border-bottom-color:#58a6ff}
nav button:hover{color:#c9d1d9}
.tab{display:none;padding:14px;max-width:860px;margin:0 auto}
.tab.on{display:block}
.card{background:#161b22;border:1px solid #30363d;border-radius:7px;padding:13px;margin-bottom:11px}
.card h3{color:#58a6ff;font-size:0.85em;margin-bottom:9px;padding-bottom:5px;border-bottom:1px solid #30363d}
.row{display:flex;gap:9px;flex-wrap:wrap;margin-bottom:7px}
.row label{font-size:0.78em;color:#8b949e;display:flex;flex-direction:column;gap:3px;flex:1;min-width:90px}
input[type=text],input[type=number],select{background:#0d1117;color:#c9d1d9;border:1px solid #30363d;border-radius:4px;padding:5px 7px;font-size:0.82em;width:100%}
input:focus,select:focus{outline:none;border-color:#58a6ff}
.btn{display:inline-block;background:#238636;color:#fff;border:none;border-radius:4px;padding:6px 13px;cursor:pointer;font-size:0.82em;margin:3px 3px 3px 0;touch-action:manipulation}
.btn:hover{background:#2ea043}
.btn.b{background:#1f6feb}.btn.b:hover{background:#388bfd}
.btn.r{background:#da3633}.btn.r:hover{background:#f85149}
.resp{background:#0d1117;border:1px solid #30363d;border-radius:4px;padding:9px;font-family:monospace;font-size:0.76em;white-space:pre-wrap;min-height:36px;max-height:280px;overflow-y:auto;color:#3fb950;margin-top:7px}
.note{font-size:0.72em;color:#8b949e;margin:3px 0}
input[type=checkbox]{width:auto;accent-color:#58a6ff;margin-right:5px}
.cr{display:flex;flex-wrap:wrap;gap:14px;padding:4px 0}
.cr label{font-size:0.8em;color:#c9d1d9;display:flex;align-items:center;white-space:nowrap}
</style>
</head>
<body>
<header>
  <span style="font-size:1.5em">&#128225;</span>
  <div><h1>IVTMS Config Panel</h1><small>Wireless configuration via ESP-01 &bull; 192.168.4.1</small></div>
</header>
<nav>
  <button class="on"  onclick="tb(this,'s0')">&#128200; Status</button>
  <button onclick="tb(this,'s1')">&#9881; Settings</button>
  <button onclick="tb(this,'s2')">&#127760; Network</button>
  <button onclick="tb(this,'s3')">&#128100; Identity</button>
  <button onclick="tb(this,'s4')">&#128202; Class/Speed</button>
  <button onclick="tb(this,'s5')">&#128295; Advanced</button>
  <button onclick="tb(this,'s6')">&#128187; Terminal</button>
</nav>

<!-- ===== STATUS ===== -->
<div id="s0" class="tab on">
  <div class="card">
    <h3>Device Status</h3>
    <button class="btn b" onclick="cmd('0000','rs0')">&#128200; Query Status</button>
    <button class="btn b" onclick="cmd('0088','rs0')">&#9881; Full Settings</button>
    <button class="btn b" onclick="cmd('0018','rs0')">&#128246; GSM Signal</button>
    <button class="btn b" onclick="cmd('0007','rs0')">&#128260; Frequencies</button>
    <button class="btn b" onclick="cmd('0046','rs0')">&#128202; Vehicle Counts</button>
    <div class="resp" id="rs0">Press a button to query the device&hellip;</div>
  </div>
</div>

<!-- ===== SETTINGS ===== -->
<div id="s1" class="tab">
  <div class="card">
    <h3>Loop Sensors (cmd 0002)</h3>
    <div class="cr">
      <label><input type="checkbox" id="l0">Loop 1</label>
      <label><input type="checkbox" id="l1">Loop 2</label>
      <label><input type="checkbox" id="l2">Loop 3</label>
      <label><input type="checkbox" id="l3">Loop 4</label>
    </div>
    <p class="note">Check to enable each inductive loop sensor.</p>
    <button class="btn" onclick="setLoops()">Save Loops</button>
    <div class="resp" id="rl"></div>
  </div>
  <div class="card">
    <h3>Hysteresis &amp; Margins</h3>
    <div class="row">
      <label>HMM (&#8805;10, cmd 0011)<input type="number" id="hmm" min="10" max="99" value="10"></label>
      <label>Margin Top (cmd 0010)<input type="number" id="mgt" min="0" max="99" value="10"></label>
      <label>Margin Bottom<input type="number" id="mgb" min="0" max="99" value="10"></label>
    </div>
    <button class="btn" onclick="setHMM()">Save HMM</button>
    <button class="btn" onclick="setMargins()">Save Margins</button>
    <div class="resp" id="rhm"></div>
  </div>
  <div class="card">
    <h3>Loop Geometry</h3>
    <div class="row">
      <label>Distance cm (cmd 0004)<input type="number" id="ld" min="0" max="999" value="200"></label>
      <label>Width cm (cmd 0005)<input type="number" id="lw" min="0" max="999" value="100"></label>
    </div>
    <button class="btn" onclick="setGeo()">Save Geometry</button>
    <div class="resp" id="rg"></div>
  </div>
  <div class="card">
    <h3>Device Mode &amp; Power</h3>
    <div class="row">
      <label>H-Device (cmd 0003)
        <select id="au"><option value="0">0 &ndash; Standard</option><option value="1">1 &ndash; H Device</option></select>
      </label>
      <label>Power Type (cmd 0021)
        <select id="pw"><option value="0">0 &ndash; Solar</option><option value="1">1 &ndash; Night Power</option><option value="2">2 &ndash; Backup</option><option value="3">3 &ndash; No Battery</option></select>
      </label>
    </div>
    <button class="btn" onclick="setMode()">Save Mode/Power</button>
    <div class="resp" id="rm"></div>
  </div>
</div>

<!-- ===== NETWORK ===== -->
<div id="s2" class="tab">
  <div class="card">
    <h3>GPRS / APN (cmd 0032)</h3>
    <div class="row">
      <label>APN
        <select id="apn"><option value="0">0 &ndash; MTN IranCell</option><option value="1">1 &ndash; MCI (Hamrah)</option></select>
      </label>
    </div>
    <button class="btn" onclick="setAPN()">Save APN</button>
    <div class="resp" id="ra"></div>
  </div>
  <div class="card">
    <h3>Data Server (cmd 0034)</h3>
    <div class="row">
      <label>Octet 1<input type="number" id="i1" min="0" max="255" value="0"></label>
      <label>Octet 2<input type="number" id="i2" min="0" max="255" value="0"></label>
      <label>Octet 3<input type="number" id="i3" min="0" max="255" value="0"></label>
      <label>Octet 4<input type="number" id="i4" min="0" max="255" value="0"></label>
      <label>Port<input type="number" id="ip" min="1" max="65535" value="8821"></label>
    </div>
    <button class="btn" onclick="setSrv('34','i1','i2','i3','i4','ip','rsrv')">Save Server</button>
    <div class="resp" id="rsrv"></div>
  </div>
  <div class="card">
    <h3>MQTT Broker (cmds 0035 / 0036)</h3>
    <div class="row">
      <label>Octet 1<input type="number" id="m1" min="0" max="255" value="0"></label>
      <label>Octet 2<input type="number" id="m2" min="0" max="255" value="0"></label>
      <label>Octet 3<input type="number" id="m3" min="0" max="255" value="0"></label>
      <label>Octet 4<input type="number" id="m4" min="0" max="255" value="0"></label>
      <label>Port<input type="number" id="mp" min="1" max="65535" value="1883"></label>
    </div>
    <div class="cr" style="margin-top:6px">
      <label><input type="checkbox" id="mqen">Enable MQTT (cmd 0036)</label>
    </div>
    <button class="btn" onclick="setSrv('35','m1','m2','m3','m4','mp','rmqtt')">Save MQTT Broker</button>
    <button class="btn" onclick="setMQen()">Save MQTT Enable</button>
    <div class="resp" id="rmqtt"></div>
  </div>
</div>

<!-- ===== IDENTITY ===== -->
<div id="s3" class="tab">
  <div class="card">
    <h3>Date &amp; Time (cmd 0012)</h3>
    <div class="row">
      <label>Year (00-99)<input type="number" id="ty" min="0" max="99" value="24"></label>
      <label>Month (01-12)<input type="number" id="tm" min="1" max="12" value="1"></label>
      <label>Day (01-31)<input type="number" id="td" min="1" max="31" value="1"></label>
      <label>Hour<input type="number" id="th" min="0" max="23" value="0"></label>
      <label>Minute<input type="number" id="tmi" min="0" max="59" value="0"></label>
      <label>Second<input type="number" id="ts" min="0" max="59" value="0"></label>
    </div>
    <button class="btn b" onclick="fillNow()">Fill Current Time</button>
    <button class="btn" onclick="setTime()">Set Device Time</button>
    <div class="resp" id="rt"></div>
  </div>
  <div class="card">
    <h3>Location Name (cmd 0023, max 32 chars)</h3>
    <div class="row">
      <label>Name<input type="text" id="loc" maxlength="32" placeholder="e.g. Station A - North Lane"></label>
    </div>
    <button class="btn" onclick="setLoc()">Save Location</button>
    <div class="resp" id="rloc"></div>
  </div>
  <div class="card">
    <h3>SMS Number (cmd 0022, 11 digits)</h3>
    <div class="row">
      <label>Mobile number<input type="text" id="sms" maxlength="11" pattern="[0-9]{11}" placeholder="09XXXXXXXXX"></label>
    </div>
    <button class="btn" onclick="setSMS()">Save SMS Number</button>
    <div class="resp" id="rsms"></div>
  </div>
</div>

<!-- ===== CLASS/SPEED ===== -->
<div id="s4" class="tab">
  <div class="card">
    <h3>Vehicle Class Length Limits (cmds 0040&ndash;0045)</h3>
    <p class="note">Maximum length threshold (cm) for each class, 4 digits (0000&ndash;9999).</p>
    <div class="row">
      <label>Class X (0040)<input type="number" id="cx" min="0" max="9999" value="0"></label>
      <label>Class A (0041)<input type="number" id="ca" min="0" max="9999" value="0"></label>
      <label>Class B (0042)<input type="number" id="cb" min="0" max="9999" value="0"></label>
    </div>
    <div class="row">
      <label>Class C (0043)<input type="number" id="cc" min="0" max="9999" value="0"></label>
      <label>Class D (0044)<input type="number" id="cd" min="0" max="9999" value="0"></label>
      <label>Class E (0045)<input type="number" id="ce" min="0" max="9999" value="0"></label>
    </div>
    <button class="btn" onclick="setClasses()">Save Class Limits</button>
    <div class="resp" id="rcl"></div>
  </div>
  <div class="card">
    <h3>Speed Limits km/h (cmd 0019)</h3>
    <p class="note">Day and night speed limits for each lane, 3 digits (0&ndash;999).</p>
    <div class="row">
      <label>Lane 1 Day<input type="number" id="v1d" min="0" max="999" value="120"></label>
      <label>Lane 1 Night<input type="number" id="v1n" min="0" max="999" value="80"></label>
      <label>Lane 2 Day<input type="number" id="v2d" min="0" max="999" value="120"></label>
      <label>Lane 2 Night<input type="number" id="v2n" min="0" max="999" value="80"></label>
    </div>
    <button class="btn" onclick="setSpeeds()">Save Speed Limits</button>
    <div class="resp" id="rsp"></div>
  </div>
</div>

<!-- ===== ADVANCED ===== -->
<div id="s5" class="tab">
  <div class="card">
    <h3>Maintenance</h3>
    <button class="btn"   onclick="cmd('0006','radv',15000)">&#128260; Recalibrate Loops</button>
    <button class="btn"   onclick="cmd('0017','radv')">&#128260; Restart GSM</button>
    <button class="btn"   onclick="cmd('0008','radv')">&#128027; Toggle Debug</button>
    <button class="btn"   onclick="cmd('0025','radv')">&#128246; Toggle GSM Debug</button>
    <button class="btn r" onclick="if(confirm('Reset the MCU now?'))cmd('0013','radv')">&#9888;&#65039; Reset MCU</button>
    <div class="resp" id="radv"></div>
  </div>
</div>

<!-- ===== TERMINAL ===== -->
<div id="s6" class="tab">
  <div class="card">
    <h3>Raw Command Terminal</h3>
    <p class="note">Type a 4-digit command code followed by its parameters, then press Enter or Send.</p>
    <div style="display:flex;gap:8px;margin-top:8px">
      <input type="text" id="raw" placeholder="e.g. 0000 or 0088" style="font-family:monospace;flex:1">
      <button class="btn b" onclick="sendRaw()">Send &#9658;</button>
    </div>
    <p class="note" style="margin-top:5px">
      Common codes: 0000=status &bull; 0088=full settings &bull; 0006=calibrate &bull;
      0007=frequencies &bull; 0008=debug toggle &bull; 0013=reset &bull; 0017=GSM restart
    </p>
    <div class="resp" id="rterm" style="min-height:120px"></div>
  </div>
</div>

<script>
// Tab switcher
function tb(btn, id) {
  document.querySelectorAll('nav button').forEach(b => b.classList.remove('on'));
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('on'));
  btn.classList.add('on');
  document.getElementById(id).classList.add('on');
}

// Generic command send → show in resp element
function cmd(c, rid, tmo) {
  var el = document.getElementById(rid);
  el.textContent = 'Sending ' + c + '\u2026';
  var url = '/cmd?c=' + encodeURIComponent(c) + (tmo ? '&t=' + tmo : '');
  fetch(url, {signal: AbortSignal.timeout ? AbortSignal.timeout((tmo||3000)+1000) : undefined})
    .then(r => r.text())
    .then(t => { el.textContent = t || '(no response)'; el.scrollTop = el.scrollHeight; })
    .catch(e => { el.textContent = 'Network error: ' + e; });
}

// Zero-pad number to w characters
function zp(v, w) { return String(parseInt(v) || 0).padStart(w, '0'); }
// Clamp and pad an IP octet
function oct(id) { return zp(Math.min(255, Math.max(0, parseInt(document.getElementById(id).value) || 0)), 3); }

function setLoops() {
  var v = (document.getElementById('l0').checked?'1':'0') +
          (document.getElementById('l1').checked?'1':'0') +
          (document.getElementById('l2').checked?'1':'0') +
          (document.getElementById('l3').checked?'1':'0');
  cmd('0002' + v, 'rl');
}

function setMargins() {
  cmd('0010' + zp(document.getElementById('mgt').value,2) +
               zp(document.getElementById('mgb').value,2), 'rhm');
}

function setHMM() {
  cmd('0011' + zp(Math.max(10, parseInt(document.getElementById('hmm').value)||0), 2), 'rhm');
}

function setGeo() {
  var dist = zp(document.getElementById('ld').value, 3);
  var wid  = zp(document.getElementById('lw').value, 3);
  var el = document.getElementById('rg');
  el.textContent = 'Saving\u2026';
  fetch('/cmd?c=' + encodeURIComponent('0004' + dist)).then(r=>r.text()).then(t=>{
    el.textContent = '0004: ' + t.trim();
    return fetch('/cmd?c=' + encodeURIComponent('0005' + wid));
  }).then(r=>r.text()).then(t=>{ el.textContent += '\n0005: ' + t.trim(); })
    .catch(e=>{ el.textContent += '\nError: ' + e; });
}

function setMode() {
  var au = document.getElementById('au').value;
  var pw = document.getElementById('pw').value;
  var el = document.getElementById('rm');
  el.textContent = 'Saving\u2026';
  fetch('/cmd?c=' + encodeURIComponent('0003' + au)).then(r=>r.text()).then(t=>{
    el.textContent = '0003: ' + t.trim();
    return fetch('/cmd?c=' + encodeURIComponent('0021' + pw));
  }).then(r=>r.text()).then(t=>{ el.textContent += '\n0021: ' + t.trim(); })
    .catch(e=>{ el.textContent += '\nError: ' + e; });
}

function setAPN() { cmd('0032' + document.getElementById('apn').value, 'ra'); }

function setSrv(code, i1, i2, i3, i4, pp, rid) {
  var p = Math.min(65535, Math.max(1, parseInt(document.getElementById(pp).value) || 1));
  cmd('00' + code + oct(i1) + '.' + oct(i2) + '.' + oct(i3) + '.' + oct(i4) + ',' + zp(p, 5), rid);
}

function setMQen() {
  cmd('0036' + (document.getElementById('mqen').checked ? '1' : '0'), 'rmqtt');
}

function setTime() {
  cmd('0012' + zp(document.getElementById('ty').value,2)  +
               zp(document.getElementById('tm').value,2)  +
               zp(document.getElementById('td').value,2)  +
               zp(document.getElementById('th').value,2)  +
               zp(document.getElementById('tmi').value,2) +
               zp(document.getElementById('ts').value,2), 'rt');
}

function fillNow() {
  var d = new Date();
  document.getElementById('ty').value  = zp(d.getFullYear() % 100, 2);
  document.getElementById('tm').value  = zp(d.getMonth() + 1, 2);
  document.getElementById('td').value  = zp(d.getDate(), 2);
  document.getElementById('th').value  = zp(d.getHours(), 2);
  document.getElementById('tmi').value = zp(d.getMinutes(), 2);
  document.getElementById('ts').value  = zp(d.getSeconds(), 2);
}

function setLoc() {
  var n = document.getElementById('loc').value;
  n = n.padEnd(32, ' ');
  cmd('0023' + n.substring(0, 32), 'rloc');
}

function setSMS() {
  var n = document.getElementById('sms').value;
  n = n.padEnd(11, ' ');
  cmd('0022' + n.substring(0, 11), 'rsms');
}

function setClasses() {
  var ids   = ['cx','ca','cb','cc','cd','ce'];
  var codes = ['0040','0041','0042','0043','0044','0045'];
  var el = document.getElementById('rcl');
  el.textContent = 'Saving\u2026';
  var i = 0;
  function next() {
    if (i >= ids.length) { el.textContent += '\nDone.'; return; }
    var v = zp(Math.min(9999, Math.max(0, parseInt(document.getElementById(ids[i]).value)||0)), 4);
    fetch('/cmd?c=' + encodeURIComponent(codes[i] + v)).then(r=>r.text()).then(t=>{
      el.textContent += '\n' + codes[i] + ': ' + t.trim();
      i++; setTimeout(next, 700);
    }).catch(e=>{ el.textContent += '\nError: ' + e; });
  }
  next();
}

function setSpeeds() {
  cmd('0019' + zp(document.getElementById('v1d').value,3) +
               zp(document.getElementById('v1n').value,3) +
               zp(document.getElementById('v2d').value,3) +
               zp(document.getElementById('v2n').value,3), 'rsp');
}

function sendRaw() {
  var c = document.getElementById('raw').value.trim();
  if (!c) return;
  cmd(c, 'rterm');
}
document.getElementById('raw').addEventListener('keydown', function(e) {
  if (e.key === 'Enter') sendRaw();
});
</script>
</body>
</html>)HTML";

// -----------------------------------------------------------------------
// UART bridge: send cmd to dsPIC, collect full response, return it
// -----------------------------------------------------------------------
static String sendToDsPIC(const String& cmdStr, unsigned int timeout_ms) {
    // Flush any stale bytes in the receive buffer
    while (Serial.available()) Serial.read();

    // Transmit command one byte at a time with a small inter-character gap
    // so the dsPIC's UART1 ISR (which echoes each byte) does not overflow.
    for (size_t i = 0; i < cmdStr.length(); i++) {
        Serial.write((uint8_t)cmdStr[i]);
        delayMicroseconds(300);
        yield();
    }
    Serial.write('\r');   // dsPIC expects a bare CR to end each command
    Serial.flush();

    int          idx       = 0;
    unsigned long start    = millis();
    unsigned long last_rx  = millis();

    while (millis() - start < timeout_ms) {
        while (Serial.available() && idx < (int)(sizeof(rxbuf) - 1)) {
            rxbuf[idx++] = (char)Serial.read();
            last_rx = millis();
        }
        // End-of-response: the dsPIC always finishes with "OK\r\n"
        if (idx >= 4 &&
            rxbuf[idx - 4] == 'O' && rxbuf[idx - 3] == 'K' &&
            rxbuf[idx - 2] == '\r' && rxbuf[idx - 1] == '\n')
            break;
        // Inter-character inactivity: treat as end of response
        if (idx > 0 && (millis() - last_rx) > IDLE_TIMEOUT_MS)
            break;
        yield();
    }

    rxbuf[idx] = '\0';
    return String(rxbuf);
}

// -----------------------------------------------------------------------
// Input sanitiser: allow only printable ASCII, cap at 60 characters
// -----------------------------------------------------------------------
static String sanitiseCmd(const String& raw) {
    String out;
    out.reserve(64);
    for (size_t i = 0; i < raw.length() && out.length() < 60; i++) {
        char c = raw[i];
        if (c >= 0x20 && c < 0x7F) out += c;
    }
    return out;
}

// -----------------------------------------------------------------------
// HTTP handlers
// -----------------------------------------------------------------------
static void handleRoot() {
    server.send_P(200, "text/html", PAGE_HTML);
}

static void handleCmd() {
    if (!server.hasArg(F("c"))) {
        server.send(400, F("text/plain"), F("missing param: c"));
        return;
    }

    String cmdStr = sanitiseCmd(server.arg(F("c")));
    if (cmdStr.length() < 4) {
        server.send(400, F("text/plain"), F("command too short (min 4 digits)"));
        return;
    }

    // Determine timeout: caller may pass ?t=Nms; calibration gets extra time
    unsigned int tmo = CMD_TIMEOUT_MS;
    if (server.hasArg(F("t"))) {
        int tv = server.arg(F("t")).toInt();
        if (tv >= 1000 && tv <= 30000) tmo = (unsigned int)tv;
    }
    if (cmdStr.startsWith(F("0006"))) tmo = CAL_TIMEOUT_MS;

    String resp = sendToDsPIC(cmdStr, tmo);
    server.send(200, F("text/plain"), resp);
}

static void handleNotFound() {
    server.send(404, F("text/plain"), F("404 Not Found"));
}

// -----------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------
void setup() {
    // Hardware UART on GPIO1 (TX) / GPIO3 (RX) – connects to dsPIC P2
    Serial.begin(115200);
    delay(100);

    // Bring up WiFi access point
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_GW, AP_SN);
    WiFi.softAP(AP_SSID, AP_PASS[0] ? AP_PASS : nullptr, AP_CHANNEL, 0, AP_MAX_CONN);

    // mDNS: http://ivtms.local (works on most Android/iOS/Windows)
    MDNS.begin(F("ivtms"));

    // Register HTTP routes
    server.on(F("/"),    HTTP_GET, handleRoot);
    server.on(F("/cmd"), HTTP_GET, handleCmd);
    server.onNotFound(handleNotFound);
    server.begin();
}

// -----------------------------------------------------------------------
// Loop
// -----------------------------------------------------------------------
void loop() {
    server.handleClient();
    MDNS.update();
}
