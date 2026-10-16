#pragma once
#include <WiFi.h>
#include "debug.h"
#include "settings.h"
#include "sensor.h"

static const char *FW_VERSION = "0.3.0";

// Unique device id: CRC-16 over the MAC address.
static String deviceCrc() {
  uint64_t efuse = ESP.getEfuseMac();
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < 6; i++) {
    crc ^= (uint8_t)(efuse >> (8 * i)) << 8;
    for (int b = 0; b < 8; b++) crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
  }
  return String(crc);
}

// FHEM creates the device with this name on first contact. BLE advertises only the CRC
// (the prefix is implied); the Shelly script builds the full name.
static String deviceName() { return "Moisture_" + deviceCrc(); }

static String macString() {
  uint64_t efuse = ESP.getEfuseMac();
  char s[13];
  for (int i = 0; i < 6; i++) snprintf(s + 2 * i, 3, "%02X", (uint8_t)(efuse >> (8 * i)));
  return String(s);
}

// Payload for the myUtils function handleMoistureSensor() in FHEM (see docs/FHEM.md):
// {handleMoistureSensor("<device>", "<key> <value>: <key> <value>: ...")}
// Keys are 3-letter abbreviations (same for WiFi and the Shelly BLE gateway); the FHEM
// function expands them to the reading names.
static String buildPayload(const Settings &cfg, const Reading &r) {
  String p = "{handleMoistureSensor(\"" + deviceName() + "\", \"";
  p += "moi " + String(r.moisture);
  p += ": raw " + String(r.rawMv);
  p += ": sup " + String(r.supplyMv);
  p += ": usb " + String(r.usb ? 1 : 0);
  p += ": wtr " + String(r.needsWater ? 1 : 0);
  if (r.wateredAgoH >= 0) p += ": ago " + String(r.wateredAgoH);
  if (r.first) {
    p += ": fwv " + String(FW_VERSION);
    p += ": mac " + macString();
    p += ": int " + String(cfg.intervalMin);
    p += ": hbt " + String(cfg.heartbeatMin);
  }
  p += "\")}\r\n";
  return p;
}

// Push the readings to the FHEM telnet port (same scheme as the ESP32-temperature-sensor).
static bool sendToFhem(const Settings &cfg, const Reading &r) {
  WiFiClient client;
  if (!client.connect(cfg.fhemHost.c_str(), cfg.fhemPort, 5000)) {
    LOG("FHEM: connect failed");
    return false;
  }
  String payload = buildPayload(cfg, r);
  LOG(payload);
  bool ok = client.print(payload) == payload.length();
  client.print("exit\r\n");
  client.flush();
  delay(50);  // let the stack push the data out before closing
  client.stop();
  return ok;
}

static void wifiSetMode(wifi_mode_t mode) {
  WiFi.mode(mode);
  // ESP32-C3 SuperMini boards have a poor antenna matching: full TX power breaks connections
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
}

// WiFi fast connect: channel + BSSID (skips the channel scan) and the DHCP result (static IP,
// skips DHCP) are kept in RTC memory across deep sleep. Reliability rules:
//  - any failure (fast connect, FHEM unreachable, changed credentials) drops the cache and
//    the next attempt is a normal full connect;
//  - a failed fast connect is retried as a full connect immediately;
//  - every WIFI_FAST_MAX_USES connects a full DHCP connect refreshes the lease, so the
//    static IP can not clash with a lease the router has given to someone else meanwhile.
struct WifiCache {
  bool valid;
  uint32_t credHash;      // SSID + password: invalidates the cache when they change
  uint8_t channel;
  uint8_t bssid[6];
  uint32_t ip, gateway, subnet, dns;
  uint8_t fastUses;       // fast connects since the last full DHCP connect
};
RTC_DATA_ATTR static WifiCache wifiCache;
static const uint8_t WIFI_FAST_MAX_USES = 20;
static const uint32_t WIFI_FAST_TIMEOUT_MS = 4000;

static uint32_t credHash(const Settings &cfg) {
  uint32_t h = 2166136261u;  // FNV-1a
  for (char c : cfg.ssid + '\n' + cfg.pass) h = (h ^ (uint8_t)c) * 16777619u;
  return h;
}

static void wifiCacheInvalidate() { wifiCache.valid = false; }

static bool waitForWifi(uint32_t timeoutMs) {
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) delay(20);
  return WiFi.status() == WL_CONNECTED;
}

static bool connectFast(const Settings &cfg) {
  const WifiCache &c = wifiCache;
  WiFi.config(IPAddress(c.ip), IPAddress(c.gateway), IPAddress(c.subnet), IPAddress(c.dns));
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str(), c.channel, c.bssid);
  return waitForWifi(WIFI_FAST_TIMEOUT_MS);
}

static bool connectFull(const Settings &cfg, uint32_t timeoutMs) {
  WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));  // back to DHCP
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  return waitForWifi(timeoutMs);
}

static void wifiCacheStore(const Settings &cfg) {
  WifiCache &c = wifiCache;
  c.valid = true;
  c.credHash = credHash(cfg);
  c.channel = WiFi.channel();
  memcpy(c.bssid, WiFi.BSSID(), 6);
  c.ip = (uint32_t)WiFi.localIP();
  c.gateway = (uint32_t)WiFi.gatewayIP();
  c.subnet = (uint32_t)WiFi.subnetMask();
  c.dns = (uint32_t)WiFi.dnsIP();
  c.fastUses = 0;
}

// Connects as station. An already running access point is kept.
static bool connectWifi(const Settings &cfg, uint32_t timeoutMs = 10000) {
  wifiSetMode(WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA ? WIFI_AP_STA : WIFI_STA);
  WiFi.persistent(false);  // do not write credentials to flash on every connect

  bool useFast = wifiCache.valid && wifiCache.credHash == credHash(cfg) && wifiCache.ip != 0 &&
                 wifiCache.fastUses < WIFI_FAST_MAX_USES;
  if (useFast) {
    if (connectFast(cfg)) {
      wifiCache.fastUses++;
      LOGF("WiFi: fast connect (%u)", wifiCache.fastUses);
      return true;
    }
    LOG("WiFi: fast connect failed, falling back to full connect");
    wifiCacheInvalidate();
    WiFi.disconnect();
  }

  if (!connectFull(cfg, timeoutMs)) return false;
  wifiCacheStore(cfg);
  LOGF("WiFi: full connect, ch %u, %s", wifiCache.channel, WiFi.localIP().toString().c_str());
  return true;
}
