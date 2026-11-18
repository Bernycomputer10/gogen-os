#pragma once
#include <Arduino.h>
#include "drive_logic.h"

// Přední pohon: plyn jezdce (GPIO36), MCP4725, relé fází (GPIO18), brzda předního kontroléru (GPIO19).
namespace drive {
  void begin();
  void tick(uint32_t now);            // volat každých DRIVE_TICK_MS
  void forceOff();                    // klidový plyn, fáze rozepnout (vypínání, OTA)

  void setEnabled(uint8_t arg);       // ARG_ON / ARG_OFF / ARG_TOGGLE (jen pokud je povoleno v nastavení)
  bool enabled();
  void resetFault();
  // kalibrace plynu: 0 = puštěný, 1 = plný; vrací naměřené mV, 0 = chyba
  uint16_t calibrateThrottle(uint8_t which);

  drv::State state();
  uint8_t reasons();
  drv::Fault fault();
  bool phaseClosed();                 // fáze připojené → nabíjení musí být vypnuté
  float throttle();                   // 0..1, záporné = bez signálu
  uint16_t throttleMv();
  float output();                     // 0..1
}
