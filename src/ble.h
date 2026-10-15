#pragma once
#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include "fhem.h"
#include "sensor.h"

// BTHome v2 advertisement (unencrypted, non-connectable), received by a Shelly gateway
// (fhem/shelly_chili_gateway.js). Exactly 31 bytes: service data + complete local name
// "Chili_<crc>", no flags. BTHome objects must be sorted by object id.
RTC_DATA_ATTR static uint8_t blePacketId = 0;

static void bleSend(const Reading &r) {
  String name = deviceName();
  // Linear LiPo estimate: 3.0 V = 0 %, 4.2 V = 100 %
  uint8_t batt = constrain(((long)r.supplyMv - 3000) * 100 / 1200, 0, 100);

  uint8_t sd[] = {
    0xD2, 0xFC, 0x40,                                   // UUID 0xFCD2, BTHome v2, no encryption
    0x00, ++blePacketId,                                // packet id (receiver de-duplicates)
    0x01, batt,                                         // battery %
    0x0C, (uint8_t)r.supplyMv, (uint8_t)(r.supplyMv >> 8),  // voltage, 0.001 V
    0x0F, (uint8_t)(r.needsWater ? 1 : 0),              // generic boolean: water
    0x10, (uint8_t)(r.usb ? 1 : 0),                     // power on: USB supply
    0x2F, r.moisture                                    // moisture %
  };
  uint8_t adv[31];
  size_t n = 0;
  adv[n++] = sizeof(sd) + 1; adv[n++] = 0x16;
  memcpy(adv + n, sd, sizeof(sd)); n += sizeof(sd);
  adv[n++] = name.length() + 1; adv[n++] = 0x09;
  memcpy(adv + n, name.c_str(), name.length()); n += name.length();

  BLEDevice::init("");
  BLEAdvertising *a = BLEDevice::getAdvertising();
  BLEAdvertisementData data;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  data.addData(String((const char *)adv, n));
#else
  data.addData(std::string((const char *)adv, n));
#endif
  a->setAdvertisementData(data);
  a->setAdvertisementType(ADV_TYPE_NONCONN_IND);
  a->setMinInterval(0xA0);  // 100 ms
  a->setMaxInterval(0xA0);
  a->start();
  delay(1500);              // ~15 repeats so the gateway reliably catches one
  a->stop();
  BLEDevice::deinit(false);
}
