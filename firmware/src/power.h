#pragma once
#include <Arduino.h>
#include "power_logic.h"

// Napájecí latch (PWR_HOLD), automatické vypnutí, ochrana článků.
namespace power {
  void begin();                        // hned po startu: PWR_HOLD = HIGH
  void activity();                     // tlačítko, příkaz → odkládá automatické vypnutí
  // busy = jízda / nabíjení / BLE / OTA; vrací true, pokud se má vypnout
  pwr::OffReason tick(uint32_t now, bool busy);
  uint16_t secondsLeft(uint32_t now);
  // vypne výstupy, uloží km, uvolní latch; bez latche usne (probudí brzda)
  [[noreturn]] void shutdown(pwr::OffReason why);
}
