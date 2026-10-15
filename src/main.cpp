// Moisture sensor.
// Battery: wake -> measure -> send only if needed -> deep sleep.
// Button wake (GPIO0) or no valid config: config web server for 5 min, measuring and sending
// continues meanwhile, then back to the normal cycle.
// USB (5V present): no sleep, config web server always on, measurement in the loop.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include "debug.h"
#include "settings.h"
#include "sensor.h"
#include "logic.h"
#include "fhem.h"
#include "ble.h"
#include "portal.h"

static const uint32_t PORTAL_TIMEOUT_MS = 5UL * 60 * 1000;

static Settings cfg;
static bool portalTimeLimited = false;  // portal only for PORTAL_TIMEOUT_MS (battery operation)
static uint32_t lastCycle = 0;          // millis() of the last measurement cycle, 0 = none yet
RTC_DATA_ATTR static uint32_t lastSleepMin = 0;

[[noreturn]] static void sleepFor(uint32_t minutes) {
  LOGF("Sleeping %u min", (unsigned)minutes);
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

// One measurement cycle: measure, decide, transmit. Returns false if a needed transmission failed.
static bool cycle(uint32_t elapsedMin) {
  Reading r = measure(cfg);
  bool send = evaluate(cfg, r, elapsedMin);
  #ifdef DEBUG_FORCE_SEND
  send = true;
  #endif
  LOGF("raw=%u mV moisture=%u%% supply=%u mV usb=%d water=%d send=%d",
       r.rawMv, r.moisture, r.supplyMv, r.usb, r.needsWater, send);
  if (!send) return true;
  debugFlash(3);
  if (cfg.useBle()) bleSend(r);  // fire and forget, no acknowledgement
  bool ok = true;
  if (cfg.useWifi()) {
    ok = (WiFi.status() == WL_CONNECTED || connectWifi(cfg)) && sendToFhem(cfg, r);
    if (!ok) wifiCacheInvalidate();  // next attempt: full connect with fresh DHCP
  }
  if (ok) markSent(r);
  return ok;
}

// Runs a cycle when the interval (or the shorter retry interval after a failure) has elapsed.
// Used while the web server is up (portal and USB operation).
static void cycleIfDue() {
  if (!cfg.valid()) return;  // nowhere to send to yet
  uint32_t now = millis();
  if (lastCycle != 0 && now - lastCycle < cfg.intervalMin * 60000UL) return;

  uint32_t elapsedMin = lastCycle == 0 ? 0 : cfg.intervalMin;
  lastCycle = now;
  if (!cycle(elapsedMin)) {
    // pretend the last cycle happened retryMin after its start so that the retry comes early
    lastCycle = millis() - (cfg.intervalMin - cfg.retryMin) * 60000UL;
  }
}

static bool wokenByButton(esp_sleep_wakeup_cause_t cause) {
  return cause == ESP_SLEEP_WAKEUP_GPIO ||
         (cause == ESP_SLEEP_WAKEUP_UNDEFINED && digitalRead(PIN_BUTTON) == LOW);
}

void setup() {
  debugBegin();
  LOG("Starting setup...");
  cfg.load();

  gpio_deep_sleep_hold_dis();
  gpio_hold_dis((gpio_num_t)PIN_BUTTON);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  bool usbMode = usbPresent(cfg);
  bool portalMode = FORCE_PORTAL || wokenByButton(cause) || !cfg.valid();
  LOGF("Wake cause: %d, USB mode: %d, portal mode: %d", (int)cause, usbMode, portalMode);

  if (portalMode || usbMode) {
    // Web server up: loop() serves it and keeps measuring/sending.
    portalTimeLimited = portalMode && !usbMode;
    startNetwork(cfg, portalMode);
    addRoutes(cfg);
    return;
  }

  uint32_t elapsedMin = cause == ESP_SLEEP_WAKEUP_TIMER ? lastSleepMin : 0;
  sleepFor(cycle(elapsedMin) ? cfg.intervalMin : cfg.retryMin);
}

void loop() {
  server.handleClient();
  debugLoop();
  cycleIfDue();

  bool portalExpired = portalTimeLimited && millis() > PORTAL_TIMEOUT_MS;
  if (rebootRequested || portalExpired) {
    delay(500);
    ESP.restart();  // back to the normal measuring mode
  }
  delay(2);
}
