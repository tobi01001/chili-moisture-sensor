#pragma once
#include <Arduino.h>

// Debug switches, set via build flags (see env:supermini-debug in platformio.ini):
//   DEBUG_LOG          serial log output (USB CDC), compiled out otherwise
//   DEBUG_FORCE_PORTAL treat every start like a button wake: config portal + measuring
//   DEBUG_LED          onboard LED: heartbeat blink in loop(), flashes on transmission
//   DEBUG_FORCE_SEND   always send data, regardless of evaluation

#ifdef DEBUG_ALL
  #ifndef DEBUG_LOG
    #define DEBUG_LOG
  #endif
  #ifndef DEBUG_FORCE_PORTAL
    #define DEBUG_FORCE_PORTAL
  #endif
  #ifndef DEBUG_LED
    #define DEBUG_LED
  #endif
  #ifndef DEBUG_FORCE_SEND
    #define DEBUG_FORCE_SEND
  #endif
#endif

#ifdef DEBUG_LED
  #ifndef PIN_DEBUG_LED
    #define PIN_DEBUG_LED 8   // SuperMini: blue LED on GPIO8, active low
  #endif
  #ifndef DEBUG_LED_ON
    #define DEBUG_LED_ON LOW
  #endif
#endif
#ifdef DEBUG_LOG
  #define LOG(msg)       Serial.println(msg)
  #define LOGF(fmt, ...) Serial.printf(fmt "\n", ##__VA_ARGS__)
#else
  #define LOG(msg)       do {} while (0)
  #define LOGF(fmt, ...) do {} while (0)
#endif

#ifdef DEBUG_FORCE_PORTAL
  static const bool FORCE_PORTAL = true;
#else
  static const bool FORCE_PORTAL = false;
#endif

static void debugBegin() {
#ifdef DEBUG_LOG
  Serial.begin(115200);
  #ifdef DEBUG_LED
  pinMode(PIN_DEBUG_LED, OUTPUT);
  #endif

  
  // Native USB CDC: output printed before the host has attached is lost
  for (uint32_t t0 = millis(); !Serial.isConnected() && millis() - t0 < 10000;)
  {
    if(!Serial.isConnected()) {
      Serial.end();
      delay(100);
      Serial.begin(115200);
    }
    #ifdef DEBUG_LED
    static bool on = false;
    on = !on;
    digitalWrite(PIN_DEBUG_LED, on? DEBUG_LED_ON : !DEBUG_LED_ON);
    #endif
    delay(100);
  }
#endif
}

// Heartbeat: toggles the LED every 500 ms. Non-blocking, call from loop().
static void debugLoop() {
#ifdef DEBUG_LED
  static uint32_t last = 0;
  static bool on = false;
  if (millis() - last < 500) return;
  last = millis();
  on = !on;
  digitalWrite(PIN_DEBUG_LED, on ? DEBUG_LED_ON : !DEBUG_LED_ON);
#endif
}

// Short blocking flashes, e.g. while transmitting.
static void debugFlash(uint8_t times) {
#ifdef DEBUG_LED
  for (uint8_t i = 0; i < times; i++) {
    digitalWrite(PIN_DEBUG_LED, DEBUG_LED_ON);
    delay(10);
    digitalWrite(PIN_DEBUG_LED, !DEBUG_LED_ON);
    delay(60);
  }
#endif
}
