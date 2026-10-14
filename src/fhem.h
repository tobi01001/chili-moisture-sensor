#pragma once
#include <WiFi.h>
#include <HTTPClient.h>
#include "settings.h"
#include "sensor.h"

// Push readings to FHEM: FHEMWEB command "setreading" on a dummy device.
// Several commands are chained with ';' in a single HTTP request.
static bool sendToFhem(const Settings &cfg, const Reading &r) {
  String d = cfg.fhemDevice;
  String cmd = "setreading " + d + " moisture " + r.moisture;
  cmd += ";setreading " + d + " raw_mv " + r.rawMv;
  cmd += ";setreading " + d + " battery_mv " + r.batteryMv;
  cmd += ";setreading " + d + " water " + (r.needsWater ? "1" : "0");

  String url = "http://" + cfg.fhemHost + ":" + cfg.fhemPort + "/fhem?XHR=1&cmd=";
  for (char c : cmd) {
    if (isalnum(c) || c == '_') url += c;
    else { char b[4]; snprintf(b, sizeof b, "%%%02X", (uint8_t)c); url += b; }
  }

  HTTPClient http;
  http.setTimeout(5000);
  http.begin(url);
  int code = http.GET();
  http.end();
  Serial.printf("FHEM -> HTTP %d\n", code);
  return code == 200;
}

static bool connectWifi(const Settings &cfg, uint32_t timeoutMs = 10000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) delay(50);
  return WiFi.status() == WL_CONNECTED;
}
