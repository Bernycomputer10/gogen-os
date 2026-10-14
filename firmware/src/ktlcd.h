#pragma once
#include <Arduino.h>

// Řadič KT-LCD3 – pasivní poslech UART2 (nic se nevysílá). Rychlost se počítá z periody otáčky kola;
// dokud není nastaven obvod kola (`wheel <mm>` v konzoli), je rychlost "není k dispozici".
namespace ktlcd {
  void begin();
  void poll(uint32_t now);
  bool linkOk(uint32_t now);         // dorazil platný rámec v posledních KT_STALE_MS
  bool speedValid(uint32_t now);     // linkOk && kolo nastaveno
  float speedKmh();
  uint32_t odoM();
  uint32_t tripM();
  void resetTrip();
  void setWheelMm(uint16_t mm);      // 0 = nenastaveno; ukládá se do NVS
  uint16_t wheelMm();
  void printDebug(Print &out, uint32_t now);   // poslední surový rámec + čítače
}
