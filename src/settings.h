#pragma once
#include <Arduino.h>
#include <Preferences.h>

// All user-adjustable parameters, persisted in NVS (survives deep sleep and reflash).
struct Settings {
  String ssid, pass;
  String fhemHost;            // FHEM server (FHEMWEB)
  uint16_t fhemPort = 8083;
  String fhemDevice = "chili1";  // dummy device in FHEM that receives the readings
  uint16_t intervalMin = 60;  // wake-up interval
  uint16_t retryMin = 5;      // wake-up interval after a failed transmission
  uint16_t rawDry = 3000;     // sensor mV in dry air
  uint16_t rawWet = 1300;     // sensor mV in water
  uint8_t waterBelowPct = 35; // recommend watering below this moisture

  bool valid() const { return ssid.length() && fhemHost.length(); }

  void load() {
    Preferences p;
    p.begin("chili", true);
    ssid = p.getString("ssid");
    pass = p.getString("pass");
    fhemHost = p.getString("host");
    fhemPort = p.getUShort("port", fhemPort);
    fhemDevice = p.getString("dev", fhemDevice);
    intervalMin = p.getUShort("int", intervalMin);
    retryMin = p.getUShort("retry", retryMin);
    rawDry = p.getUShort("dry", rawDry);
    rawWet = p.getUShort("wet", rawWet);
    waterBelowPct = p.getUChar("thr", waterBelowPct);
    p.end();
  }

  void save() const {
    Preferences p;
    p.begin("chili", false);
    p.putString("ssid", ssid);
    p.putString("pass", pass);
    p.putString("host", fhemHost);
    p.putUShort("port", fhemPort);
    p.putString("dev", fhemDevice);
    p.putUShort("int", intervalMin);
    p.putUShort("retry", retryMin);
    p.putUShort("dry", rawDry);
    p.putUShort("wet", rawWet);
    p.putUChar("thr", waterBelowPct);
    p.end();
  }
};
