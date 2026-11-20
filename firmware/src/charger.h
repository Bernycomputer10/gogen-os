#pragma once
#include <Arduino.h>

// Dobíjení sekundární baterie z druhé baterie. Firmware spíná jediný výstup (PIN_CHARGE)
// a drží všechny ochrany; aplikace jen žádá o start/stop a nastavuje cíl.
namespace charger {
  void begin();
  void tick(uint32_t now, float secVolts, uint8_t secPercent, bool bleConnected);

  void setMode(uint8_t mode);        // CHG_MODE_*; změna režimu vypne nabíjení a smaže poruchu
  bool start();                      // jen ruční režim; false = odmítnuto
  void stop();
  bool setKey(uint8_t key, uint16_t value);
  void save();

  uint8_t mode();
  uint8_t phase();
  uint8_t target();
  uint8_t limitDeciA();
  int16_t currentCentiA();           // 10 mA; 0 bez čidla
  uint16_t elapsedMin();
  bool hasSense();
  bool outputOn();
}
