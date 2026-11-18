#pragma once
#include <Arduino.h>
#include "charge_logic.h"

// Dobíjení z předního kola: měření generátoru (GPIO35) a výstup CHG_EN (GPIO17).
namespace charge {
  void begin();
  void tick(uint32_t now, bool frontDriveConnected);
  void forceOff();

  void setMode(chg::Mode m);
  void resetFault();
  chg::State state();
  chg::Mode mode();
  uint8_t reasons();
  chg::Fault fault();
  bool output();
  float genVolts();
  uint16_t chargingMin(uint32_t now);
}
