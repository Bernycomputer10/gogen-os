#pragma once
#include <Arduino.h>

namespace battery {
  void begin();
  void poll();                 // volat každých BATTERY_POLL_MS
  float volts();               // filtrované napětí baterie [V]
  uint8_t percent();
  uint16_t adcMilliVolts();    // poslední průměr na pinu ADC
  // Nastaví kalibraci podle skutečně změřeného napětí (multimetr).
  // Vrací novou hodnotu battCalPermille, 0 = mimo rozsah.
  uint16_t calibrate(float realVolts);
}
