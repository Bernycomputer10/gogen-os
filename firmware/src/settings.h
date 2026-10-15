#pragma once
#include <Arduino.h>
#include "protocol.h"

// Všechna nastavení jsou u16 hodnoty indexované klíčem CFG_* (stejně jako v BLE).
struct Settings {
  uint16_t v[CFG_KEY_COUNT + 1];
  uint32_t passkey;          // 0 = výchozí z config.h
};

struct KeyInfo {
  const char *name;          // název pro konzoli
  uint16_t def, min, max;
};

namespace settings {
  extern Settings cfg;
  void load();
  void save();
  void factoryReset();
  // false = neznámý klíč / mimo rozsah / nekonzistentní s jiným klíčem
  bool setKey(uint8_t key, uint16_t value);
  inline uint16_t get(uint8_t key) { return key <= CFG_KEY_COUNT ? cfg.v[key] : 0; }
  const KeyInfo *info(uint8_t key);
  int keyByName(const char *name);   // −1 = nenalezeno
  uint32_t passkey();

  // ujeté km – samostatně, ať se při každé změně nepřepisuje celé nastavení
  uint32_t loadOdometer();
  void saveOdometer(uint32_t meters);
}
