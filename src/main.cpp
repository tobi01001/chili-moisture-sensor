// Chili moisture sensor: wake -> measure -> send to FHEM -> deep sleep.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include "settings.h"
#include "sensor.h"
#include "fhem.h"
#include "portal.h"

static Settings cfg;

[[noreturn]] static void sleepFor(uint32_t minutes) {
  Serial.printf("Sleeping %u min\n", (unsigned)minutes);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_sleep_enable_timer_wakeup((uint64_t)minutes * 60ULL * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  cfg.load();

  pinMode(PIN_CONFIG, INPUT_PULLUP);
  if (!cfg.valid() || digitalRead(PIN_CONFIG) == LOW) {
    Serial.println("Config portal");
    if (runConfigPortal(cfg)) ESP.restart();
    sleepFor(cfg.intervalMin);  // timeout: continue normal operation
  }

  Reading r = measure(cfg);
  Serial.printf("raw=%u mV moisture=%u%% battery=%u mV water=%d\n",
                r.rawMv, r.moisture, r.batteryMv, r.needsWater);

  bool ok = connectWifi(cfg) && sendToFhem(cfg, r);
  sleepFor(ok ? cfg.intervalMin : cfg.retryMin);
}

void loop() {}
