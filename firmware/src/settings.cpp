#include "settings.h"
#include <Preferences.h>
#include "config.h"

namespace settings {

Settings cfg;
static Preferences prefs;
static const uint32_t MAGIC = 0x47470003;  // "GG" v3 – starší nastavení se nahradí výchozím

static const KeyInfo KEYS[CFG_KEY_COUNT + 1] = {
  {"-", 0, 0, 0},
  {"blink_ms",      660, 300, 2000},
  {"blink_off_s",    30,   0,  600},
  {"bat_cal",      1000, 800, 1200},
  {"bat_low",      3300, 2800, 4000},
  {"bat_crit",     3100, 2700, 4000},
  {"brake_flash",     1,   0,    1},
  {"tail_front",      1,   0,    1},
  {"led_bright",    128,   0,  255},
  {"led_mode",        0,   0, LED_MODE_COUNT - 1},
  {"light_boot",      0,   0,    2},
  {"lux_on",         40,   1, 2000},
  {"lux_off",       120,   2, 5000},
  {"auto_brake",      1,   0,    1},
  {"brake_mg",      150,  50,  600},
  {"mpu_axis",        0,   0,    5},
  {"auto_off_min",   15,   0,  240},
  {"bat_off",      3000, 2500, 3600},
  {"chg_start",      25,   5,   90},
  {"chg_stop",       95,  20,  100},
  {"chg_mode",        0,   0,    1},
  {"wheel_mm",      800, 200, 3000},
  {"rear_ppr",       15,   1,  120},
  {"front_ppr",      15,   1,  120},
  {"drv_allowed",     0,   0,    1},
  {"drv_assist",     60,   0,  100},
  {"drv_max_kmh",    25,   5,   60},
  {"drv_min_bat",    20,   5,   80},
  {"drv_kp",         80,   0,  500},
  {"drv_ki",         40,   0,  500},
  {"thr_min_mv",      0,   0, 3300},
  {"thr_max_mv",      0,   0, 3300},
  {"dac_idle",       80,   0,  500},
  {"dac_start",     110,   0,  500},
  {"dac_max",       420,   0,  500},
};

static void defaults() {
  for (int k = 0; k <= CFG_KEY_COUNT; k++) cfg.v[k] = KEYS[k].def;
  cfg.passkey = 0;
}

void load() {
  prefs.begin("gogen", true);
  bool valid = prefs.getUInt("magic", 0) == MAGIC &&
               prefs.getBytesLength("cfg") == sizeof(Settings);
  if (valid) prefs.getBytes("cfg", &cfg, sizeof(Settings));
  prefs.end();
  if (!valid) {
    defaults();
    return;
  }
  // hodnoty mimo rozsah (např. po změně firmwaru) vrátit na výchozí
  for (int k = 1; k <= CFG_KEY_COUNT; k++)
    if (cfg.v[k] < KEYS[k].min || cfg.v[k] > KEYS[k].max) cfg.v[k] = KEYS[k].def;
}

void save() {
  prefs.begin("gogen", false);
  prefs.putUInt("magic", MAGIC);
  prefs.putBytes("cfg", &cfg, sizeof(Settings));
  prefs.end();
}

void factoryReset() {
  prefs.begin("gogen", false);
  uint32_t odo = prefs.getUInt("odo", 0);   // ujeté km tovární reset nemaže
  prefs.clear();
  prefs.putUInt("odo", odo);
  prefs.end();
  defaults();
}

uint32_t passkey() {
  return (cfg.passkey >= 1 && cfg.passkey <= 999999) ? cfg.passkey : BLE_DEFAULT_PASSKEY;
}

const KeyInfo *info(uint8_t key) { return (key >= 1 && key <= CFG_KEY_COUNT) ? &KEYS[key] : nullptr; }

int keyByName(const char *name) {
  for (int k = 1; k <= CFG_KEY_COUNT; k++)
    if (!strcmp(name, KEYS[k].name)) return k;
  return -1;
}

bool setKey(uint8_t key, uint16_t v) {
  const KeyInfo *ki = info(key);
  if (!ki || v < ki->min || v > ki->max) return false;
  uint16_t *c = cfg.v;
  // vzájemné vazby
  switch (key) {
    case CFG_BATT_CRIT:  if (v > c[CFG_BATT_LOW]) return false; break;
    case CFG_BATT_LOW:   if (v < c[CFG_BATT_CRIT]) return false; break;
    case CFG_BATT_OFF:   if (v > c[CFG_BATT_CRIT]) return false; break;
    case CFG_LUX_ON:     if (v >= c[CFG_LUX_OFF]) return false; break;
    case CFG_LUX_OFF:    if (v <= c[CFG_LUX_ON]) return false; break;
    case CFG_CHG_START:  if (v + 5 > c[CFG_CHG_STOP]) return false; break;
    case CFG_CHG_STOP:   if (v < c[CFG_CHG_START] + 5) return false; break;
    case CFG_DAC_IDLE:   if (v > c[CFG_DAC_START]) return false; break;
    case CFG_DAC_START:  if (v < c[CFG_DAC_IDLE] || v >= c[CFG_DAC_MAX]) return false; break;
    case CFG_DAC_MAX:    if (v <= c[CFG_DAC_START]) return false; break;
  }
  c[key] = v;
  return true;
}

uint32_t loadOdometer() {
  prefs.begin("gogen", true);
  uint32_t m = prefs.getUInt("odo", 0);
  prefs.end();
  return m;
}

void saveOdometer(uint32_t meters) {
  prefs.begin("gogen", false);
  prefs.putUInt("odo", meters);
  prefs.end();
}

}  // namespace settings
