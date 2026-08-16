#pragma once
#include <Arduino.h>

// ArsWebUI v3.0 — Marauder-style Dark UI + Apple Juice
// Endpoints: /scan /scan_clients /deauth /deauth_all /deauth_all_ch /csa
//   /beacon_spam /stop /stopdeauth /udp_flood /savesta /setap /pkt_count
//   /attack_status /log /sysinfo /set /ota_info /setotapass
//   /applejuice /applejuice_stop

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>ArsWebUI v3 — Marauder</title>
<style>
:root{
  --bg:#0b0e14;--card:#12161f;--border:#1c2333;
  --accent:#00d4aa;--red:#ff3b5c;--yellow:#ffb020;
  --green:#00d4aa;--purple:#a78bfa;--blue:#38bdf8;
  --text:#e2e8f0;--muted:#64748b;--panel:#0f131a;
}
*{margin:0;padding:0;box-sizing:border-box}
body{background:var(--bg);color:var(--text);
     font-family:'Segoe UI',system-ui,sans-serif;min-height:100vh}
.wrap{max-width:960px;margin:0 auto;padding:10px}

/* Header — Marauder style */
.hdr{background:linear-gradient(135deg,#0a0f18,#0d1520);
     border:1px solid #1a2332;border-radius:12px;
     padding:16px;margin-bottom:10px;text-align:center;
     position:relative;overflow:hidden}
.hdr::before{content:'';position:absolute;inset:0;
  background:radial-gradient(ellipse at top,#00d4aa11,transparent 60%);
  pointer-events:none}
.hdr h1{font-size:22px;font-weight:900;letter-spacing:4px;
        color:var(--accent);text-shadow:0 0 20px #00d4aa44}
.hdr-sub{color:var(--muted);font-size:10px;letter-spacing:2px;margin-top:4px}
.live-row{display:flex;justify-content:center;gap:20px;
          margin-top:10px;font-size:11px;font-weight:700;letter-spacing:1px;
          flex-wrap:wrap}
.live-row span{color:var(--muted)}
.live-row b{color:var(--accent)}
#liveStatus{color:var(--purple)}
.net-row{display:flex;justify-content:center;gap:14px;
         margin-top:6px;font-size:10px;letter-spacing:.8px;flex-wrap:wrap}
.net-row span{color:var(--muted)}
.net-row b{color:var(--green)}
.net-row .ota-b{color:var(--yellow)}

/* Sidebar nav tabs */
.nav{display:flex;gap:4px;margin-bottom:10px;flex-wrap:wrap}
.nav-btn{flex:1;min-width:80px;padding:10px 8px;background:var(--panel);
         border:1px solid var(--border);border-radius:8px;color:var(--muted);
         font-size:10px;font-weight:800;letter-spacing:1px;text-transform:uppercase;
         cursor:pointer;transition:all .15s;text-align:center}
.nav-btn:hover{border-color:var(--accent);color:var(--accent)}
.nav-btn.active{background:#0a2018;border-color:var(--accent);color:var(--accent);
                box-shadow:0 0 12px #00d4aa22}

/* Cards */
.card{background:var(--card);border:1px solid var(--border);
      border-radius:10px;padding:14px;margin-bottom:10px;display:none}
.card.show{display:block}
.card-h{font-size:10px;font-weight:800;letter-spacing:2px;
        text-transform:uppercase;color:var(--muted);
        border-bottom:1px solid var(--border);
        padding-bottom:8px;margin-bottom:12px;
        display:flex;align-items:center;gap:8px}
.dot{width:6px;height:6px;border-radius:50%;background:var(--accent);flex-shrink:0}
.dot.r{background:var(--red)}.dot.y{background:var(--yellow)}
.dot.p{background:var(--purple)}.dot.g{background:var(--green)}
.dot.o{background:#f59e0b}.dot.b{background:var(--blue)}

/* Buttons */
.btn{padding:9px 14px;border:none;border-radius:7px;cursor:pointer;
     font-size:11px;font-weight:700;letter-spacing:.4px;
     font-family:inherit;transition:all .13s}
.btn:active{transform:scale(.96)}
.btn-blue{background:#0369a1;color:#fff}
.btn-blue:hover{background:#0284c7}
.btn-red{background:#be123c;color:#fff}
.btn-red:hover{background:#e11d48}
.btn-yel{background:#b45309;color:#fff}
.btn-yel:hover{background:#d97706}
.btn-pur{background:#6d28d9;color:#fff}
.btn-pur:hover{background:#7c3aed}
.btn-green{background:#047857;color:#fff}
.btn-green:hover{background:#059669}
.btn-stop{background:#1c0a0a;color:#fca5a5;border:1px solid #7f1d1d}
.btn-stop:hover{background:#7f1d1d}
.btn-grey{background:#151b28;color:#94a3b8;border:1px solid var(--border)}
.btn-grey:hover{background:#1e2742}
.btn-sel{padding:4px 9px;background:#0369a1;color:#fff;border:none;
         border-radius:5px;cursor:pointer;font-size:10px;font-weight:700}
.btn-sel:hover{background:#0284c7}
.btn-csa{padding:4px 9px;background:#6d28d9;color:#fff;border:none;
         border-radius:5px;cursor:pointer;font-size:10px;font-weight:700}
.btn-csa:hover{background:#5b21b6}
.btn-auto{padding:4px 9px;border:1px solid var(--border);background:#0d1321;
          color:var(--muted);border-radius:5px;cursor:pointer;
          font-size:10px;font-weight:700;transition:all .15s}
.btn-auto.on{background:#0a2010;border-color:var(--green);color:var(--green)}

/* Badge */
.badge{display:inline-block;padding:3px 11px;border-radius:20px;
       font-size:10px;font-weight:800;letter-spacing:.8px}
.b-off{background:#111620;color:#4b5675}
.b-on{background:#3b0808;color:#fca5a5;animation:pulse 1.4s infinite}
.b-udp{background:#3b2000;color:#fbbf24}
.b-bcn{background:#1e0a40;color:#c4b5fd}
.b-csa{background:#002a28;color:#6ee7b7}
.b-aj{background:#1a1030;color:#c4b5fd}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.55}}

/* Tables */
table{width:100%;border-collapse:collapse;font-size:11px}
th{padding:7px 5px;text-align:left;color:var(--muted);font-size:9px;
   letter-spacing:1px;border-bottom:1px solid var(--border);font-weight:700;
   text-transform:uppercase}
td{padding:7px 5px;border-bottom:1px solid #0b0e14;vertical-align:middle}
tr:hover td{background:#141925}

/* Forms */
.field{margin-bottom:11px}
.field label{display:block;font-size:9px;color:var(--muted);
             font-weight:700;letter-spacing:1px;margin-bottom:4px;
             text-transform:uppercase}
.field input,.field select{
  width:100%;padding:8px 11px;background:#07090e;
  border:1px solid var(--border);border-radius:6px;
  color:var(--text);font-size:12px;font-family:inherit;transition:border .12s}
.field input:focus,.field select:focus{
  outline:none;border-color:var(--accent);
  box-shadow:0 0 0 2px rgba(0,212,170,.12)}
.field input::placeholder{color:#2a3348}
.row2{display:grid;grid-template-columns:1fr 1fr;gap:10px}

/* Intensity */
.inten-row{display:grid;grid-template-columns:repeat(4,1fr);gap:5px;margin-bottom:11px}
.ib{padding:7px 3px;border:1px solid var(--border);border-radius:6px;
    background:#07090e;color:var(--muted);cursor:pointer;
    font-size:9px;font-weight:700;letter-spacing:.4px;text-align:center;
    transition:all .13s;line-height:1.6}
.ib:hover{border-color:var(--accent);color:var(--accent)}
.il{background:#081a30;border-color:#38bdf8;color:#38bdf8}
.im{background:#1c1400;border-color:#f59e0b;color:#fbbf24}
.ih{background:#1f0b00;border-color:#ef4444;color:#fca5a5}
.ix{background:#140028;border-color:#a855f7;color:#d8b4fe}

/* Stats */
.stats{display:grid;grid-template-columns:repeat(3,1fr);gap:7px}
.stat{background:#07090e;border:1px solid var(--border);
      border-radius:8px;padding:11px}
.stat-l{color:var(--muted);font-size:8px;letter-spacing:1.5px;
        text-transform:uppercase;font-weight:700;margin-bottom:4px}
.stat-v{color:var(--accent);font-weight:800;font-size:14px;word-break:break-all}

/* OTA */
.ota-block{background:#070b10;border:1px solid #1a2a18;
           border-radius:8px;padding:12px;margin-bottom:10px}
.ota-block .ota-ip{font-family:monospace;font-size:13px;
                   color:var(--green);font-weight:800;letter-spacing:.5px}
.ota-block .ota-sub{font-size:10px;color:var(--muted);margin-top:4px;line-height:1.7}

/* Misc */
.ctrls{display:flex;flex-wrap:wrap;gap:6px;margin-top:10px}
.info{background:#0a1628;border-left:3px solid var(--accent);
      padding:9px 12px;border-radius:5px;font-size:11px;
      color:#93c5fd;margin:7px 0;line-height:1.6}
.hint{font-size:9px;color:var(--muted);margin-top:-7px;margin-bottom:10px}
.foot{text-align:center;padding:12px;color:#1e2742;font-size:9px;
      letter-spacing:2px;margin-top:6px}
.scan-hdr{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.scan-auto-lbl{font-size:9px;color:var(--muted);font-weight:700;
               letter-spacing:1px;text-transform:uppercase}
.loading{text-align:center;padding:20px;color:var(--muted);font-size:12px}
.log-box{background:#050708;border:1px solid var(--border);
         border-radius:6px;padding:9px 11px;font-family:monospace;
         font-size:10px;color:#4ade80;max-height:160px;overflow-y:auto;
         white-space:pre-wrap;line-height:1.75;margin-top:7px}
@media(max-width:560px){
  .row2,.inten-row{grid-template-columns:1fr 1fr}
  .stats{grid-template-columns:1fr 1fr}
  .nav-btn{min-width:70px;font-size:9px}
}
</style>
</head>
<body>
<div class="wrap">

<div class="hdr">
  <h1>ArsWebUI</h1>
  <div class="hdr-sub">ESP32-S3 N16R8 ▸ 19.5 dBm ▸ v3.0 MARAUDER</div>
  <div class="live-row">
    <span>PKTS <b id="liveCount">0</b></span>
    <span>STATUS <b id="liveStatus">IDLE</b></span>
    <span>HEAP <b id="liveHeap">—</b></span>
  </div>
  <div class="net-row">
    <span>WEB <b id="liveWebIP">—</b></span>
    <span>OTA <b class="ota-b" id="liveOtaIP">—</b></span>
  </div>
</div>

<div class="nav">
  <button class="nav-btn active" onclick="showTab('scan')">SCAN</button>
  <button class="nav-btn" onclick="showTab('attack')">ATTACK</button>
  <button class="nav-btn" onclick="showTab('beacon')">BEACON</button>
  <button class="nav-btn" onclick="showTab('juice')">APPLE</button>
  <button class="nav-btn" onclick="showTab('udp')">UDP</button>
  <button class="nav-btn" onclick="showTab('config')">CONFIG</button>
  <button class="nav-btn" onclick="showTab('sys')">SYS</button>
</div>

<!-- SCAN -->
<div class="card show" id="tab-scan">
  <div class="card-h">
    <span class="dot"></span>
    <div class="scan-hdr" style="flex:1">
      <span>Network Scanner</span>
      <span class="scan-auto-lbl" style="margin-left:auto">AUTO</span>
      <button class="btn-auto" id="autoScanBtn" onclick="toggleAutoScan()">OFF</button>
    </div>
  </div>
  <div class="ctrls">
    <button class="btn btn-blue" onclick="scanWiFi()">SCAN NETWORKS</button>
    <button class="btn btn-blue" onclick="scanClients()">PASSIVE CLIENTS</button>
  </div>
  <div id="scanArea"><div class="loading">Tap SCAN to discover networks</div></div>
  <div id="clientArea" style="display:none;margin-top:12px">
    <div style="display:flex;align-items:center;gap:8px;margin-bottom:8px">
      <span style="font-size:9px;color:var(--muted);font-weight:700;letter-spacing:1px;text-transform:uppercase">PASSIVE AUTO-REFRESH</span>
      <button class="btn-auto" id="autoClientBtn" onclick="toggleAutoClient()">OFF</button>
    </div>
    <div id="clientTable"><div class="loading">...</div></div>
  </div>
</div>

<!-- ATTACK -->
<div class="card" id="tab-attack">
  <div class="card-h"><span class="dot r"></span>Deauth &amp; CSA</div>
  <div class="row2">
    <div class="field">
      <label>Target BSSID</label>
      <input type="text" id="bssidInput" placeholder="AA:BB:CC:DD:EE:FF" maxlength="17" style="text-transform:uppercase;font-family:monospace">
    </div>
    <div class="field">
      <label>SSID (display)</label>
      <input type="text" id="ssidInput" placeholder="auto-filled" readonly style="color:#4b5675">
    </div>
  </div>
  <div class="row2">
    <div class="field">
      <label>Channel</label>
      <select id="channelSelect">
        <option value="0">— select —</option>
        <option value="1">1</option><option value="2">2</option><option value="3">3</option>
        <option value="4">4</option><option value="5">5</option><option value="6">6</option>
        <option value="7">7</option><option value="8">8</option><option value="9">9</option>
        <option value="10">10</option><option value="11">11</option>
        <option value="12">12</option><option value="13">13</option>
      </select>
    </div>
    <div class="field">
      <label>Client MAC (optional)</label>
      <input type="text" id="clientInput" placeholder="empty = broadcast" maxlength="17" style="text-transform:uppercase;font-family:monospace">
    </div>
  </div>
  <div class="field">
    <label>Attack Intensity</label>
    <div class="inten-row">
      <div class="ib il" data-v="1" onclick="setInten(1)">LOW<br><span style="font-size:8px;opacity:.6">8f</span></div>
      <div class="ib" data-v="2" onclick="setInten(2)">MED<br><span style="font-size:8px;opacity:.6">20f</span></div>
      <div class="ib" data-v="3" onclick="setInten(3)">HIGH<br><span style="font-size:8px;opacity:.6">40f</span></div>
      <div class="ib" data-v="4" onclick="setInten(4)">MAX<br><span style="font-size:8px;opacity:.6">80f</span></div>
    </div>
  </div>
  <div class="row2" style="margin-bottom:10px">
    <div>
      <label style="font-size:9px;color:var(--muted);letter-spacing:1px;display:block;margin-bottom:5px">MAC RANDOMIZE</label>
      <div style="display:flex;gap:6px">
        <button class="btn btn-grey" id="mrOn" onclick="setMacRand(1)" style="flex:1;font-size:10px">ON</button>
        <button class="btn btn-grey" id="mrOff" onclick="setMacRand(0)" style="flex:1;font-size:10px">OFF</button>
      </div>
    </div>
    <div>
      <label style="font-size:9px;color:var(--muted);letter-spacing:1px;display:block;margin-bottom:8px">STATUS</label>
      <span id="targetStatus" class="badge b-off">IDLE</span>
      <div id="typeLabel" style="font-size:10px;color:var(--muted);margin-top:5px"></div>
    </div>
  </div>
  <div class="ctrls">
    <button class="btn btn-red" onclick="startDeauth()">DEAUTH TARGET</button>
    <button class="btn btn-red" onclick="startDeauthAll()">DEAUTH ALL AP-CH</button>
    <button class="btn btn-red" onclick="startDeauthAllCh()" style="background:#7f1d1d;border:1px solid #ef4444">DEAUTH CH1-13</button>
    <button class="btn btn-pur" onclick="startCSA()">CSA ATTACK</button>
    <button class="btn btn-stop" onclick="stopAll()">■ STOP ALL</button>
  </div>
  <div class="hint">CH1-13 hops radio briefly. UI auto-reconnects.</div>
</div>

<!-- BEACON -->
<div class="card" id="tab-beacon">
  <div class="card-h"><span class="dot p"></span>Beacon Spam</div>
  <div class="info">Floods air with 20 fake SSIDs on rotating channels. Ghost networks in scanner lists.</div>
  <div class="ctrls">
    <button class="btn btn-pur" onclick="startBeacon()">START BEACON SPAM</button>
    <button class="btn btn-stop" onclick="stopAll()">■ STOP</button>
  </div>
</div>

<!-- APPLE JUICE -->
<div class="card" id="tab-juice">
  <div class="card-h"><span class="dot p"></span>Apple Juice (BLE Spam)</div>
  <div class="info">Spoofs AirPods / Beats / AppleTV BLE advertisements. Nearby iOS devices show popups.</div>
  <div class="ctrls">
    <button class="btn btn-green" onclick="startAppleJuice()">START APPLE JUICE</button>
    <button class="btn btn-stop" onclick="stopAppleJuice()">■ STOP JUICE</button>
  </div>
  <div class="hint">Uses BLE stack. Stops WiFi attacks while running.</div>
</div>

<!-- UDP -->
<div class="card" id="tab-udp">
  <div class="card-h"><span class="dot y"></span>UDP Flood</div>
  <div class="row2">
    <div class="field">
      <label>Target IP</label>
      <input type="text" id="udpIP" placeholder="192.168.1.1" maxlength="15">
    </div>
    <div class="field">
      <label>Target Port</label>
      <input type="number" id="udpPort" value="80" min="1" max="65535">
    </div>
  </div>
  <div class="info">Requires STA connection. Configure WiFi in CONFIG first.</div>
  <div class="ctrls">
    <button class="btn btn-yel" onclick="startUDP()">START UDP FLOOD</button>
    <button class="btn btn-stop" onclick="stopAll()">■ STOP</button>
  </div>
</div>

<!-- CONFIG -->
<div class="card" id="tab-config">
  <div class="card-h"><span class="dot"></span>WiFi &amp; AP Config</div>
  <div class="row2">
    <div class="field">
      <label>Router SSID</label>
      <input type="text" id="staSSID" placeholder="Home WiFi" maxlength="32">
    </div>
    <div class="field">
      <label>Router Password</label>
      <input type="password" id="staPass" placeholder="password" maxlength="63">
    </div>
  </div>
  <div class="ctrls">
    <button class="btn btn-blue" onclick="saveSTA()">SAVE &amp; CONNECT STA</button>
  </div>
  <div style="height:12px"></div>
  <div class="row2">
    <div class="field">
      <label>New AP SSID</label>
      <input type="text" id="apSSID" placeholder="KNHS HOTSPOT PRIVATE" maxlength="32">
    </div>
    <div class="field">
      <label>New AP Password (min 8)</label>
      <input type="password" id="apPass" placeholder="min 8 chars" maxlength="63">
    </div>
  </div>
  <div class="ctrls">
    <button class="btn btn-blue" onclick="saveAP()">APPLY &amp; REBROADCAST</button>
  </div>
  <div style="height:12px"></div>
  <div class="ota-block">
    <div style="font-size:9px;color:var(--muted);font-weight:700;letter-spacing:1.5px;text-transform:uppercase;margin-bottom:7px">Network Addresses</div>
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:8px">
      <div>
        <div style="font-size:8px;color:var(--muted);letter-spacing:1px;margin-bottom:3px">WEB UI (AP)</div>
        <div class="ota-ip" id="otaWebAddr">—</div>
      </div>
      <div>
        <div style="font-size:8px;color:#b45309;letter-spacing:1px;margin-bottom:3px">OTA FLASH</div>
        <div class="ota-ip" style="color:#f59e0b" id="otaFlashAddr">—</div>
      </div>
    </div>
    <div class="ota-sub">Arduino IDE: Sketch → Upload Method → OTA (port 3232)<br>
    Or: <code style="color:#60a5fa">python3 espota.py -i &lt;IP&gt; -p 3232 -f firmware.bin</code></div>
  </div>
  <div class="row2">
    <div class="field">
      <label>OTA Password (min 4)</label>
      <input type="password" id="otaPassInput" placeholder="arswebui" maxlength="32">
    </div>
    <div style="display:flex;align-items:flex-end;padding-bottom:11px">
      <button class="btn btn-grey" onclick="saveOTAPass()" style="width:100%">SAVE OTA PASS</button>
    </div>
  </div>
</div>

<!-- SYS -->
<div class="card" id="tab-sys">
  <div class="card-h"><span class="dot g"></span>System Status</div>
  <div id="sysInfo"><div class="loading">Loading...</div></div>
  <div style="height:10px"></div>
  <div class="card-h">
    <span class="dot g"></span>Event Log
    <button class="btn btn-grey" style="margin-left:auto;padding:3px 9px;font-size:9px" onclick="loadLog()">REFRESH</button>
  </div>
  <div class="log-box" id="logBox">Waiting for events...</div>
</div>

<div class="foot">ArsWebUI v3.0 | ESP32-S3 N16R8 | Marauder + Apple Juice</div>
</div>

<script>
'use strict';
var statusMon=null,curInten=1,mrOn=true,autoScanInt=null,autoClientInt=null;

function showTab(id){
  document.querySelectorAll('.card').forEach(function(c){c.classList.remove('show');});
  document.querySelectorAll('.nav-btn').forEach(function(b){b.classList.remove('active');});
  var el=document.getElementById('tab-'+id);
  if(el)el.classList.add('show');
  event.target.classList.add('active');
}

var intenCls=['','il','im','ih','ix'];
function setInten(v){
  curInten=v;
  document.querySelectorAll('.ib').forEach(function(b){
    var bv=parseInt(b.getAttribute('data-v'));
    b.className='ib'+(bv===v?' '+intenCls[v]:'');
  });
  fetch('/set?inten='+v).catch(function(){});
}
function setMacRand(v){
  mrOn=!!v;
  document.getElementById('mrOn').style.background=v?'#0f2d50':'#151b28';
  document.getElementById('mrOff').style.background=v?'#151b28':'#2d0808';
  fetch('/set?mac_rand='+v).catch(function(){});
}
setMacRand(1);

var badgeMap={
  idle:['IDLE','b-off'],deauth:['DEAUTH','b-on'],deauthall:['DEAUTH ALL','b-on'],
  deauthallch:['DEAUTH CH1-13','b-on'],udpflood:['UDP FLOOD','b-udp'],
  beacon:['BEACON SPAM','b-bcn'],csa:['CSA','b-csa'],applejuice:['APPLE JUICE','b-aj']
};
function setBadge(type){
  var m=badgeMap[type]||badgeMap.idle;
  var el=document.getElementById('targetStatus');
  el.className='badge '+m[1];el.textContent=m[0];
  document.getElementById('liveStatus').textContent=m[0];
  document.getElementById('liveStatus').style.color=type==='idle'?'#00d4aa':'#a78bfa';
}
function setType(t){document.getElementById('typeLabel').textContent=t||'';}

function toggleAutoScan(){
  var btn=document.getElementById('autoScanBtn');
  if(autoScanInt){clearInterval(autoScanInt);autoScanInt=null;btn.textContent='OFF';btn.className='btn-auto';}
  else{scanWiFi();autoScanInt=setInterval(scanWiFi,15000);btn.textContent='ON 15s';btn.className='btn-auto on';}
}
function toggleAutoClient(){
  var btn=document.getElementById('autoClientBtn');
  if(autoClientInt){clearInterval(autoClientInt);autoClientInt=null;btn.textContent='OFF';btn.className='btn-auto';}
  else{refreshClients();autoClientInt=setInterval(refreshClients,5000);btn.textContent='ON 5s';btn.className='btn-auto on';}
}

function scanWiFi(){
  document.getElementById('scanArea').innerHTML='<div class="loading">Scanning... (~5s)</div>';
  fetch('/scan').then(function(r){return r.text();}).then(function(d){
    document.getElementById('scanArea').innerHTML=
      '<table><thead><tr><th>#</th><th>SSID</th><th>BSSID</th><th>CH</th><th>RSSI</th><th>SEC</th><th>ACTION</th></tr></thead><tbody>'+d+'</tbody></table>';
  }).catch(function(){document.getElementById('scanArea').innerHTML='<div class="loading" style="color:#ef4444">Scan failed</div>';});
}
function scanClients(){document.getElementById('clientArea').style.display='block';refreshClients();}
function refreshClients(){
  fetch('/scan_clients').then(function(r){return r.text();}).then(function(d){
    document.getElementById('clientTable').innerHTML=
      '<table><thead><tr><th>MAC</th><th>RSSI</th><th>CH</th><th>LAST</th><th>ACTION</th></tr></thead><tbody>'+d+'</tbody></table>';
  }).catch(function(){document.getElementById('clientTable').innerHTML='<div class="loading" style="color:#ef4444">Failed</div>';});
}
function selTarget(bssid,ch,ssid){
  document.getElementById('bssidInput').value=bssid;
  document.getElementById('ssidInput').value=ssid;
  document.getElementById('channelSelect').value=ch;
  showTab('attack');
  document.querySelectorAll('.nav-btn')[1].classList.add('active');
}
function selCSA(bssid,ch,ssid){selTarget(bssid,ch,ssid);setTimeout(startCSA,200);}

function startDeauth(){
  var bssid=document.getElementById('bssidInput').value.trim().toUpperCase();
  var ch=document.getElementById('channelSelect').value;
  var ssid=document.getElementById('ssidInput').value.trim();
  var cli=document.getElementById('clientInput').value.trim().toUpperCase();
  if(!bssid){alert('Enter BSSID');return;}
  if(ch==='0'){alert('Select channel');return;}
  if(!confirm('Deauth '+ (ssid||bssid)+' CH'+ch+' ?'))return;
  fetch('/deauth?bssid='+bssid+'&ch='+ch+'&ssid='+encodeURIComponent(ssid)+'&client='+cli+'&inten='+curInten)
    .then(function(r){return r.text();}).then(function(d){
      if(d.indexOf('ERROR')>-1){alert(d);return;}
      setBadge('deauth');setType('→ '+(ssid||bssid));startStatusMon();
    }).catch(function(){alert('Request failed');});
}
function startDeauthAll(){
  if(!confirm('Deauth ALL nearby (AP stays up)?'))return;
  fetch('/deauth_all?inten='+curInten).then(function(r){return r.text();}).then(function(d){
    if(d.indexOf('ERROR')>-1){alert(d);return;}
    setBadge('deauthall');setType('→ all (AP up)');startStatusMon();
  }).catch(function(){alert('Failed');});
}
function startDeauthAllCh(){
  if(!confirm('Deauth CH1-13? AP hops briefly.'))return;
  fetch('/deauth_all_ch?inten='+curInten).then(function(r){return r.text();}).then(function(d){
    if(d.indexOf('ERROR')>-1){alert(d);return;}
    setBadge('deauthallch');setType('→ ch1-13');startStatusMon();startAutoReconnect();
  }).catch(function(){setBadge('deauthallch');setType('→ ch1-13');startAutoReconnect();});
}
var reconnectTimer=null;
function startAutoReconnect(){
  if(reconnectTimer)return;
  reconnectTimer=setInterval(function(){
    fetch('/attack_status',{signal:AbortSignal.timeout(1500)}).then(function(r){return r.text();}).then(function(d){
      var p=d.split(',');
      if(p[0]==='0'){stopAutoReconnect();setBadge('idle');setType('');stopStatusMon();}
      else{setBadge(p[1]||'deauthallch');document.getElementById('liveCount').textContent=p[2]||'0';}
    }).catch(function(){});
  },1800);
}
function stopAutoReconnect(){if(reconnectTimer){clearInterval(reconnectTimer);reconnectTimer=null;}}

function startCSA(){
  var bssid=document.getElementById('bssidInput').value.trim().toUpperCase();
  var ch=document.getElementById('channelSelect').value;
  var ssid=document.getElementById('ssidInput').value.trim();
  if(!bssid){alert('Enter BSSID');return;}
  if(ch==='0'){alert('Select channel');return;}
  if(!confirm('CSA on '+(ssid||bssid)+' CH'+ch+'?'))return;
  fetch('/csa?bssid='+bssid+'&ch='+ch+'&ssid='+encodeURIComponent(ssid))
    .then(function(r){return r.text();}).then(function(d){
      if(d.indexOf('ERROR')>-1){alert(d);return;}
      setBadge('csa');setType('→ '+(ssid||bssid));startStatusMon();
    }).catch(function(){alert('Failed');});
}
function startBeacon(){
  if(!confirm('Start beacon spam?'))return;
  fetch('/beacon_spam').then(function(r){return r.text();}).then(function(d){
    if(d.indexOf('ERROR')>-1){alert(d);return;}
    setBadge('beacon');setType('→ fake SSIDs');startStatusMon();
  }).catch(function(){alert('Failed');});
}
function stopAll(){
  fetch('/stop').then(function(r){return r.text();}).then(function(){
    setBadge('idle');setType('');stopStatusMon();
  });
}
function startUDP(){
  var ip=document.getElementById('udpIP').value.trim();
  var port=parseInt(document.getElementById('udpPort').value);
  if(!ip){alert('Enter IP');return;}
  if(port<1||port>65535){alert('Bad port');return;}
  if(!confirm('UDP flood → '+ip+':'+port+'?'))return;
  fetch('/udp_flood?ip='+ip+'&port='+port).then(function(r){return r.text();}).then(function(d){
    if(d.indexOf('ERROR')>-1){alert(d);return;}
    setBadge('udpflood');setType('→ '+ip+':'+port);startStatusMon();
  }).catch(function(){alert('Failed');});
}
function startAppleJuice(){
  if(!confirm('Start Apple Juice BLE spam?'))return;
  fetch('/applejuice').then(function(r){return r.text();}).then(function(d){
    if(d.indexOf('ERROR')>-1){alert(d);return;}
    setBadge('applejuice');setType('→ BLE spam');startStatusMon();
  }).catch(function(){alert('Failed');});
}
function stopAppleJuice(){
  fetch('/applejuice_stop').then(function(r){return r.text();}).then(function(){
    setBadge('idle');setType('');stopStatusMon();
  });
}
function saveSTA(){
  var ss=document.getElementById('staSSID').value.trim();
  var pp=document.getElementById('staPass').value;
  if(!ss){alert('Enter SSID');return;}
  fetch('/savesta?ssid='+encodeURIComponent(ss)+'&pass='+encodeURIComponent(pp))
    .then(function(r){return r.text();}).then(function(d){alert(d);});
}
function saveAP(){
  var ss=document.getElementById('apSSID').value.trim();
  var pp=document.getElementById('apPass').value;
  if(!ss){alert('Enter SSID');return;}
  if(pp.length<8){alert('Pass min 8');return;}
  if(!confirm('Rename AP to "'+ss+'"?'))return;
  fetch('/setap?ssid='+encodeURIComponent(ss)+'&pass='+encodeURIComponent(pp))
    .then(function(r){return r.text();}).then(function(d){alert(d);})
    .catch(function(){alert('Sent — reconnect');});
}
function saveOTAPass(){
  var p=document.getElementById('otaPassInput').value;
  if(p.length<4){alert('Min 4 chars');return;}
  fetch('/setotapass?pass='+encodeURIComponent(p)).then(function(r){return r.text();}).then(function(d){alert(d);});
}
function loadOtaInfo(){
  fetch('/ota_info').then(function(r){return r.text();}).then(function(d){
    var parts=d.trim().split(',');
    document.getElementById('liveWebIP').textContent=parts[0]||'';
    document.getElementById('liveOtaIP').textContent=parts[1]||'';
    document.getElementById('otaWebAddr').textContent=parts[0]||'';
    document.getElementById('otaFlashAddr').textContent=parts[1]||'';
  }).catch(function(){});
}
function startStatusMon(){if(statusMon)return;statusMon=setInterval(pollStatus,2500);}
function stopStatusMon(){if(statusMon){clearInterval(statusMon);statusMon=null;}}
function pollStatus(){
  fetch('/attack_status').then(function(r){return r.text();}).then(function(d){
    var p=d.split(',');
    var running=p[0]==='1';
    var type=p[1]||'idle';
    document.getElementById('liveCount').textContent=p[2]||'0';
    if(!running){setBadge('idle');setType('');stopStatusMon();}
    else setBadge(type);
  }).catch(function(){});
}
function pollPkts(){
  fetch('/pkt_count').then(function(r){return r.text();}).then(function(d){
    document.getElementById('liveCount').textContent=d.trim();
  }).catch(function(){});
}
function loadSysInfo(){
  fetch('/sysinfo').then(function(r){return r.text();}).then(function(d){
    document.getElementById('sysInfo').innerHTML=d;
    var vs=document.querySelectorAll('#sysInfo .stat-v');
    if(vs.length>=2)document.getElementById('liveHeap').textContent=vs[1].textContent;
  }).catch(function(){});
}
function loadLog(){
  fetch('/log').then(function(r){return r.text();}).then(function(d){
    var box=document.getElementById('logBox');
    box.textContent=d||'(empty)';box.scrollTop=box.scrollHeight;
  }).catch(function(){});
}
loadSysInfo();loadLog();loadOtaInfo();
setInterval(loadSysInfo,6000);setInterval(pollPkts,2000);
setInterval(loadLog,12000);setInterval(loadOtaInfo,10000);
</script>
</body>
</html>
)rawliteral";
