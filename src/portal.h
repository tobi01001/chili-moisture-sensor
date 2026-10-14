#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include "settings.h"
#include "sensor.h"

// Config portal: the sensor opens an access point and serves one HTML page.
// It is left after saving or after PORTAL_TIMEOUT_MS (to protect the battery).
static const uint32_t PORTAL_TIMEOUT_MS = 5UL * 60 * 1000;

static String htmlEscape(String s) {
  s.replace("&", "&amp;"); s.replace("\"", "&quot;"); s.replace("<", "&lt;");
  return s;
}

static String field(const char *label, const char *name, const String &val,
                    const char *type = "text") {
  return String("<p><label>") + label + "<br><input name=" + name + " type=" + type +
         " value=\"" + htmlEscape(val) + "\"></label></p>";
}

// Returns when settings were saved (caller restarts) or on timeout.
static bool runConfigPortal(Settings &cfg) {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ChiliSensor-Setup");  // open AP, 192.168.4.1
  WebServer server(80);
  bool saved = false;

  server.on("/", [&] {
    Reading r = measure(cfg);
    String h = "<!doctype html><meta name=viewport content='width=device-width'>"
               "<style>body{font-family:sans-serif;margin:1em}input{width:95%}</style>"
               "<h2>Chili Sensor</h2><p>Live: <b>" + String(r.rawMv) + " mV</b> = " +
               String(r.moisture) + " % &middot; Akku " + String(r.batteryMv) + " mV</p>"
               "<p><a href='/cal?p=dry'>Aktuellen Wert als TROCKEN speichern</a><br>"
               "<a href='/cal?p=wet'>Aktuellen Wert als NASS speichern</a></p>"
               "<form method=post action=/save>";
    h += field("WLAN SSID", "ssid", cfg.ssid);
    h += field("WLAN Passwort", "pass", cfg.pass, "password");
    h += field("FHEM Host", "host", cfg.fhemHost);
    h += field("FHEM Port (FHEMWEB)", "port", String(cfg.fhemPort), "number");
    h += field("FHEM Device (dummy)", "dev", cfg.fhemDevice);
    h += field("Intervall (min)", "int", String(cfg.intervalMin), "number");
    h += field("Wiederholung bei Fehler (min)", "retry", String(cfg.retryMin), "number");
    h += field("Gie&szlig;en unter (%)", "thr", String(cfg.waterBelowPct), "number");
    h += field("Kalibrierung trocken (mV)", "dry", String(cfg.rawDry), "number");
    h += field("Kalibrierung nass (mV)", "wet", String(cfg.rawWet), "number");
    h += "<button>Speichern &amp; neu starten</button></form>";
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
    cfg.waterBelowPct = constrain(server.arg("thr").toInt(), 0, 100);
    cfg.rawDry = server.arg("dry").toInt();
    cfg.rawWet = server.arg("wet").toInt();
    cfg.save();
    server.send(200, "text/html", "Gespeichert. Neustart...");
    saved = true;
  });

  server.begin();
  uint32_t t0 = millis();
  while (!saved && millis() - t0 < PORTAL_TIMEOUT_MS) {
    server.handleClient();
    delay(2);
  }
  delay(500);
  return saved;
}
