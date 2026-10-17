#pragma once
#include <Arduino.h>
#include <driver/gpio.h>
#include "settings.h"

// Above this supply voltage (at the 5V pin) we assume USB; a LiPo never exceeds 4.2 V.
static const uint16_t USB_MIN_MV = 4500;

// Time between switching the sensor on and reading AOUT (build flag overridable).
#ifndef SENSOR_SETTLE_MS
  #define SENSOR_SETTLE_MS 1000
#endif

struct Reading {
  uint16_t rawMv;      // averaged sensor output in mV
  uint8_t moisture;    // 0..100 %
  uint16_t supplyMv;   // voltage at the 5V pin (= battery voltage when not on USB)
  uint16_t supplyRawMv;  // same, without the calibration factor
  bool usb;
  bool needsWater;     // filled in by evaluate()
  bool first;          // first transmission since power-on: also send static info
  int32_t wateredAgoH; // -1 = unknown
};

// Most recent measurement, shared between the measuring task and the web handlers so that the
// handlers never touch the ADC or the sensor power pin (and never block).
static Reading liveReading;
static bool liveValid = false;
static portMUX_TYPE liveMux = portMUX_INITIALIZER_UNLOCKED;

static void outPutSensorPower(bool on) {
  if(on) {
    pinMode(PIN_SENSOR_POWER1, OUTPUT);
    pinMode(PIN_SENSOR_POWER2, OUTPUT);
    gpio_set_drive_capability((gpio_num_t)PIN_SENSOR_POWER1, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability((gpio_num_t)PIN_SENSOR_POWER2, GPIO_DRIVE_CAP_3);
    digitalWrite(PIN_SENSOR_POWER1, HIGH);
    digitalWrite(PIN_SENSOR_POWER2, HIGH);
  } else {
    // set the outputs to low to switch off the sensor
    pinMode(PIN_SENSOR_POWER1, OUTPUT);
    pinMode(PIN_SENSOR_POWER2, OUTPUT);
    gpio_set_drive_capability((gpio_num_t)PIN_SENSOR_POWER1, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability((gpio_num_t)PIN_SENSOR_POWER2, GPIO_DRIVE_CAP_3);
    digitalWrite(PIN_SENSOR_POWER1, LOW);
    digitalWrite(PIN_SENSOR_POWER2, LOW);
  }
}

static void liveStore(const Reading &r) {
  portENTER_CRITICAL(&liveMux);
  liveReading = r;
  liveValid = true;
  portEXIT_CRITICAL(&liveMux);
}

static bool liveGet(Reading &r) {
  portENTER_CRITICAL(&liveMux);
  bool ok = liveValid;
  if (ok) r = liveReading;
  portEXIT_CRITICAL(&liveMux);
  return ok;
}

// Average several ADC samples (in mV) to suppress noise. Blocking (~32 ms): call it from the
// measuring task or before deep sleep, never from loop().
static uint16_t readMv(int pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogReadMilliVolts(pin);
    delay(2);
  }
  if(pin == PIN_SENSOR_ADC) {
    LOGF("Sensor raw mV: %lu", sum / 16);
  } else if(pin == PIN_SUPPLY_ADC) {
    LOGF("Supply raw mV: %lu", sum / 16);
  }
  else {
    LOGF("readMv(%d) = %lu", pin, sum / 16);
  }
  return sum / 16;
}

// Two equal resistors form the divider (any value, e.g. 100k..1M): factor 2.
// The per-mille correction compensates resistor tolerance and ADC offset.
static uint16_t rawSupplyMv() { return readMv(PIN_SUPPLY_ADC) * 2; }

static uint16_t supplyMv(const Settings &cfg) {
  return (uint32_t)rawSupplyMv() * cfg.supplyCalPm / 1000;
}

static bool usbPresent(const Settings &cfg) { return supplyMv(cfg) > USB_MIN_MV; }

static Reading measure(const Settings &cfg) {
  Reading r{};

  // Power the sensor only while measuring.
  outPutSensorPower(true);
#ifdef DEBUG_SETTLE_LOG
  // Shows how long the output needs after power-on: tune SENSOR_SETTLE_MS from this.
  for (int t = 0; t <= 10000; t += 50) {
    LOGF("settle %4d ms: %u mV", t, (unsigned)readMv(PIN_SENSOR_ADC));
    delay(50-32);
  }
#else
  delay(SENSOR_SETTLE_MS);  // sensor oscillator and output filter must settle
#endif
  r.rawMv = readMv(PIN_SENSOR_ADC);
  outPutSensorPower(false);

  // Capacitive sensor: higher voltage = drier soil.
  long span = (long)cfg.rawDry - cfg.rawWet;
  long pct = span != 0 ? ((long)cfg.rawDry - r.rawMv) * 100 / span : 0;
  r.moisture = constrain(pct, 0, 100);

  r.supplyRawMv = rawSupplyMv();
  r.supplyMv = (uint32_t)r.supplyRawMv * cfg.supplyCalPm / 1000;
  r.usb = r.supplyMv > USB_MIN_MV;
  r.wateredAgoH = -1;
  liveStore(r);
  return r;
}
