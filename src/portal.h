#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "debug.h"
#include "settings.h"
#include "sensor.h"
#include "logic.h"
#include "fhem.h"

// Config web server. Reachable in the WiFi (IP on the serial console) and/or
// via the access point "MoistureSensor-Setup" at 192.168.4.1.
// The web page itself (data/index.html, style.css, app.js) lives in LittleFS ("pio run -t uploadfs");
// the firmware only serves it and provides a small JSON API.
static WebServer server(80);
static bool rebootRequested = false;
static bool fsMounted = false;

// Battery operation: the portal closes PORTAL_TIMEOUT_MS after start. Every user action in the
// web UI guarantees at least PORTAL_EXTEND_MS more. Polling requests do not count as actions.
static const uint32_t PORTAL_TIMEOUT_MS = 5UL * 60 * 1000;
static const uint32_t PORTAL_EXTEND_MS = 2UL * 60 * 1000;
static bool portalTimeLimited = false;
static uint32_t portalDeadlineMs = PORTAL_TIMEOUT_MS;

static void portalTouch() {
  uint32_t d = millis() + PORTAL_EXTEND_MS;
  if ((int32_t)(d - portalDeadlineMs) > 0) portalDeadlineMs = d;
}

static bool portalExpiredNow() { return (int32_t)(millis() - portalDeadlineMs) > 0; }

static int32_t portalLeftS() {
  if (!portalTimeLimited) return -1;
  int32_t left = (int32_t)(portalDeadlineMs - millis());
  return left > 0 ? left / 1000 : 0;
}

static String jsonStr(const String &s) {
  String o = "\"";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
    else o += c;
  }
  return o + "\"";
}

static void sendJson(const String &body, int code = 200) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

static void serveFile(const char *path, const char *mime) {
  File f = fsMounted ? LittleFS.open(path, "r") : File();
  if (!f) {
    server.send(404, "text/plain",
                "Web files not found in the device filesystem. Upload them with: pio run -t uploadfs");
    return;
  }
  server.sendHeader("Cache-Control", "no-cache");
  server.streamFile(f, mime);
  f.close();
}

static String stateJson(const Settings &cfg) {
  Reading r{};
  uint32_t age = 0;
  bool have = liveGet(r, &age);
  String j = "{\"dev\":" + jsonStr(deviceName()) + ",\"fw\":" + jsonStr(FW_VERSION) +
             ",\"cal\":" + String(cfg.supplyCalPm / 1000.0, 3) + ",\"left\":" + String(portalLeftS()) +
             ",\"have\":" + (have ? "true" : "false");
  if (have) {
    j += ",\"moisture\":" + String(r.moisture) + ",\"raw\":" + String(r.rawMv) +
         ",\"supply\":" + String(r.supplyMv) + ",\"usb\":" + (r.usb ? "true" : "false") +
         ",\"age\":" + String(age);
  }
  return j + "}";
}

static String settingsJson(const Settings &cfg) {
  return "{\"tr\":" + String(cfg.transport) + ",\"ssid\":" + jsonStr(cfg.ssid) +
         ",\"pass\":" + jsonStr(cfg.pass) + ",\"host\":" + jsonStr(cfg.fhemHost) +
         ",\"port\":" + String(cfg.fhemPort) + ",\"int\":" + String(cfg.intervalMin) +
         ",\"retry\":" + String(cfg.retryMin) + ",\"hb\":" + String(cfg.heartbeatMin) +
         ",\"delta\":" + String(cfg.deltaPct) + ",\"thr\":" + String(cfg.waterBelowPct) +
         ",\"hyst\":" + String(cfg.hystPct) + ",\"jump\":" + String(cfg.wateredJumpPct) +
         ",\"dry\":" + String(cfg.rawDry) + ",\"wet\":" + String(cfg.rawWet) +
         ",\"settle\":" + String(cfg.settleMs) + "}";
}

static String settleJson() {
  static SettleCurve c;  // too big for the handler stack; handlers never run concurrently
  settleGet(c);
  String j = "{\"state\":" + String(c.state) + ",\"step\":" + String(SETTLE_STEP_MS) +
             ",\"wanted\":" + String(c.wanted) + ",\"mv\":[";
  for (uint16_t i = 0; i < c.count; i++) {
    if (i) j += ',';
    j += c.mv[i];
  }
  return j + "]}";
}

