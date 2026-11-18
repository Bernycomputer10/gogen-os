#pragma once
#include <Arduino.h>

// I2C senzory a DAC: BH1750 (světlo), MPU6050 (zrychlení), MCP4725 (plyn předního motoru).
// Všechny jsou volitelné – co neodpovídá, se každé 3 s zkusí znovu najít.
namespace sensors {
  void begin();
  void poll(uint32_t now);            // volat každých SENSOR_POLL_MS

  bool luxOk();
  float lux();                        // −1 = bez senzoru
  bool autoLightLit();                // výsledek AutoLight (bez senzoru = svítit)

  bool mpuOk();
  float decelG();                     // kladné = brzdí
  bool autoBrakeActive();
  bool emergency();

  bool dacOk();
  bool dacWrite(float volts);         // false = DAC neodpovídá
  bool dacSetPowerOnZero();           // uloží 0 V jako výchozí hodnotu do EEPROM MCP4725
}
