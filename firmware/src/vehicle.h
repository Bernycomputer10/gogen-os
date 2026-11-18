#pragma once
#include <Arduino.h>

// Osvětlení sekundárního okruhu: světla (režim vyp/zap/auto), blinkry, brzda, LED
namespace vehicle {
  void begin();
  void tick(uint32_t now);           // počítá a zapisuje výstupy
  void forceOff();                   // okamžitě všechny výstupy LOW (vypínání, OTA)

  void setFront(uint8_t arg);        // ARG_ON / ARG_OFF / ARG_TOGGLE → ruční režim
  void setTail(uint8_t arg);
  void setLights(uint8_t arg);       // přední + zadní
  void setLightMode(uint8_t mode);   // LIGHT_*
  void cycleLightMode(bool autoAvailable);  // tlačítko: vyp → zap → auto → vyp
  void setAutoLight(bool lit);       // výsledek BH1750
  void setAutoBrake(bool active, bool emergency);
  void setBlink(uint8_t mode);       // BLINK_*
  void toggleBlink(uint8_t mode);
  void setLed(uint8_t arg);
  void setLedBrightness(uint8_t v);
  void setLedMode(uint8_t m);
  void cycleLedMode();
  void setBrakeInput(bool pressed);
  void allOff();

  uint8_t outputsMask();             // OUT_FRONT..OUT_LED
  uint8_t lightMode();
  uint8_t blinkMode();
  bool frontOn();
  bool ledEnabled();
  uint8_t ledBrightness();
  uint8_t ledMode();
  bool brakeInput();
  bool brakeLight();                 // páka nebo akcelerometr
  bool autoBrake();
  bool emergency();
  bool battLow();
  bool battCrit();
}
