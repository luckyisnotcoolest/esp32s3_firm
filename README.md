# ArsWebUI v4.beta — ESP32-S3 N16R8

Marauder-style WiFi attack tool + Apple Juice BLE spam + Evil Portal captive credential harvester.  
Compiled entirely on GitHub Actions. No local toolchain needed.

---

## Repo layout (exact — don't rename anything)

```
your-repo/
├── ArsWebUI.ino          ← main sketch
├── web_content.h         ← embedded HTML (INDEX_HTML, CAPTIVE_HTML, SUCCESS_HTML)
├── README.md
└── .github/
    └── workflows/
        └── build.yml     ← CI pipeline
```

---

## How to build

1. Fork or create a private repo
2. Drop all four files in at the paths above (sketch files at root, workflow at `.github/workflows/`)
3. Push — GitHub Actions triggers automatically
4. Actions → your run → **ArsWebUI-v4beta-firmware** artifact → download `.zip`
5. Unzip → flash `ArsWebUI.ino.bin` (or the merged `.bin`) with esptool or Arduino IDE

---

## Flash settings (must match FQBN in build.yml)

| Setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| Flash Size | 16MB (128Mb) |
| Partition Scheme | Huge APP (3MB No OTA / 1MB SPIFFS) |
| PSRAM | OPI PSRAM |
| USB Mode | Hardware CDC and JTAG |
| USB CDC On Boot | **Disabled** ← CRITICAL |
| CPU Frequency | 240 MHz |
| Upload Speed | 921600 |

---

## The libnet80211 patch (why deauth works)

`esp_wifi_80211_tx()` passes frames through `ieee80211_raw_frame_sanity_check()`  
before TX. On ESP-IDF 5.x / Arduino Core 3.x the built-in implementation  
blocks deauth/disassoc/auth frames — they return `ESP_ERR_INVALID_ARG` silently  
and nothing goes out.

The fix is a two-step override:

**Step 1 — objcopy (build.yml step 5)**  
Weakens the symbol inside `libnet80211.a` so the linker will prefer any other definition:
```bash
xtensa-esp32s3-elf-objcopy \
  --weaken-symbol=ieee80211_raw_frame_sanity_check \
  libnet80211.a libnet80211.a
```

**Step 2 — extern "C" stub (ArsWebUI.ino, line ~73)**  
Provides a replacement that always returns 0 (pass):
```cpp
extern "C" int ieee80211_raw_frame_sanity_check(int32_t a, int32_t b, int32_t c) {
  (void)a; (void)b; (void)c;
  return 0;
}
```

The linker resolves the strong stub from the sketch over the now-weak lib symbol.  
Both steps are required. The CI verifies the symbol became weak before compiling.

---

## First connect

- AP SSID: `KNHS HOTSPOT PRIVATE`
- AP pass: `knhsattack12`
- Web UI: `http://192.168.4.1`
- OTA: port 3232, password `arswebui`

Change AP credentials from the WebUI → Settings tab.

---

## Attacks

| Attack | Endpoint | Notes |
|---|---|---|
| Deauth (targeted) | `/deauth?bssid=XX:XX...&ch=N` | Both-IF TX, reason rotation, optional client MAC |
| Deauth All (AP channel) | `/deauth_all` | Scans then deauths all visible APs |
| Deauth All (multichannel) | `/deauth_all_ch` | Hops ch1-13, PSRAM target buffer, AP restarts between sweeps |
| CSA Attack | `/csa?bssid=XX:XX...&ch=N` | Channel switch announcement flood |
| Beacon Spam | `/beacon_spam` | 20 fake SSIDs, randomized BSSIDs |
| UDP Flood | `/udp_flood?ip=X&port=N` | 1KB random payloads |
| Apple Juice | `/applejuice` | BLE proximity spam (AirPods, AirTag, etc.) |
| Evil Portal | `/portal_start?ssid=Free-WiFi` | Open AP + DNS hijack + credential capture |
| Stop all | `/stop` | |

Credentials from Evil Portal: `/portal_creds` (plain text download) — also available in WebUI PORTAL tab.
