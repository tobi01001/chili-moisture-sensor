#pragma once
#include <WiFi.h>
#include "settings.h"
#include "sensor.h"

static const char *FW_VERSION = "0.3.0";

// Unique device name: CRC-16 over the MAC address. FHEM creates the device on first contact.
static String deviceName() {
  uint64_t efuse = ESP.getEfuseMac();
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < 6; i++) {
    crc ^= (uint8_t)(efuse >> (8 * i)) << 8;
    for (int b = 0; b < 8; b++) crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
  }
  return "Chili_" + String(crc);
}

static String macString() {
  uint64_t efuse = ESP.getEfuseMac();
  char s[13];
  for (int i = 0; i < 6; i++) snprintf(s + 2 * i, 3, "%02X", (uint8_t)(efuse >> (8 * i)));
  return String(s);
}

// Payload for the myUtils function handleChiliSensor() in FHEM (see docs/FHEM.md):
// {handleChiliSensor("<device>", "<reading> <value>: <reading> <value>: ...")}
static String buildPayload(const Settings &cfg, const Reading &r) {
  String p = "{handleChiliSensor(\"" + deviceName() + "\", \"";
  p += "moisture " + String(r.moisture);
  p += ": raw_mv " + String(r.rawMv);
  p += ": supply_mv " + String(r.supplyMv);
  p += ": usb " + String(r.usb ? 1 : 0);
  p += ": water " + String(r.needsWater ? 1 : 0);
  if (r.wateredAgoH >= 0) p += ": watered_ago_h " + String(r.wateredAgoH);
  if (r.first) {
    p += ": fw_version " + String(FW_VERSION);
    p += ": mac_address " + macString();
    p += ": interval_min " + String(cfg.intervalMin);
    p += ": heartbeat_min " + String(cfg.heartbeatMin);
  }
  p += "\")}\r\n";
  return p;
}

// Push the readings to the FHEM telnet port (same scheme as the ESP32-temperature-sensor).
static bool sendToFhem(const Settings &cfg, const Reading &r) {
  WiFiClient client;
  if (!client.connect(cfg.fhemHost.c_str(), cfg.fhemPort, 5000)) {
    Serial.println("FHEM: connect failed");
    return false;
  }
  String payload = buildPayload(cfg, r);
  Serial.print(payload);
  bool ok = client.print(payload) == payload.length();
  client.print("exit\r\n");
  client.flush();
  delay(50);  // let the stack push the data out before closing
  client.stop();
  return ok;
}

static bool connectWifi(const Settings &cfg, uint32_t timeoutMs = 10000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) delay(50);
  return WiFi.status() == WL_CONNECTED;
}
