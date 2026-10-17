#pragma once
#include <Arduino.h>

// Debug switches, set via build flags (see env:supermini-debug in platformio.ini):
//   DEBUG_LOG          serial log output (USB CDC), compiled out otherwise
//   DEBUG_FORCE_PORTAL treat every start like a button wake: config portal + measuring
//   DEBUG_FORCE_SEND   always send data, regardless of evaluation
#ifdef DEBUG_ALL
  #ifndef DEBUG_LOG
    #define DEBUG_LOG
  #endif
  #ifndef DEBUG_FORCE_PORTAL
    #define DEBUG_FORCE_PORTAL
  #endif
  #ifndef DEBUG_FORCE_SEND
    #define DEBUG_FORCE_SEND
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

  // Native USB CDC: output printed before the host has attached is lost
  for (uint32_t t0 = millis(); !Serial.isConnected() && millis() - t0 < 10000;)
  {
    if(!Serial.isConnected()) {
      Serial.end();
      delay(100);
      Serial.begin(115200);
    }
    delay(100);
  }
#endif
}
