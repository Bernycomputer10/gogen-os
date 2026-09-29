#pragma once
#include <Arduino.h>

// Logika sekundárního okruhu: světla, blinkry, brzda, LED
namespace vehicle {
  void begin();
  void tick(uint32_t now);           // počítá a zapisuje výstupy

  void setFront(uint8_t arg);        // ARG_ON / ARG_OFF / ARG_TOGGLE
  void setTail(uint8_t arg);
  void setLights(uint8_t arg);       // přední + zadní
  void setBlink(uint8_t mode);       // BLINK_*
  void toggleBlink(uint8_t mode);    // stejný směr podruhé = vypnout
  void setLed(uint8_t arg);
  void setLedBrightness(uint8_t v);
  void setLedMode(uint8_t m);
  void cycleLedMode();
  void setBrakeInput(bool pressed);
  void allOff();

  uint8_t outputsMask();             // OUT_* bity
  uint8_t blinkMode();
  bool frontOn();
  bool ledEnabled();
  uint8_t ledBrightness();
  uint8_t ledMode();
  bool brakeInput();
  bool battLow();
  bool battCrit();
}
