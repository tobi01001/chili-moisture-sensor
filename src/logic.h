#pragma once
#include "settings.h"
#include "sensor.h"

// State kept in RTC memory across deep sleep cycles.
struct State {
  bool init;               // true after the first successful transmission
  bool water;              // current (hysteresis) watering state
  uint8_t lastMoist;       // previous measurement
  uint8_t lastSent;        // last transmitted moisture
  bool watered;            // a watering was detected at some point
  uint32_t minSinceSend;
  uint32_t minSinceWatered;
};
RTC_DATA_ATTR static State st;

// Updates the state with a new reading and decides whether it must be sent.
// elapsedMin: minutes since the previous call.
static bool evaluate(const Settings &c, Reading &r, uint32_t elapsedMin) {
  st.minSinceSend += elapsedMin;
  st.minSinceWatered += elapsedMin;

  bool watered = st.init && r.moisture >= st.lastMoist + c.wateredJumpPct;
  if (watered) { st.watered = true; st.minSinceWatered = 0; }
  st.lastMoist = r.moisture;

  bool prevWater = st.water;
  if (!st.water && r.moisture < c.waterBelowPct) st.water = true;
  else if (st.water && r.moisture >= c.waterBelowPct + c.hystPct) st.water = false;

  r.needsWater = st.water;
  r.wateredAgoH = st.watered ? st.minSinceWatered / 60 : -1;

  return !st.init || watered || st.water != prevWater ||
         abs((int)r.moisture - st.lastSent) >= c.deltaPct ||
         st.minSinceSend >= c.heartbeatMin;
}

static void markSent(const Reading &r) {
  st.init = true;
  st.lastSent = r.moisture;
  st.minSinceSend = 0;
}
