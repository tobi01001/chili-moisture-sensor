#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include "debug.h"
#include "settings.h"
#include "sensor.h"
#include "logic.h"
#include "fhem.h"

// Config web server. Reachable in the WiFi (IP on the serial console) and/or
// via the access point "MoistureSensor-Setup" at 192.168.4.1.
static WebServer server(80);
static bool rebootRequested = false;

static const char *PAGE_HEAD =
  "<!doctype html><html lang=en><head><meta charset=utf-8>"
  "<meta name=viewport content='width=device-width,initial-scale=1'>"
  "<title>Moisture Sensor</title><style>"
  "body{font-family:system-ui,-apple-system,Segoe UI,sans-serif;background:#eef2ee;color:#222;"
  "margin:0;padding:12px}"
  "main{max-width:480px;margin:0 auto}"
  "h1{font-size:1.4em;margin:.3em 0}h1 small{display:block;font-size:.55em;color:#667;font-weight:400}"
  "section{background:#fff;border-radius:10px;padding:12px 16px;margin:12px 0;"
  "box-shadow:0 1px 3px #0002}"
  "h2{font-size:1em;margin:.2em 0 .6em;color:#2e7d32;text-transform:uppercase;letter-spacing:.05em}"
  "label{display:block;margin:.7em 0 .2em;font-size:.9em;color:#444}"
  "input,select{width:100%;box-sizing:border-box;padding:9px;font-size:1em;border:1px solid #bbb;"
  "border-radius:6px;background:#fff}"
  "small.hint{display:block;color:#778;margin-top:3px}"
  "button,a.btn{display:inline-block;box-sizing:border-box;background:#2e7d32;color:#fff;border:0;"
  "border-radius:6px;padding:10px 14px;font-size:1em;text-decoration:none;margin:8px 6px 0 0;"
  "cursor:pointer}"
  "button.main{width:100%;margin:12px 0 0;padding:13px}"
  "a.btn.sec{background:#607d8b}"
  ".live{font-size:2em;font-weight:600}.live small{font-size:.45em;font-weight:400;color:#556}"
  "table{width:100%;border-collapse:collapse}td{padding:3px 0}td+td{text-align:right}"
  "</style></head><body><main>";

static String htmlEscape(String s) {
  s.replace("&", "&amp;"); s.replace("\"", "&quot;"); s.replace("<", "&lt;"); s.replace(">", "&gt;");
  return s;
}

static void sendPage(const String &body) {
  server.send(200, "text/html; charset=utf-8", String(PAGE_HEAD) + body + "</main></body></html>");
}

static String field(const char *label, const char *name, const String &val,
                    const char *type = "number", const char *hint = "") {
  String h = String("<label>") + label + "<input name=" + name + " type=" + type +
             (strcmp(type, "number") == 0 ? " inputmode=numeric" : "") +
             " value=\"" + htmlEscape(val) + "\">";
  if (*hint) h += String("<small class=hint>") + hint + "</small>";
  return h + "</label>";
}

static String transportSelect(uint8_t current) {
  const char *names[] = {"WiFi (FHEM telnet)", "Bluetooth (BTHome via Shelly gateway)",
                         "WiFi + Bluetooth (test)"};
  String h = "<label>Transmission<select name=tr>";
  for (int i = 0; i < 3; i++)
    h += String("<option value=") + i + (current == i ? " selected" : "") + ">" + names[i] + "</option>";
  return h + "</select></label>";
}

static String statusSection(const Settings &cfg) {
  Reading r{};
  if (!liveGet(r)) return "<section><h2>Live</h2><p>Measuring... reload the page in a moment.</p></section>";
  return "<section><h2>Live</h2><div class=live>" + String(r.moisture) +
         " % <small>moisture (" + String(r.rawMv) + " mV raw)</small></div>"
         "<table><tr><td>Supply</td><td>" + String(r.supplyMv) + " mV (" + (r.usb ? "USB" : "battery") +
         ")</td></tr><tr><td>Supply calibration factor</td><td>" + String(cfg.supplyCalPm / 1000.0, 3) +
         "</td></tr><tr><td>Firmware</td><td>" + FW_VERSION + "</td></tr></table></section>";
}

