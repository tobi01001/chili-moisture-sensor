#pragma once
#include <Arduino.h>
#include "settings.h"

struct Reading {
  uint16_t rawMv;     // averaged sensor output in mV
  uint8_t moisture;   // 0..100 %
  uint16_t batteryMv; // 0 if battery measurement is disabled
  bool needsWater;
};

// Average several ADC samples (in mV) to suppress noise.
static uint16_t readMv(int pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogReadMilliVolts(pin);
    delay(2);
  }
  return sum / 16;
}

static Reading measure(const Settings &cfg) {
  Reading r{};

  // Power the sensor only while measuring.
  pinMode(PIN_SENSOR_POWER, OUTPUT);
  digitalWrite(PIN_SENSOR_POWER, HIGH);
  delay(100);  // let the sensor oscillator settle
  r.rawMv = readMv(PIN_SENSOR_ADC);
  digitalWrite(PIN_SENSOR_POWER, LOW);

  // Capacitive sensor: higher voltage = drier soil.
  long span = (long)cfg.rawDry - cfg.rawWet;
  long pct = span != 0 ? ((long)cfg.rawDry - r.rawMv) * 100 / span : 0;
  r.moisture = constrain(pct, 0, 100);
  r.needsWater = r.moisture < cfg.waterBelowPct;

#if PIN_BATTERY_ADC >= 0
  r.batteryMv = readMv(PIN_BATTERY_ADC) * 2;  // 1:1 divider
#endif
  return r;
}
