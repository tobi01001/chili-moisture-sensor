#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include "settings.h"
#include "sensor.h"
#include "logic.h"
#include "fhem.h"

// Config web server. Reachable in the WLAN (IP on the serial console) and/or
// via the access point "ChiliSensor-Setup" at 192.168.4.1.
static WebServer server(80);
static bool rebootRequested = false;

static String htmlEscape(String s) {
  s.replace("&", "&amp;"); s.replace("\"", "&quot;"); s.replace("<", "&lt;");
  return s;
}

static String field(const char *label, const char *name, const String &val,
                    const char *type = "number") {
  return String("<p><label>") + label + "<br><input name=" + name + " type=" + type +
         " value=\"" + htmlEscape(val) + "\"></label></p>";
}

static void addRoutes(Settings &cfg) {
  server.on("/", [&] {
    Reading r = measure(cfg);
    String h = "<!doctype html><meta name=viewport content='width=device-width'>"
               "<style>body{font-family:sans-serif;margin:1em}input{width:95%}</style>"
               "<h2>Chili Sensor</h2><p>Live: <b>" + String(r.rawMv) + " mV</b> = " +
               String(r.moisture) + " % &middot; Versorgung " + String(r.supplyMv) +
               " mV (" + (r.usb ? "USB" : "Akku") + ")</p>"
               "<p><a href='/cal?p=dry'>Aktuellen Wert als TROCKEN speichern</a><br>"
               "<a href='/cal?p=wet'>Aktuellen Wert als NASS speichern</a></p>"
               "<form method=post action=/save>";
    h += field("WLAN SSID", "ssid", cfg.ssid, "text");
    h += field("WLAN Passwort", "pass", cfg.pass, "password");
    h += field("FHEM Host", "host", cfg.fhemHost, "text");
    h += field("FHEM Port (FHEMWEB)", "port", String(cfg.fhemPort));
    h += field("FHEM Device (dummy)", "dev", cfg.fhemDevice, "text");
    h += field("Messintervall (min)", "int", String(cfg.intervalMin));
    h += field("Wiederholung bei Fehler (min)", "retry", String(cfg.retryMin));
    h += field("Heartbeat: spätestens senden alle (min)", "hb", String(cfg.heartbeatMin));
    h += field("Senden bei Änderung ab (% Feuchte)", "delta", String(cfg.deltaPct));
    h += field("Gießen unter (%)", "thr", String(cfg.waterBelowPct));
    h += field("Hysterese (%)", "hyst", String(cfg.hystPct));
    h += field("Gegossen-Erkennung: Anstieg um (%)", "jump", String(cfg.wateredJumpPct));
    h += field("Kalibrierung trocken (mV)", "dry", String(cfg.rawDry));
    h += field("Kalibrierung nass (mV)", "wet", String(cfg.rawWet));
    h += "<button>Speichern &amp; Messmodus starten</button></form>"
         "<p><a href='/reboot'>Neustart im Messmodus (ohne Speichern)</a></p>";
    server.send(200, "text/html", h);
  });

  server.on("/cal", [&] {
    uint16_t mv = measure(cfg).rawMv;
    if (server.arg("p") == "dry") cfg.rawDry = mv; else cfg.rawWet = mv;
    cfg.save();
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/save", HTTP_POST, [&] {
    cfg.ssid = server.arg("ssid");
    cfg.pass = server.arg("pass");
    cfg.fhemHost = server.arg("host");
    cfg.fhemPort = server.arg("port").toInt();
    cfg.fhemDevice = server.arg("dev");
    cfg.intervalMin = max(1L, server.arg("int").toInt());
    cfg.retryMin = max(1L, server.arg("retry").toInt());
    cfg.heartbeatMin = max(1L, server.arg("hb").toInt());
    cfg.deltaPct = constrain(server.arg("delta").toInt(), 1, 100);
    cfg.waterBelowPct = constrain(server.arg("thr").toInt(), 0, 100);
    cfg.hystPct = constrain(server.arg("hyst").toInt(), 0, 100);
    cfg.wateredJumpPct = constrain(server.arg("jump").toInt(), 1, 100);
    cfg.rawDry = server.arg("dry").toInt();
    cfg.rawWet = server.arg("wet").toInt();
    cfg.save();
    server.send(200, "text/html", "Gespeichert. Neustart...");
    rebootRequested = true;
  });

  server.on("/reboot", [] {
    server.send(200, "text/html", "Neustart...");
    rebootRequested = true;
  });
  server.begin();
}

// withAp: always offer the access point (button wake / no WLAN configured).
// Otherwise the AP is only started if the WLAN connection fails.
static void startNetwork(const Settings &cfg, bool withAp) {
  WiFi.mode(withAp ? WIFI_AP_STA : WIFI_STA);
  if (cfg.ssid.length()) {
    WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) delay(50);
  }
  bool connected = WiFi.status() == WL_CONNECTED;
  if (!connected && !withAp) WiFi.mode(WIFI_AP_STA);
  if (withAp || !connected) WiFi.softAP("ChiliSensor-Setup");
  if (connected) Serial.printf("Config: http://%s/\n", WiFi.localIP().toString().c_str());
  else Serial.println("Config: http://192.168.4.1/ (AP ChiliSensor-Setup)");
}
