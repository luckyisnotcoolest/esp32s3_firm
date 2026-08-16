#pragma once
#include <Arduino.h>

// ArsWebUI v2.9 — Dark UI
// Endpoints: /scan /scan_clients /deauth /deauth_all /deauth_all_ch /csa
//   /beacon_spam /stop /stopdeauth /udp_flood /savesta /setap /pkt_count
//   /attack_status /log /sysinfo /set /ota_info /setotapass

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>ArsWebUI</title>
<style>
:root{
  --bg:#0a0c10;--card:#111318;--border:#1e2330;
  --accent:#3b82f6;--red:#ef4444;--yellow:#f59e0b;
  --green:#22c55e;--purple:#a855f7;
  --text:#dde3f0;--muted:#4b5675;
}
*{margin:0;padding:0;box-sizing:border-box}
body{background:var(--bg);color:var(--text);
     font-family:'Segoe UI',system-ui,sans-serif;min-height:100vh}
.wrap{max-width:900px;margin:0 auto;padding:12px}

/* ── Hardware recommendations ── */
.hw-rec{
  background:linear-gradient(135deg,#050f05,#0a1a0a);
  border:1px solid #22c55e;border-radius:10px;
  padding:12px 14px;margin-bottom:10px;font-size:11px;
  color:#86efac;line-height:1.8}
.hw-rec strong{color:#4ade80;font-size:12px;display:block;
               margin-bottom:6px;letter-spacing:1px}
.hw-rec .hw-dismiss{
  float:right;background:none;border:1px solid #166534;
  border-radius:5px;color:#4b5675;cursor:pointer;
  font-size:11px;padding:1px 8px;transition:all .15s}
.hw-rec .hw-dismiss:hover{border-color:#22c55e;color:#4ade80}
.hw-rec ul{padding-left:14px;margin:0}
.hw-rec li{margin-bottom:2px}
.hw-rec .hw-tag{
  display:inline-block;background:#052e1a;border:1px solid #166534;
  border-radius:4px;padding:0 6px;font-size:9px;
  color:#4ade80;font-weight:700;margin-left:4px;vertical-align:middle}

/* ── Antenna banner (legacy, kept for compatibility) ── */
.ant-banner{display:none}

/* ── Header ── */
.hdr{background:linear-gradient(135deg,#080b14,#141033);
     border:1px solid #1e1b4b;border-radius:14px;
     padding:18px 18px 14px;margin-bottom:10px;text-align:center}
.hdr h1{font-size:21px;font-weight:900;letter-spacing:3px;
        background:linear-gradient(90deg,#60a5fa,#c084fc);
        -webkit-background-clip:text;-webkit-text-fill-color:transparent}
.hdr-sub{color:var(--muted);font-size:10px;letter-spacing:2px;margin-top:3px}
.live-row{display:flex;justify-content:center;gap:22px;
          margin-top:9px;font-size:11px;font-weight:700;letter-spacing:1px;
          flex-wrap:wrap}
.live-row span{color:var(--muted)}
.live-row b{color:#60a5fa}
#liveStatus{color:#a855f7}
.net-row{display:flex;justify-content:center;gap:16px;
         margin-top:5px;font-size:10px;letter-spacing:.8px;
         flex-wrap:wrap}
.net-row span{color:var(--muted)}
.net-row b{color:#34d399}
.net-row .ota-b{color:#f59e0b}

/* ── Cards ── */
.card{background:var(--card);border:1px solid var(--border);
      border-radius:11px;padding:16px;margin-bottom:10px}
.card-h{font-size:10px;font-weight:800;letter-spacing:2px;
        text-transform:uppercase;color:var(--muted);
        border-bottom:1px solid var(--border);
        padding-bottom:10px;margin-bottom:14px;
        display:flex;align-items:center;gap:8px}
.dot{width:6px;height:6px;border-radius:50%;background:var(--accent);flex-shrink:0}
.dot.r{background:var(--red)}.dot.y{background:var(--yellow)}
.dot.p{background:var(--purple)}.dot.g{background:var(--green)}
.dot.o{background:#f59e0b}

/* ── Buttons ── */
.btn{padding:8px 15px;border:none;border-radius:7px;cursor:pointer;
     font-size:11px;font-weight:700;letter-spacing:.4px;
     font-family:inherit;transition:all .13s}
.btn:active{transform:scale(.96)}
.btn-blue{background:#1d4ed8;color:#fff}
.btn-blue:hover{background:#1e40af}
.btn-red{background:#b91c1c;color:#fff}
.btn-red:hover{background:#991b1b}
.btn-yel{background:#b45309;color:#fff}
.btn-yel:hover{background:#92400e}
.btn-pur{background:#7c3aed;color:#fff}
.btn-pur:hover{background:#6d28d9}
.btn-stop{background:#2d0808;color:#fca5a5;border:1px solid #7f1d1d}
.btn-stop:hover{background:#7f1d1d}
.btn-grey{background:#151b28;color:#94a3b8;border:1px solid var(--border)}
.btn-grey:hover{background:#1e2742}
.btn-sel{padding:4px 9px;background:#1d4ed8;color:#fff;border:none;
         border-radius:5px;cursor:pointer;font-size:10px;font-weight:700}
.btn-sel:hover{background:#1e40af}
.btn-csa{padding:4px 9px;background:#6d28d9;color:#fff;border:none;
         border-radius:5px;cursor:pointer;font-size:10px;font-weight:700}
.btn-csa:hover{background:#5b21b6}
.btn-auto{padding:4px 9px;border:1px solid var(--border);background:#0d1321;
          color:var(--muted);border-radius:5px;cursor:pointer;
          font-size:10px;font-weight:700;transition:all .15s}
.btn-auto.on{background:#0a2010;border-color:#22c55e;color:#4ade80}

/* ── Badge ── */
.badge{display:inline-block;padding:3px 11px;border-radius:20px;
       font-size:10px;font-weight:800;letter-spacing:.8px}
.b-off{background:#111620;color:#4b5675}
.b-on{background:#3b0808;color:#fca5a5;animation:pulse 1.4s infinite}
.b-udp{background:#3b2000;color:#fbbf24}
.b-bcn{background:#1e0a40;color:#c4b5fd}
.b-csa{background:#002a28;color:#6ee7b7}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.55}}

/* ── Tables ── */
table{width:100%;border-collapse:collapse;font-size:11px}
th{padding:7px 5px;text-align:left;color:var(--muted);font-size:9px;
   letter-spacing:1px;border-bottom:1px solid var(--border);font-weight:700;
   text-transform:uppercase}
td{padding:7px 5px;border-bottom:1px solid #0b0e14;vertical-align:middle}
tr:hover td{background:#141925}

/* ── Forms ── */
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
  box-shadow:0 0 0 2px rgba(59,130,246,.12)}
.field input::placeholder{color:#2a3348}
.row2{display:grid;grid-template-columns:1fr 1fr;gap:10px}

/* ── Intensity ── */
.inten-row{display:grid;grid-template-columns:repeat(4,1fr);gap:5px;margin-bottom:11px}
.ib{padding:7px 3px;border:1px solid var(--border);border-radius:6px;
    background:#07090e;color:var(--muted);cursor:pointer;
    font-size:9px;font-weight:700;letter-spacing:.4px;text-align:center;
    transition:all .13s;line-height:1.6}
.ib:hover{border-color:var(--accent);color:#60a5fa}
.il{background:#081a30;border-color:#3b82f6;color:#60a5fa}
.im{background:#1c1400;border-color:#f59e0b;color:#fbbf24}
.ih{background:#1f0b00;border-color:#ef4444;color:#fca5a5}
.ix{background:#140028;border-color:#a855f7;color:#d8b4fe}

/* ── Stats ── */
.stats{display:grid;grid-template-columns:repeat(3,1fr);gap:7px}
.stat{background:#07090e;border:1px solid var(--border);
      border-radius:8px;padding:11px}
.stat-l{color:var(--muted);font-size:8px;letter-spacing:1.5px;
        text-transform:uppercase;font-weight:700;margin-bottom:4px}
.stat-v{color:#60a5fa;font-weight:800;font-size:14px;word-break:break-all}

/* ── OTA block ── */
.ota-block{background:#070b10;border:1px solid #1a2a18;
           border-radius:8px;padding:12px;margin-bottom:10px}
.ota-block .ota-ip{font-family:monospace;font-size:13px;
                   color:#34d399;font-weight:800;letter-spacing:.5px}
.ota-block .ota-sub{font-size:10px;color:var(--muted);margin-top:4px;line-height:1.7}

/* ── Misc ── */
.ctrls{display:flex;flex-wrap:wrap;gap:6px;margin-top:10px}
.info{background:#0a1628;border-left:3px solid var(--accent);
      padding:9px 12px;border-radius:5px;font-size:11px;
      color:#93c5fd;margin:7px 0;line-height:1.6}
.tgt{background:#07090e;border:1px solid var(--border);
     border-left:3px solid var(--red);border-radius:7px;
     padding:10px 13px;margin:9px 0;font-size:11px;
     display:flex;align-items:center;gap:11px;flex-wrap:wrap}
.bar-bg{background:#07090e;border:1px solid var(--border);
        border-radius:5px;height:20px;margin-top:7px;overflow:hidden}
.bar-fill{height:100%;background:linear-gradient(90deg,#1d4ed8,#7c3aed);
          width:0%;transition:width .4s;display:flex;align-items:center;
          justify-content:center;font-size:10px;font-weight:700;color:#fff}
.log-box{background:#050708;border:1px solid var(--border);
         border-radius:6px;padding:9px 11px;font-family:monospace;
         font-size:10px;color:#4ade80;max-height:150px;overflow-y:auto;
         white-space:pre-wrap;line-height:1.75;margin-top:7px}
.loading{text-align:center;padding:20px;color:var(--muted);font-size:12px}
.hint{font-size:9px;color:var(--muted);margin-top:-7px;margin-bottom:10px}
.foot{text-align:center;padding:12px;color:#1e2742;font-size:9px;
      letter-spacing:2px;margin-top:6px}
.scan-hdr{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.scan-auto-lbl{font-size:9px;color:var(--muted);font-weight:700;
               letter-spacing:1px;text-transform:uppercase}
@media(max-width:560px){
  .row2,.inten-row{grid-template-columns:1fr 1fr}
  .stats{grid-template-columns:1fr 1fr}
}
</style>
</head>
<body>
<div class="wrap">

<!-- ══ HARDWARE RECOMMENDATIONS ══════════════════════════════════════════════ -->
<div class="hw-rec" id="hwRec">
  <button class="hw-dismiss" onclick="document.getElementById('hwRec').style.display='none'">✕</button>
  <strong>🛠 Hardware Setup — Bare ESP32-S3 N16R8 Range Optimization</strong>
  <ul>
    <li><b>External Antenna</b> — Connect U.FL/IPEX to SMA adapter on the ESP32-S3 antenna pad.
        Attach a <b>5 dBi 2.4GHz omni</b> (budget) or <b>9 dBi panel</b> (directional).
        Recommended: <span style="color:#4ade80">TP-Link TL-ANT2409CL</span> or any 2.4GHz SMA antenna.
        <span class="hw-tag">+6dBm effective</span></li>
    <li><b>Power Supply</b> — Use a quality 5V ≥1A adapter. At MAX TX (19.5 dBm) the radio
        draws ~350mA. A weak charger causes brownouts → resets. USB power bank works fine.</li>
    <li><b>Placement</b> — Elevation = range. Every 1m above obstacles adds ~3dBm effective.
        Avoid metal enclosures — they kill range by 10-20dB. Use plastic or acrylic case.</li>
    <li><b>Heat</b> — At MAX intensity for extended periods the S3 runs warm (~55°C).
        Add a small heatsink to the module if running MAX for >10 minutes.</li>
    <li><b>Channel</b> — Your AP runs on CH6. For best deauth effectiveness on a target,
        use <b>DEAUTH CH1-13</b> mode which physically hops channels. For a single target
        on CH6, standard deauth is most powerful (no AP interruption).</li>
    <li><b>Antenna connector</b> — Most ESP32-S3 N16R8 boards have a U.FL (IPEX1) connector
        near the module edge. Some have it populated, some need the ceramic antenna trace cut
        first. Check your board silkscreen for "ANT" or "RF".</li>
  </ul>
</div>

<!-- ══ HEADER ════════════════════════════════════════════════════════════════ -->
<div class="hdr">
  <h1>ArsWebUI</h1>
  <div class="hdr-sub">ESP32-S3 N16R8 &nbsp;▸&nbsp; 19.5 dBm &nbsp;▸&nbsp; v2.9</div>
  <div class="live-row">
    <span>PKTS&nbsp;<b id="liveCount">0</b></span>
    <span>STATUS&nbsp;<b id="liveStatus">IDLE</b></span>
    <span>HEAP&nbsp;<b id="liveHeap">—</b></span>
  </div>
  <div class="net-row">
    <span>WEB&nbsp;<b id="liveWebIP">—</b></span>
    <span>OTA&nbsp;<b class="ota-b" id="liveOtaIP">—</b></span>
  </div>
</div>

<!-- ══ SCANNER ═══════════════════════════════════════════════════════════════ -->
<div class="card">
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
      <span style="font-size:9px;color:var(--muted);font-weight:700;
                   letter-spacing:1px;text-transform:uppercase">PASSIVE AUTO-REFRESH</span>
      <button class="btn-auto" id="autoClientBtn" onclick="toggleAutoClient()">OFF</button>
    </div>
    <div id="clientTable"><div class="loading">...</div></div>
  </div>
</div>

<!-- ══ DEAUTH / CSA ══════════════════════════════════════════════════════════ -->
<div class="card">
  <div class="card-h"><span class="dot r"></span>Deauth &amp; CSA Attack</div>

  <div class="row2">
    <div class="field">
      <label>Target BSSID</label>
      <input type="text" id="bssidInput" placeholder="AA:BB:CC:DD:EE:FF"
             maxlength="17" style="text-transform:uppercase;font-family:monospace">
    </div>
    <div class="field">
      <label>SSID (display)</label>
      <input type="text" id="ssidInput" placeholder="auto-filled" readonly
             style="color:#4b5675">
    </div>
  </div>
  <div class="row2">
    <div class="field">
      <label>Channel</label>
      <select id="channelSelect">
        <option value="0">— select —</option>
        <option value="1">1 (2412)</option><option value="2">2 (2417)</option>
        <option value="3">3 (2422)</option><option value="4">4 (2427)</option>
        <option value="5">5 (2432)</option><option value="6">6 (2437)</option>
        <option value="7">7 (2442)</option><option value="8">8 (2447)</option>
        <option value="9">9 (2452)</option><option value="10">10 (2457)</option>
        <option value="11">11 (2462)</option><option value="12">12 (2467)</option>
        <option value="13">13 (2472)</option>
      </select>
    </div>
    <div class="field">
      <label>Client MAC (optional)</label>
      <input type="text" id="clientInput" placeholder="leave empty = broadcast"
             maxlength="17" style="text-transform:uppercase;font-family:monospace">
    </div>
  </div>

  <div class="field">
    <label>Attack Intensity</label>
    <div class="inten-row">
      <div class="ib il" data-v="1" onclick="setInten(1)">
        LOW<br><span style="font-size:8px;opacity:.6">8 frames</span>
      </div>
      <div class="ib" data-v="2" onclick="setInten(2)">
        MED<br><span style="font-size:8px;opacity:.6">20 frames</span>
      </div>
      <div class="ib" data-v="3" onclick="setInten(3)">
        HIGH<br><span style="font-size:8px;opacity:.6">40 frames</span>
      </div>
      <div class="ib" data-v="4" onclick="setInten(4)">
        MAX<br><span style="font-size:8px;opacity:.6">80 frames</span>
      </div>
    </div>
  </div>

  <div class="row2" style="margin-bottom:10px">
    <div>
      <label style="font-size:9px;color:var(--muted);letter-spacing:1px;
                    display:block;margin-bottom:5px">MAC RANDOMIZE</label>
      <div style="display:flex;gap:6px">
        <button class="btn btn-grey" id="mrOn"  onclick="setMacRand(1)"
                style="flex:1;font-size:10px">ON</button>
        <button class="btn btn-grey" id="mrOff" onclick="setMacRand(0)"
                style="flex:1;font-size:10px">OFF</button>
      </div>
    </div>
    <div>
      <label style="font-size:9px;color:var(--muted);letter-spacing:1px;
                    display:block;margin-bottom:8px">ATTACK STATUS</label>
      <span id="targetStatus" class="badge b-off">IDLE</span>
      <div id="typeLabel" style="font-size:10px;color:var(--muted);margin-top:5px"></div>
    </div>
  </div>

  <div class="ctrls">
    <button class="btn btn-red"  onclick="startDeauth()">DEAUTH TARGET</button>
    <button class="btn btn-red"  onclick="startDeauthAll()">DEAUTH ALL AP-CH</button>
    <button class="btn btn-red"  onclick="startDeauthAllCh()"
            style="background:#7f1d1d;border:1px solid #ef4444">
      DEAUTH CH1-13 ⚡
    </button>
    <button class="btn btn-pur"  onclick="startCSA()">CSA ATTACK</button>
    <button class="btn btn-stop" onclick="stopAll()">■ STOP ALL</button>
  </div>
  <div class="hint">
    ⚡ CH1-13 briefly stops control AP per channel (~300ms). UI auto-reconnects.
  </div>
</div>

<!-- ══ BEACON SPAM ════════════════════════════════════════════════════════════ -->
<div class="card">
  <div class="card-h"><span class="dot p"></span>Beacon Spam</div>
  <div class="info">
    Floods the air with 20 fake SSIDs on rotating channels.
    Saturates nearby device scanner lists with ghost networks.
  </div>
  <div class="ctrls">
    <button class="btn btn-pur"  onclick="startBeacon()">START BEACON SPAM</button>
    <button class="btn btn-stop" onclick="stopAll()">■ STOP</button>
  </div>
</div>

<!-- ══ UDP FLOOD ══════════════════════════════════════════════════════════════ -->
<div class="card">
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
  <div class="info">Requires STA connection. Configure WiFi below first.</div>
  <div class="ctrls">
    <button class="btn btn-yel"  onclick="startUDP()">START UDP FLOOD</button>
    <button class="btn btn-stop" onclick="stopAll()">■ STOP</button>
  </div>
</div>

<!-- ══ WIFI STA CONFIG ════════════════════════════════════════════════════════ -->
<div class="card">
  <div class="card-h"><span class="dot"></span>WiFi Config (STA)</div>
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
    <button class="btn btn-blue" onclick="saveSTA()">SAVE &amp; CONNECT</button>
  </div>
  <div class="hint" style="margin-top:8px">Required for UDP flood. OTA also works on STA IP when connected.</div>
</div>

<!-- ══ AP CONFIG ══════════════════════════════════════════════════════════════ -->
<div class="card">
  <div class="card-h"><span class="dot"></span>AP Config</div>
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
  <div class="hint" style="margin-top:7px">Reconnect with new credentials after apply.</div>
</div>

<!-- ══ OTA / NETWORK ══════════════════════════════════════════════════════════ -->
<div class="card">
  <div class="card-h"><span class="dot o"></span>OTA Update &amp; Network</div>

  <div class="ota-block">
    <div style="font-size:9px;color:var(--muted);font-weight:700;
                letter-spacing:1.5px;text-transform:uppercase;margin-bottom:7px">
      Network Addresses
    </div>
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:8px">
      <div>
        <div style="font-size:8px;color:var(--muted);letter-spacing:1px;margin-bottom:3px">
          WEB UI (AP)
        </div>
        <div class="ota-ip" id="otaWebAddr">—</div>
      </div>
      <div>
        <div style="font-size:8px;color:#b45309;letter-spacing:1px;margin-bottom:3px">
          OTA FLASH PORT
        </div>
        <div class="ota-ip" style="color:#f59e0b" id="otaFlashAddr">—</div>
      </div>
    </div>
    <div class="ota-sub">
      Flash via Arduino IDE: <b>Sketch → Upload Method → OTA</b> (set IP above, port 3232).<br>
      Or: <code style="color:#60a5fa">python3 espota.py -i &lt;IP&gt; -p 3232 -f firmware.bin</code>
    </div>
  </div>

  <div class="row2">
    <div class="field">
      <label>OTA Password (min 4)</label>
      <input type="password" id="otaPassInput" placeholder="arswebui" maxlength="32">
    </div>
    <div style="display:flex;align-items:flex-end;padding-bottom:11px">
      <button class="btn btn-grey" onclick="saveOTAPass()" style="width:100%">
        SAVE OTA PASS
      </button>
    </div>
  </div>
  <div class="hint">Default OTA password: <b>arswebui</b> — change here. Reboot to apply.</div>
</div>

<!-- ══ SYSTEM STATUS ══════════════════════════════════════════════════════════ -->
<div class="card">
  <div class="card-h"><span class="dot g"></span>System Status</div>
  <div id="sysInfo"><div class="loading">Loading...</div></div>
</div>

<!-- ══ EVENT LOG ══════════════════════════════════════════════════════════════ -->
<div class="card">
  <div class="card-h">
    <span class="dot g"></span>Event Log
    <button class="btn btn-grey"
            style="margin-left:auto;padding:3px 9px;font-size:9px"
            onclick="loadLog()">REFRESH</button>
  </div>
  <div class="log-box" id="logBox">Waiting for events...</div>
</div>

<div class="foot">ArsWebUI v2.9 &nbsp;|&nbsp; ESP32-S3 N16R8 &nbsp;|&nbsp; AsyncTCP + ArduinoOTA</div>
</div>

<script>
'use strict';

var statusMon    = null;
var curInten     = 1;
var mrOn         = true;
var autoScanInt  = null;
var autoClientInt = null;

// ── Banner dismiss ────────────────────────────────────────────────────────────
function dismissBanner() {
  document.getElementById('antBanner').style.display = 'none';
}

// ── Intensity ─────────────────────────────────────────────────────────────────
var intenCls = ['','il','im','ih','ix'];
function setInten(v) {
  curInten = v;
  document.querySelectorAll('.ib').forEach(function(b){
    var bv = parseInt(b.getAttribute('data-v'));
    b.className = 'ib' + (bv === v ? ' ' + intenCls[v] : '');
  });
  fetch('/set?inten=' + v).catch(function(){});
}

function setMacRand(v) {
  mrOn = !!v;
  document.getElementById('mrOn').style.background  = v ? '#0f2d50' : '#151b28';
  document.getElementById('mrOff').style.background = v ? '#151b28' : '#2d0808';
  fetch('/set?mac_rand=' + v).catch(function(){});
}
setMacRand(1);

// ── Badge helpers ─────────────────────────────────────────────────────────────
var badgeMap = {
  idle:        ['IDLE',          'b-off'],
  deauth:      ['DEAUTH ACTIVE', 'b-on'],
  deauthall:   ['DEAUTH ALL',    'b-on'],
  deauthallch: ['DEAUTH CH1-13', 'b-on'],
  udpflood:    ['UDP FLOOD',     'b-udp'],
  beacon:      ['BEACON SPAM',   'b-bcn'],
  csa:         ['CSA ATTACK',    'b-csa']
};

function setBadge(type) {
  var m  = badgeMap[type] || badgeMap.idle;
  var el = document.getElementById('targetStatus');
  el.className   = 'badge ' + m[1];
  el.textContent = m[0];
  document.getElementById('liveStatus').textContent = m[0];
  document.getElementById('liveStatus').style.color =
    type === 'idle' ? '#60a5fa' : '#a855f7';
}

function setType(t) { document.getElementById('typeLabel').textContent = t || ''; }

// ── Auto-refresh: WiFi scan ───────────────────────────────────────────────────
function toggleAutoScan() {
  var btn = document.getElementById('autoScanBtn');
  if (autoScanInt) {
    clearInterval(autoScanInt);
    autoScanInt = null;
    btn.textContent = 'OFF';
    btn.className   = 'btn-auto';
  } else {
    scanWiFi();
    autoScanInt = setInterval(scanWiFi, 15000);
    btn.textContent = 'ON 15s';
    btn.className   = 'btn-auto on';
  }
}

// ── Auto-refresh: passive clients ─────────────────────────────────────────────
function toggleAutoClient() {
  var btn = document.getElementById('autoClientBtn');
  if (autoClientInt) {
    clearInterval(autoClientInt);
    autoClientInt = null;
    btn.textContent = 'OFF';
    btn.className   = 'btn-auto';
  } else {
    refreshClients();
    autoClientInt = setInterval(refreshClients, 5000);
    btn.textContent = 'ON 5s';
    btn.className   = 'btn-auto on';
  }
}

// ── Scan ──────────────────────────────────────────────────────────────────────
function scanWiFi() {
  document.getElementById('scanArea').innerHTML =
    '<div class="loading">Scanning... (~5s)</div>';
  fetch('/scan').then(function(r){return r.text();}).then(function(d){
    document.getElementById('scanArea').innerHTML =
      '<table><thead><tr><th>#</th><th>SSID</th><th>BSSID</th>' +
      '<th>CH</th><th>RSSI</th><th>SEC</th><th>ACTION</th></tr></thead>' +
      '<tbody>' + d + '</tbody></table>';
  }).catch(function(){
    document.getElementById('scanArea').innerHTML =
      '<div class="loading" style="color:#ef4444">Scan failed</div>';
  });
}

function scanClients() {
  var a = document.getElementById('clientArea');
  a.style.display = 'block';
  refreshClients();
}

function refreshClients() {
  fetch('/scan_clients').then(function(r){return r.text();}).then(function(d){
    document.getElementById('clientTable').innerHTML =
      '<table><thead><tr><th>MAC</th><th>RSSI</th><th>CH</th>' +
      '<th>LAST SEEN</th><th>ACTION</th></tr></thead>' +
      '<tbody>' + d + '</tbody></table>';
  }).catch(function(){
    document.getElementById('clientTable').innerHTML =
      '<div class="loading" style="color:#ef4444">Failed</div>';
  });
}

function selTarget(bssid, ch, ssid) {
  document.getElementById('bssidInput').value       = bssid;
  document.getElementById('ssidInput').value        = ssid;
  document.getElementById('channelSelect').value    = ch;
  document.getElementById('bssidInput').scrollIntoView({behavior:'smooth',block:'center'});
}

function selCSA(bssid, ch, ssid) {
  selTarget(bssid, ch, ssid);
  setTimeout(startCSA, 200);
}

// ── Deauth ────────────────────────────────────────────────────────────────────
function startDeauth() {
  var bssid = document.getElementById('bssidInput').value.trim().toUpperCase();
  var ch    = document.getElementById('channelSelect').value;
  var ssid  = document.getElementById('ssidInput').value.trim();
  var cli   = document.getElementById('clientInput').value.trim().toUpperCase();

  if (!bssid)     { alert('Enter BSSID first'); return; }
  if (ch === '0') { alert('Select a channel'); return; }
  if (!confirm('Deauth: ' + (ssid||bssid) + ' [' + bssid + '] CH' + ch +
               '\nClient: ' + (cli||'broadcast (all clients)') + '\n\nContinue?')) return;

  fetch('/deauth?bssid=' + bssid + '&ch=' + ch +
        '&ssid=' + encodeURIComponent(ssid) +
        '&client=' + cli + '&inten=' + curInten)
    .then(function(r){return r.text();}).then(function(d){
      if (d.indexOf('ERROR') > -1) { alert(d); return; }
      setBadge('deauth'); setType('→ ' + (ssid||bssid));
      startStatusMon();
    }).catch(function(){ alert('Request failed'); });
}

function startDeauthAll() {
  if (!confirm('Deauth ALL nearby networks — AP stays up, best-effort on non-AP channels.\n\nContinue?')) return;
  fetch('/deauth_all?inten=' + curInten)
    .then(function(r){return r.text();}).then(function(d){
      if (d.indexOf('ERROR') > -1) { alert(d); return; }
      setBadge('deauthall'); setType('→ all networks (AP up)');
      startStatusMon();
    }).catch(function(){ alert('Request failed'); });
}

function startDeauthAllCh() {
  if (!confirm('Deauth ALL networks CH 1-13?\n\n⚠ Control AP stops briefly per channel hop (~300ms each).\nThe UI auto-reconnects automatically.\n\nContinue?')) return;
  fetch('/deauth_all_ch?inten=' + curInten)
    .then(function(r){return r.text();}).then(function(d){
      if (d.indexOf('ERROR') > -1) { alert(d); return; }
      setBadge('deauthallch'); setType('→ ch1-13 PSRAM sweep');
      startStatusMon();
      startAutoReconnect();
    }).catch(function(){
      // AP may be mid-hop — just start reconnect
      setBadge('deauthallch'); setType('→ ch1-13 PSRAM sweep');
      startAutoReconnect();
    });
}

// ── Auto-reconnect for CH1-13 mode (AP flickers per channel hop) ─────────────
var reconnectTimer = null;
function startAutoReconnect() {
  if (reconnectTimer) return;
  reconnectTimer = setInterval(function() {
    fetch('/attack_status', {signal: AbortSignal.timeout(1500)})
      .then(function(r){ return r.text(); })
      .then(function(d){
        var p = d.split(',');
        if (p[0] === '0') {
          stopAutoReconnect();
          setBadge('idle'); setType(''); stopStatusMon();
        } else {
          setBadge(p[1] || 'deauthallch');
          document.getElementById('liveCount').textContent = p[2] || '0';
        }
      })
      .catch(function() { /* AP mid-hop, retry next tick */ });
  }, 1800);
}
function stopAutoReconnect() {
  if (reconnectTimer) { clearInterval(reconnectTimer); reconnectTimer = null; }
}

function startCSA() {
  var bssid = document.getElementById('bssidInput').value.trim().toUpperCase();
  var ch    = document.getElementById('channelSelect').value;
  var ssid  = document.getElementById('ssidInput').value.trim();
  if (!bssid)     { alert('Enter BSSID first'); return; }
  if (ch === '0') { alert('Select a channel'); return; }
  if (!confirm('CSA attack on ' + (ssid||bssid) + ' CH' + ch +
               '\nForces clients to switch channels.\n\nContinue?')) return;

  fetch('/csa?bssid=' + bssid + '&ch=' + ch +
        '&ssid=' + encodeURIComponent(ssid))
    .then(function(r){return r.text();}).then(function(d){
      if (d.indexOf('ERROR') > -1) { alert(d); return; }
      setBadge('csa'); setType('→ ' + (ssid||bssid));
      startStatusMon();
    }).catch(function(){ alert('Request failed'); });
}

// ── Beacon spam ───────────────────────────────────────────────────────────────
function startBeacon() {
  if (!confirm('Start beacon spam?\nFloods nearby devices with 20 fake SSIDs.\n\nContinue?')) return;
  fetch('/beacon_spam')
    .then(function(r){return r.text();}).then(function(d){
      if (d.indexOf('ERROR') > -1) { alert(d); return; }
      setBadge('beacon'); setType('→ fake SSIDs');
      startStatusMon();
    }).catch(function(){ alert('Request failed'); });
}

// ── Stop ──────────────────────────────────────────────────────────────────────
function stopAll() {
  fetch('/stop').then(function(r){return r.text();}).then(function(){
    setBadge('idle'); setType('');
    stopStatusMon();
  });
}

// ── UDP ───────────────────────────────────────────────────────────────────────
function startUDP() {
  var ip   = document.getElementById('udpIP').value.trim();
  var port = parseInt(document.getElementById('udpPort').value);
  if (!ip)                       { alert('Enter target IP'); return; }
  if (port < 1 || port > 65535) { alert('Invalid port (1-65535)'); return; }
  if (!confirm('UDP flood → ' + ip + ':' + port + '\n\nContinue?')) return;

  fetch('/udp_flood?ip=' + ip + '&port=' + port)
    .then(function(r){return r.text();}).then(function(d){
      if (d.indexOf('ERROR') > -1) { alert(d); return; }
      setBadge('udpflood'); setType('→ ' + ip + ':' + port);
      startStatusMon();
    }).catch(function(){ alert('Request failed'); });
}

// ── STA config ────────────────────────────────────────────────────────────────
function saveSTA() {
  var ss = document.getElementById('staSSID').value.trim();
  var pp = document.getElementById('staPass').value;
  if (!ss) { alert('Enter SSID'); return; }
  fetch('/savesta?ssid=' + encodeURIComponent(ss) +
        '&pass=' + encodeURIComponent(pp))
    .then(function(r){return r.text();}).then(function(d){ alert(d); });
}

// ── AP config ─────────────────────────────────────────────────────────────────
function saveAP() {
  var ss = document.getElementById('apSSID').value.trim();
  var pp = document.getElementById('apPass').value;
  if (!ss)           { alert('Enter new SSID'); return; }
  if (pp.length < 8) { alert('Password must be at least 8 characters'); return; }
  if (!confirm('Rename AP to "' + ss + '"?\nYou will need to reconnect.')) return;
  fetch('/setap?ssid=' + encodeURIComponent(ss) +
        '&pass=' + encodeURIComponent(pp))
    .then(function(r){return r.text();}).then(function(d){ alert(d); })
    .catch(function(){ alert('Request sent — reconnect to new SSID'); });
}

// ── OTA password ──────────────────────────────────────────────────────────────
function saveOTAPass() {
  var p = document.getElementById('otaPassInput').value;
  if (p.length < 4) { alert('OTA password must be at least 4 characters'); return; }
  fetch('/setotapass?pass=' + encodeURIComponent(p))
    .then(function(r){return r.text();}).then(function(d){ alert(d); })
    .catch(function(){ alert('Request failed'); });
}

// ── OTA info ──────────────────────────────────────────────────────────────────
function loadOtaInfo() {
  fetch('/ota_info').then(function(r){return r.text();}).then(function(d){
    var parts = d.trim().split(',');
    var webAddr = parts[0] || '';
    var otaAddr = parts[1] || '';

    // Header row
    document.getElementById('liveWebIP').textContent = webAddr;
    document.getElementById('liveOtaIP').textContent = otaAddr;

    // OTA panel
    document.getElementById('otaWebAddr').textContent  = webAddr;
    document.getElementById('otaFlashAddr').textContent = otaAddr;
  }).catch(function(){});
}

// ── Status monitor ────────────────────────────────────────────────────────────
function startStatusMon() {
  if (statusMon) return;
  statusMon = setInterval(pollStatus, 2500);
}

function stopStatusMon() {
  if (statusMon) { clearInterval(statusMon); statusMon = null; }
}

function pollStatus() {
  fetch('/attack_status').then(function(r){return r.text();}).then(function(d){
    var p = d.split(',');
    var running = p[0] === '1';
    var type    = p[1] || 'idle';
    var pkts    = p[2] || '0';
    document.getElementById('liveCount').textContent = pkts;
    if (!running) { setBadge('idle'); setType(''); stopStatusMon(); }
    else          { setBadge(type); }
  }).catch(function(){});
}

// ── Packet counter (always running) ──────────────────────────────────────────
function pollPkts() {
  fetch('/pkt_count').then(function(r){return r.text();}).then(function(d){
    document.getElementById('liveCount').textContent = d.trim();
  }).catch(function(){});
}

// ── Sysinfo ───────────────────────────────────────────────────────────────────
function loadSysInfo() {
  fetch('/sysinfo').then(function(r){return r.text();}).then(function(d){
    document.getElementById('sysInfo').innerHTML = d;
    var vs = document.querySelectorAll('#sysInfo .stat-v');
    if (vs.length >= 2) document.getElementById('liveHeap').textContent = vs[1].textContent;
  }).catch(function(){});
}

// ── Event log ─────────────────────────────────────────────────────────────────
function loadLog() {
  fetch('/log').then(function(r){return r.text();}).then(function(d){
    var box = document.getElementById('logBox');
    box.textContent = d || '(empty)';
    box.scrollTop   = box.scrollHeight;
  }).catch(function(){});
}

// ── Boot ──────────────────────────────────────────────────────────────────────
loadSysInfo();
loadLog();
loadOtaInfo();
setInterval(loadSysInfo,  6000);
setInterval(pollPkts,     2000);
setInterval(loadLog,     12000);
setInterval(loadOtaInfo, 10000);
</script>
</body>
</html>
)rawliteral";
