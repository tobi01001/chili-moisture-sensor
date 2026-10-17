#pragma once
#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_timer.h>

// Status LED (SuperMini: blue LED on GPIO8, active low). The power LED is assumed to be removed, so
// this LED is the only visible feedback:
//   solid     awake from deep sleep (measuring)
//   flicker   transmitting via WiFi / Bluetooth
//   1 s blink portal running (200 ms on, 800 ms off)
// The pattern is driven by a timer, so it keeps running while the measuring task blocks.
#ifndef PIN_STATUS_LED
  #define PIN_STATUS_LED 8
#endif
#ifndef STATUS_LED_ON
  #define STATUS_LED_ON 0   // level that lights the LED
#endif

enum LedMode : uint8_t { LED_OFF = 0, LED_SOLID, LED_PORTAL };

static const uint32_t LED_TICK_US = 50000;
static volatile uint8_t ledMode = LED_OFF;
static volatile bool ledSending = false;
static esp_timer_handle_t ledTimer = nullptr;

static void ledWrite(bool on) { gpio_set_level((gpio_num_t)PIN_STATUS_LED, on ? STATUS_LED_ON : !STATUS_LED_ON); }

static void ledTick(void *) {
  static uint8_t tick = 0;
  tick = (tick + 1) % 20;  // 20 ticks = 1 s
  bool on;
  if (ledSending) on = tick & 1;
  else if (ledMode == LED_SOLID) on = true;
  else if (ledMode == LED_PORTAL) on = tick < 4;
  else on = false;
  ledWrite(on);
}

// Call first in setup(): releases the pin hold from deep sleep and lights the LED.
static void ledBegin() {
  gpio_hold_dis((gpio_num_t)PIN_STATUS_LED);
  pinMode(PIN_STATUS_LED, OUTPUT);
  ledMode = LED_SOLID;
  ledSending = false;
  ledWrite(true);
  esp_timer_create_args_t args = {};
  args.callback = ledTick;
  args.name = "led";
  if (esp_timer_create(&args, &ledTimer) == ESP_OK) esp_timer_start_periodic(ledTimer, LED_TICK_US);
}

static void ledSetMode(LedMode m) { ledMode = m; }
static void ledSetSending(bool s) { ledSending = s; }

// Before deep sleep: LED off and held off.
static void ledSleep() {
  if (ledTimer) {
    esp_timer_stop(ledTimer);
    esp_timer_delete(ledTimer);
    ledTimer = nullptr;
  }
  ledWrite(false);
  gpio_hold_en((gpio_num_t)PIN_STATUS_LED);
}