static void addRoutes(Settings &cfg) {
  fsMounted = LittleFS.begin(false);
  if (!fsMounted) LOG("LittleFS mount failed: upload the web files with 'pio run -t uploadfs'");

  server.on("/", [] {
    portalTouch();
    serveFile("/index.html", "text/html; charset=utf-8");
  });
  server.on("/style.css", [] { serveFile("/style.css", "text/css"); });
  server.on("/app.js", [] { serveFile("/app.js", "application/javascript"); });

  // Polled by the page; must not count as user activity.
  server.on("/api/state", HTTP_GET, [&] { sendJson(stateJson(cfg)); });
  server.on("/api/settings", HTTP_GET, [&] { sendJson(settingsJson(cfg)); });
  server.on("/api/settle", HTTP_GET, [] { sendJson(settleJson()); });

  server.on("/api/cal", HTTP_POST, [&] {
    portalTouch();
    Reading r{};
    if (!liveGet(r)) return sendJson("{\"error\":\"no reading yet\"}", 409);  // refreshed by the measuring task
    if (server.arg("p") == "dry") cfg.rawDry = r.rawMv; else cfg.rawWet = r.rawMv;
    cfg.save();
    sendJson("{\"ok\":true}");
  });

  server.on("/api/calsupply", HTTP_POST, [&] {
    portalTouch();
    long real = server.arg("mv").toInt();
    Reading live{};
    uint16_t raw = liveGet(live) ? live.supplyRawMv : 0;
    LOGF("Calsupply: real=%ld raw=%u", real, raw);
    if (real <= 0 || raw == 0) return sendJson("{\"error\":\"invalid value or no reading yet\"}", 400);
    cfg.supplyCalPm = constrain(real * 1000 / raw, 500, 1500);
    LOGF("Calsupply: new supplyCalPm=%u", cfg.supplyCalPm);
    cfg.save();
    sendJson("{\"ok\":true}");
  });

  server.on("/api/settle", HTTP_POST, [] {
    portalTouch();
    long ms = constrain(server.arg("ms").toInt(), 500, 10000);
    if (!settleRequest(ms)) return sendJson("{\"error\":\"already running\"}", 409);
    sendJson("{\"ok\":true}");
  });

  server.on("/save", HTTP_POST, [&] {
    portalTouch();
    cfg.transport = constrain(server.arg("tr").toInt(), 0, 2);
    cfg.ssid = server.arg("ssid");
    cfg.pass = server.arg("pass");
    cfg.fhemHost = server.arg("host");
    cfg.fhemPort = server.arg("port").toInt();
    cfg.intervalMin = max(1L, server.arg("int").toInt());
    cfg.retryMin = max(1L, server.arg("retry").toInt());
    cfg.heartbeatMin = max(1L, server.arg("hb").toInt());
    cfg.deltaPct = constrain(server.arg("delta").toInt(), 1, 100);
    cfg.waterBelowPct = constrain(server.arg("thr").toInt(), 0, 100);
    cfg.hystPct = constrain(server.arg("hyst").toInt(), 0, 100);
    cfg.wateredJumpPct = constrain(server.arg("jump").toInt(), 1, 100);
    cfg.rawDry = server.arg("dry").toInt();
    cfg.rawWet = server.arg("wet").toInt();
    cfg.settleMs = constrain(server.arg("settle").toInt(), 100, 5000);
    cfg.save();
    sendJson("{\"ok\":true}");
    rebootRequested = true;
  });

  server.on("/api/reboot", HTTP_POST, [] {
    sendJson("{\"ok\":true}");
    rebootRequested = true;
  });
  server.begin();
}

// withAp: always offer the access point (button wake / no WiFi configured).
// Otherwise the AP is only started if the WiFi connection fails.
static void startNetwork(const Settings &cfg, bool withAp) {
  bool connected = cfg.ssid.length() && connectWifi(cfg);
  if (withAp || !connected) {
    wifiSetMode(WIFI_AP_STA);
    WiFi.softAP("MoistureSensor-Setup");
  }
  if (connected) LOGF("Config: http://%s/", WiFi.localIP().toString().c_str());
  if (withAp || !connected) LOG("Config: http://192.168.4.1/ (AP MoistureSensor-Setup)");
}
