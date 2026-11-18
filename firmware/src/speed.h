#pragma once
#include <Arduino.h>

// Rychloměr z hallů zadního (přes optočlen) a předního motoru + počítadlo km.
namespace speed {
  void begin();
  void update(uint32_t now);   // volat často (přepočet, časový limit pulzů)

  bool rearValid();            // od zapnutí už přišel aspoň jeden pulz (signál je zapojený)
  bool frontValid();
  float rearKmh();
  float frontKmh();
  float kmh();                 // zadní, jinak přední
  bool known();                // máme aspoň jeden zdroj rychlosti

  uint32_t odometerM();
  uint32_t tripM();
  void resetTrip();
  void saveNow();              // uložit km (vypínání)
}
