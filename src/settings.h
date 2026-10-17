#pragma once
#include <Arduino.h>
#include <Preferences.h>

// All user-adjustable parameters, persisted in NVS (survives deep sleep and reflash).
enum Transport : uint8_t { TR_WIFI = 0, TR_BLE = 1, TR_BOTH = 2 };

struct Settings {
  uint8_t transport = TR_WIFI;   // WLAN->FHEM, BLE (BTHome via Shelly gateway) or both
  String ssid, pass;
  String fhemHost;
  uint16_t fhemPort = 7072;      // FHEM telnet port (no HTTP needed)
  uint16_t intervalMin = 60;     // measurement interval
  uint16_t retryMin = 5;         // interval after a failed transmission
  uint16_t heartbeatMin = 360;   // send at least this often (FHEM sees the sensor is alive)
  uint8_t deltaPct = 5;          // send when moisture changed by this much
  uint8_t waterBelowPct = 35;    // water = 1 below this moisture ...
  uint8_t hystPct = 10;          // ... and back to 0 at threshold + hysteresis
  uint8_t wateredJumpPct = 15;   // a rise by this much counts as "watered"
  uint16_t rawDry = 2350;        // sensor mV in dry air
  uint16_t rawWet = 970;         // sensor mV in water
  uint16_t settleMs = 1000;      // wait between switching the sensor on and reading AOUT
  uint16_t supplyCalPm = 1000;   // supply voltage correction in per mille (1000 = 1.000)

  bool useWifi() const { return transport != TR_BLE; }
  bool useBle() const { return transport != TR_WIFI; }
  bool valid() const { return !useWifi() || (ssid.length() && fhemHost.length()); }

  void load() {
    Preferences p;
    p.begin("moisture", true);
    transport = min<uint8_t>(p.getUChar("tr", transport), TR_BOTH);
    ssid = p.getString("ssid");
    pass = p.getString("pass");
    fhemHost = p.getString("host");
    fhemPort = p.getUShort("port", fhemPort);
    intervalMin = p.getUShort("int", intervalMin);
    retryMin = p.getUShort("retry", retryMin);
    heartbeatMin = p.getUShort("hb", heartbeatMin);
    deltaPct = p.getUChar("delta", deltaPct);
    waterBelowPct = p.getUChar("thr", waterBelowPct);
    hystPct = p.getUChar("hyst", hystPct);
    wateredJumpPct = p.getUChar("jump", wateredJumpPct);
    rawDry = p.getUShort("dry", rawDry);
    rawWet = p.getUShort("wet", rawWet);
    supplyCalPm = p.getUShort("scal", supplyCalPm);
    settleMs = constrain(p.getUShort("settle", settleMs), 100, 5000);
    p.end();
  }

  void save() const {
    Preferences p;
    p.begin("moisture", false);
    p.putUChar("tr", transport);
    p.putString("ssid", ssid);
    p.putString("pass", pass);
    p.putString("host", fhemHost);
    p.putUShort("port", fhemPort);
    p.putUShort("int", intervalMin);
    p.putUShort("retry", retryMin);
    p.putUShort("hb", heartbeatMin);
    p.putUChar("delta", deltaPct);
    p.putUChar("thr", waterBelowPct);
    p.putUChar("hyst", hystPct);
    p.putUChar("jump", wateredJumpPct);
    p.putUShort("dry", rawDry);
    p.putUShort("wet", rawWet);
    p.putUShort("scal", supplyCalPm);
    p.putUShort("settle", settleMs);
    p.end();
  }
};
