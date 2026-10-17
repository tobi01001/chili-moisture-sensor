#pragma once
#include <Arduino.h>
#include <driver/gpio.h>
#include <soc/gpio_reg.h>
#include "settings.h"

// Above this supply voltage (at the 5V pin) we assume USB; a LiPo never exceeds 4.2 V.
static const uint16_t USB_MIN_MV = 4500;

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

// Both pins drive the same sensor VCC node, so their levels must change in a single register
// write (never one after the other) and be preloaded before the output drivers are enabled.
static void outPutSensorPower(bool on) {
  const uint32_t mask = (1UL << PIN_SENSOR_POWER1) | (1UL << PIN_SENSOR_POWER2);
  REG_WRITE(on ? GPIO_OUT_W1TS_REG : GPIO_OUT_W1TC_REG, mask);
  pinMode(PIN_SENSOR_POWER1, OUTPUT);
  pinMode(PIN_SENSOR_POWER2, OUTPUT);
  gpio_set_drive_capability((gpio_num_t)PIN_SENSOR_POWER1, GPIO_DRIVE_CAP_3);
  gpio_set_drive_capability((gpio_num_t)PIN_SENSOR_POWER2, GPIO_DRIVE_CAP_3);
}

static uint32_t liveStampMs = 0;  // millis() of the last liveStore()

static void liveStore(const Reading &r) {
  portENTER_CRITICAL(&liveMux);
  liveReading = r;
  liveValid = true;
  liveStampMs = millis();
  portEXIT_CRITICAL(&liveMux);
}

// ageMs (optional): how old the reading is.
static bool liveGet(Reading &r, uint32_t *ageMs = nullptr) {
  portENTER_CRITICAL(&liveMux);
  bool ok = liveValid;
  if (ok) r = liveReading;
  if (ageMs) *ageMs = millis() - liveStampMs;
  portEXIT_CRITICAL(&liveMux);
  return ok;
}

// Settling curve: the sensor output after power-on, recorded on request from the portal. The
// recording runs in the measuring task (the only one touching the sensor), the web handlers only
// request it and read the result.
static const uint16_t SETTLE_STEP_MS = 50;
static const uint16_t SETTLE_MAX_SAMPLES = 201;  // 0..10000 ms
enum SettleState : uint8_t { SETTLE_IDLE = 0, SETTLE_REQUESTED, SETTLE_RUNNING, SETTLE_DONE };

struct SettleCurve {
  uint8_t state = SETTLE_IDLE;
  uint16_t wanted = 0;                  // number of samples requested
  uint16_t count = 0;                   // number of samples recorded so far
  uint16_t mv[SETTLE_MAX_SAMPLES] = {};
};
static SettleCurve settleCurve;

// Returns false if a recording is already pending or running.
static bool settleRequest(uint16_t durationMs) {
  uint16_t n = constrain(durationMs / SETTLE_STEP_MS + 1, 2, (int)SETTLE_MAX_SAMPLES);
  portENTER_CRITICAL(&liveMux);
  bool ok = settleCurve.state != SETTLE_REQUESTED && settleCurve.state != SETTLE_RUNNING;
  if (ok) {
    settleCurve.state = SETTLE_REQUESTED;
    settleCurve.wanted = n;
    settleCurve.count = 0;
  }
  portEXIT_CRITICAL(&liveMux);
  return ok;
}

static bool settleRequested() {
  portENTER_CRITICAL(&liveMux);
  bool r = settleCurve.state == SETTLE_REQUESTED;
  portEXIT_CRITICAL(&liveMux);
  return r;
}

static void settleGet(SettleCurve &out) {
  portENTER_CRITICAL(&liveMux);
  out = settleCurve;
  portEXIT_CRITICAL(&liveMux);
}

// Average several ADC samples (in mV) to suppress noise. Blocking (~32 ms): call it from the
// measuring task or before deep sleep, never from loop().
static uint16_t sampleMv(int pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogReadMilliVolts(pin);
    delay(2);
  }
  return sum / 16;
}

static uint16_t readMv(int pin) {
  uint16_t mv = sampleMv(pin);
  if(pin == PIN_SENSOR_ADC) {
    LOGF("Sensor raw mV: %u", mv);
  } else if(pin == PIN_SUPPLY_ADC) {
    LOGF("Supply raw mV: %u", mv);
  }
  else {
    LOGF("readMv(%d) = %u", pin, mv);
  }
  return mv;
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
  delay(cfg.settleMs);  // sensor oscillator and output filter must settle
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

// Records the settling curve (called by the measuring task after settleRequested()).
// Starts from a switched-off sensor so that every curve is a cold start.
static void settleRun() {
  uint16_t n;
  portENTER_CRITICAL(&liveMux);
  settleCurve.state = SETTLE_RUNNING;
  n = settleCurve.wanted;
  portEXIT_CRITICAL(&liveMux);

  outPutSensorPower(false);
  delay(500);
  uint32_t t0 = millis();
  outPutSensorPower(true);
  for (uint16_t i = 0; i < n; i++) {
    while ((uint32_t)(millis() - t0) < (uint32_t)i * SETTLE_STEP_MS) delay(1);
    uint16_t mv = sampleMv(PIN_SENSOR_ADC);
    portENTER_CRITICAL(&liveMux);
    settleCurve.mv[i] = mv;
    settleCurve.count = i + 1;
    portEXIT_CRITICAL(&liveMux);
  }
  outPutSensorPower(false);
  portENTER_CRITICAL(&liveMux);
  settleCurve.state = SETTLE_DONE;
  portEXIT_CRITICAL(&liveMux);
  LOGF("Settling curve recorded: %u samples", n);
}
