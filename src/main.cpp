// Chili moisture sensor.
// Battery: wake -> measure -> send only if needed -> deep sleep.
// Button wake (GPIO0): config web server for 5 min, then back to sleep.
// USB (5V present): no sleep, config web server always on, measurement in the loop.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include "settings.h"
#include "sensor.h"
#include "logic.h"
#include "fhem.h"
#include "ble.h"
#include "portal.h"

static const uint32_t PORTAL_TIMEOUT_MS = 5UL * 60 * 1000;

static Settings cfg;
static bool usbMode = false;
static uint32_t lastCycle = 0;
RTC_DATA_ATTR static uint32_t lastSleepMin = 0;

[[noreturn]] static void sleepFor(uint32_t minutes) {
  Serial.printf("Sleeping %u min\n", (unsigned)minutes);
  lastSleepMin = minutes;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  gpio_pullup_en((gpio_num_t)PIN_BUTTON);
  gpio_pulldown_dis((gpio_num_t)PIN_BUTTON);
  gpio_hold_en((gpio_num_t)PIN_BUTTON);
  gpio_deep_sleep_hold_en();
  esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_BUTTON, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_sleep_enable_timer_wakeup((uint64_t)minutes * 60ULL * 1000000ULL);
  esp_deep_sleep_start();
}

// One measurement cycle. Returns false if a needed transmission failed.
static bool cycle(uint32_t elapsedMin) {
  Reading r = measure(cfg);
  bool send = evaluate(cfg, r, elapsedMin);
  Serial.printf("raw=%u mV moisture=%u%% supply=%u mV usb=%d water=%d send=%d\n",
                r.rawMv, r.moisture, r.supplyMv, r.usb, r.needsWater, send);
  if (!send) return true;

  if (cfg.useBle()) bleSend(r);  // fire and forget, no acknowledgement
  bool ok = true;
  if (cfg.useWifi()) {
    ok = (WiFi.status() == WL_CONNECTED || connectWifi(cfg)) && sendToFhem(cfg, r);
  }
  if (ok) markSent(r);
  return ok;
}

void setup() {
  Serial.begin(115200);
  cfg.load();

  gpio_deep_sleep_hold_dis();
  gpio_hold_dis((gpio_num_t)PIN_BUTTON);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  esp_sleep_wakeup_cause_t   cause = esp_sleep_get_wakeup_cause();
  bool buttonWake = cause == ESP_SLEEP_WAKEUP_GPIO ||
                    (cause == ESP_SLEEP_WAKEUP_UNDEFINED && digitalRead(PIN_BUTTON) == LOW);

  usbMode = usbPresent(cfg);

  if (buttonWake || !cfg.valid()) {
    startNetwork(cfg, true);
    addRoutes(cfg);
    uint32_t t0 = millis();
    while (!rebootRequested && (usbMode || millis() - t0 < PORTAL_TIMEOUT_MS)) {
      server.handleClient();
      delay(2);
    }
    delay(500);
    ESP.restart();  // back to measuring mode
  }

  if (usbMode) {
    startNetwork(cfg, false);
    addRoutes(cfg);
    return;  // loop() takes over
  }

  uint32_t elapsed = cause == ESP_SLEEP_WAKEUP_TIMER ? lastSleepMin : 0;
  sleepFor(cycle(elapsed) ? cfg.intervalMin : cfg.retryMin);
}

void loop() {
  server.handleClient();
  if (rebootRequested) { delay(500); ESP.restart(); }
  if (lastCycle == 0 || millis() - lastCycle >= cfg.intervalMin * 60000UL) {
    uint32_t elapsed = lastCycle == 0 ? 0 : cfg.intervalMin;
    lastCycle = millis();
    if (!cycle(elapsed)) lastCycle = millis() - (cfg.intervalMin - cfg.retryMin) * 60000UL;
  }
  delay(2);
}
