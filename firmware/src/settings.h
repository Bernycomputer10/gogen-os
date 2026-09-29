#pragma once
#include <Arduino.h>

struct Settings {
  uint16_t blinkPeriodMs   = 660;   // ~1,5 Hz
  uint16_t blinkAutoOffS   = 30;    // 0 = nevypínat
  uint16_t battCalPermille = 1000;  // korekce děliče (1000 = 1,000)
  uint16_t battLow10mV     = 3300;  // 33,0 V (3,3 V/čl.)
  uint16_t battCrit10mV    = 3100;  // 31,0 V – odlehčení zátěže
  uint8_t  brakeFlash      = 1;     // 3× problikne při sešlápnutí
  uint8_t  tailWithFront   = 1;     // zadní TAIL se zapíná s předním
  uint8_t  ledBrightness   = 128;
  uint8_t  ledMode         = 0;
  uint8_t  lightsOnBoot    = 0;
  uint32_t passkey         = 0;     // 0 = výchozí z config.h
};

namespace settings {
  extern Settings cfg;
  void load();
  void save();
  void factoryReset();
  // vrací false, pokud je klíč neznámý / hodnota mimo rozsah
  bool setKey(uint8_t key, uint16_t value);
  uint32_t passkey();
}