static String calibrationSection() {
  return "<section><h2>Sensor calibration</h2>"
         "<p>Hold the sensor dry in the air and press <i>Save as DRY</i>. Then put it into water up to "
         "the marking and press <i>Save as WET</i>.</p>"
         "<a class=btn href='/cal?p=dry'>Save as DRY</a><a class=btn href='/cal?p=wet'>Save as WET</a>"
         "<hr><form action=/calsupply><label>Supply voltage measured with a multimeter (mV)"
         "<input name=mv type=number inputmode=numeric placeholder='e.g. 4012'>"
         "<small class=hint>Measure between the 5V pin and GND of the board (battery or USB connected), "
         "enter the value in millivolts.</small></label>"
         "<button>Calibrate supply voltage</button></form></section>";
}

static String settingsSection(const Settings &cfg) {
  String h = "<form method=post action=/save><section><h2>Transmission</h2>";
  h += transportSelect(cfg.transport);
  h += field("WiFi SSID", "ssid", cfg.ssid, "text");
  h += field("WiFi password", "pass", cfg.pass, "password");
  h += field("FHEM host", "host", cfg.fhemHost, "text", "IP address or name of the FHEM server");
  h += field("FHEM telnet port", "port", String(cfg.fhemPort));
  h += "</section><section><h2>Timing</h2>";
  h += field("Measurement interval (min)", "int", String(cfg.intervalMin));
  h += field("Retry after failure (min)", "retry", String(cfg.retryMin));
  h += field("Heartbeat (min)", "hb", String(cfg.heartbeatMin), "number", "Send at least this often");
  h += field("Send on change of (% moisture)", "delta", String(cfg.deltaPct));
  h += "</section><section><h2>Watering</h2>";
  h += field("Water below (%)", "thr", String(cfg.waterBelowPct));
  h += field("Hysteresis (%)", "hyst", String(cfg.hystPct), "number", "Watering state clears at threshold + hysteresis");
  h += field("Detect watering: rise by (%)", "jump", String(cfg.wateredJumpPct));
  h += "</section><section><h2>Raw calibration values</h2>";
  h += field("Dry (mV)", "dry", String(cfg.rawDry));
  h += field("Wet (mV)", "wet", String(cfg.rawWet));
  h += field("Settle time (ms)", "settle", String(cfg.settleMs), "number",
             "Wait after sensor power-on before reading (100-5000). Recalibrate dry/wet after changing it");
  h += "</section><button class=main>Save &amp; start measuring</button></form>";
  return h;
}

static void addRoutes(Settings &cfg) {
  server.on("/", [&] {
    String h = "<h1>Moisture Sensor<small>Device: " + deviceName() + "</small></h1>";
    h += statusSection(cfg);
    h += calibrationSection();
    h += settingsSection(cfg);
    h += "<p><a class='btn sec' href='/reboot'>Restart without saving</a></p>";
    sendPage(h);
  });

  server.on("/cal", [&] {
    Reading r{};
    if (liveGet(r)) {  // at most a few seconds old, refreshed by the measuring task
      if (server.arg("p") == "dry") cfg.rawDry = r.rawMv; else cfg.rawWet = r.rawMv;
      cfg.save();
    }
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/calsupply", [&] {
    long real = server.arg("mv").toInt();
    Reading live{};
    uint16_t raw = liveGet(live) ? live.supplyRawMv : 0;
    LOGF("Calsupply: real=%ld raw=%u", real, raw);
    if (real > 0 && raw > 0) {
      cfg.supplyCalPm = constrain(real * 1000 / raw, 500, 1500);
      LOGF("Calsupply: new supplyCalPm=%u", cfg.supplyCalPm);
      cfg.save();
    }
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/save", HTTP_POST, [&] {
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
    sendPage("<section><h2>Saved</h2><p>Restarting in measuring mode...</p></section>");
    rebootRequested = true;
  });

  server.on("/reboot", [] {
    sendPage("<section><h2>Restarting</h2><p>Restarting in measuring mode...</p></section>");
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
