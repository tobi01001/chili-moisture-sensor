#pragma once
#include <Arduino.h>
#include "settings.h"

// Above this supply voltage (at the 5V pin) we assume USB; a LiPo never exceeds 4.2 V.
static const uint16_t USB_MIN_MV = 4500;

struct Reading {
  uint16_t rawMv;      // averaged sensor output in mV
  uint8_t moisture;    // 0..100 %
  uint16_t supplyMv;   // voltage at the 5V pin (= battery voltage when not on USB)
  bool usb;
  bool needsWater;     // filled in by evaluate()
  int32_t wateredAgoH; // -1 = unknown
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

static bool usbPresent() { return readMv(PIN_SUPPLY_ADC) * 2 > USB_MIN_MV; }

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

  r.supplyMv = readMv(PIN_SUPPLY_ADC) * 2;  // 1:1 divider
  r.usb = r.supplyMv > USB_MIN_MV;
  r.wateredAgoH = -1;
  return r;
}
