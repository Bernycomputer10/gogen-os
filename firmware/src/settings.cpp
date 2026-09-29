#include "settings.h"
#include <Preferences.h>
#include "config.h"
#include "protocol.h"

namespace settings {

Settings cfg;
static Preferences prefs;
static const uint32_t MAGIC = 0x47470002;  // "GG" v2 – nová struktura, staré nastavení se zahodí

void load() {
  prefs.begin("gogen", true);
  bool valid = prefs.getUInt("magic", 0) == MAGIC &&
               prefs.getBytesLength("cfg") == sizeof(Settings);
  if (valid) prefs.getBytes("cfg", &cfg, sizeof(Settings));
  prefs.end();
  if (!valid) cfg = Settings();
}

void save() {
  prefs.begin("gogen", false);
  prefs.putUInt("magic", MAGIC);
  prefs.putBytes("cfg", &cfg, sizeof(Settings));
  prefs.end();
}

void factoryReset() {
  prefs.begin("gogen", false);
  prefs.clear();
  prefs.end();
  cfg = Settings();
}

uint32_t passkey() {
  return (cfg.passkey >= 1 && cfg.passkey <= 999999) ? cfg.passkey : BLE_DEFAULT_PASSKEY;
}

static bool inRange(uint16_t v, uint16_t lo, uint16_t hi) { return v >= lo && v <= hi; }

bool setKey(uint8_t key, uint16_t v) {
  switch (key) {
    case CFG_BLINK_PERIOD:    if (!inRange(v, 300, 2000)) return false; cfg.blinkPeriodMs = v; break;
    case CFG_BLINK_AUTOOFF:   if (v > 600) return false;               cfg.blinkAutoOffS = v; break;
    case CFG_BATT_CAL:        if (!inRange(v, 800, 1200)) return false; cfg.battCalPermille = v; break;
    case CFG_BATT_LOW:        if (!inRange(v, 2800, 4000)) return false; cfg.battLow10mV = v; break;
    case CFG_BATT_CRIT:       if (!inRange(v, 2700, 4000)) return false; cfg.battCrit10mV = v; break;
    case CFG_BRAKE_FLASH:     cfg.brakeFlash = v ? 1 : 0; break;
    case CFG_TAIL_WITH_FRONT: cfg.tailWithFront = v ? 1 : 0; break;
    case CFG_LED_BRIGHT:      if (v > 255) return false; cfg.ledBrightness = v; break;
    case CFG_LED_MODE:        if (v >= LED_MODE_COUNT) return false; cfg.ledMode = v; break;
    case CFG_LIGHTS_ON_BOOT:  cfg.lightsOnBoot = v ? 1 : 0; break;
    default: return false;
  }
  return true;
}

}  // namespace settings
