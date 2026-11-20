#include "charger.h"
#include <Preferences.h>
#include "config.h"
#include "protocol.h"

namespace charger {

struct Stored { uint32_t magic; uint8_t mode, target, limitDeciA, pad; };
static const uint32_t MAGIC = 0x43480001;

static Stored cfg = {MAGIC, CHG_MODE_AUTO, 90, 20, 0};
static Preferences prefs;
static uint8_t ph = CHG_IDLE;
static bool on = false;
static uint32_t startMs = 0, lastBleMs = 0, elapsedAtStop = 0;
static float amps = 0, lastV = 0;
static uint8_t lastPct = 0;

static void drive(bool v) {
  digitalWrite(PIN_CHARGE, (v == CHARGE_ACTIVE_HIGH) ? HIGH : LOW);
  on = v;
}

static void finish(uint8_t phase, uint32_t now) {
  if (on) elapsedAtStop = (now - startMs) / 60000UL;
  drive(false);
  ph = phase;
}

void begin() {
  drive(false);
  pinMode(PIN_CHARGE, OUTPUT);
  drive(false);                          // po startu vždy vypnuto, i když byl režim auto
#if CHARGE_HAS_SENSE
  analogSetPinAttenuation(PIN_CHARGE_SENSE, ADC_11db);
  pinMode(PIN_CHARGE_SENSE, INPUT);
#endif
  prefs.begin("gogen-chg", true);
  Stored s;
  if (prefs.getBytesLength("cfg") == sizeof(s)) {
    prefs.getBytes("cfg", &s, sizeof(s));
    if (s.magic == MAGIC && s.target >= 50 && s.target <= 100 && s.limitDeciA >= 5 && s.limitDeciA <= 50 &&
        s.mode <= CHG_MODE_MANUAL) cfg = s;
  }
  prefs.end();
}

void save() {
  prefs.begin("gogen-chg", false);
  prefs.putBytes("cfg", &cfg, sizeof(cfg));
  prefs.end();
}

static float readAmps() {
#if CHARGE_HAS_SENSE
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogReadMilliVolts(PIN_CHARGE_SENSE);
  return ((sum / 16.0f) - CHARGE_SENSE_ZERO_MV) / CHARGE_SENSE_MV_PER_A;
#else
  return 0;
#endif
}

void tick(uint32_t now, float v, uint8_t pct, bool ble) {
  if (ble) lastBleMs = now;
  lastV = v;
  lastPct = pct;
  bool vValid = v > 5.0f;
  float cellV = v / BAT_CELLS;

  if (on) {
    amps = readAmps();
    if (!vValid) finish(CHG_FAULT, now);                                   // slepé nabíjení nepovolíme
    else if (cellV >= CHARGE_MAX_CELL_V) finish(CHG_FULL, now);
    else if (pct >= cfg.target) finish(CHG_FULL, now);
#if CHARGE_HAS_SENSE
    else if (amps > cfg.limitDeciA / 10.0f) finish(CHG_FAULT, now);        // nadproud
#endif
    else if (now - startMs > (uint32_t)CHARGE_MAX_MIN * 60000UL) finish(CHG_FAULT, now);
    else if (cfg.mode == CHG_MODE_MANUAL && !ble && now - lastBleMs > (uint32_t)CHARGE_BLE_LOSS_S * 1000UL)
      finish(CHG_PAUSED, now);                                              // "dead man": telefon zmizel
    return;
  }

  amps = 0;
  if (cfg.mode == CHG_MODE_AUTO && (ph == CHG_IDLE || ph == CHG_FULL) && vValid &&
      pct < CHARGE_AUTO_START_PCT && pct < cfg.target && cellV < CHARGE_MAX_CELL_V) {
    startMs = now;
    ph = CHG_CHARGING;
    drive(true);
  }
}

void setMode(uint8_t m) {
  if (m > CHG_MODE_MANUAL) return;
  if (on) finish(CHG_IDLE, millis());
  ph = CHG_IDLE;                         // změna režimu maže i poruchu
  cfg.mode = m;
  save();
}

bool start() {
  if (cfg.mode != CHG_MODE_MANUAL || on) return false;
  if (lastV <= 5.0f || lastV / BAT_CELLS >= CHARGE_MAX_CELL_V) { ph = CHG_FAULT; return false; }
  if (lastPct >= cfg.target) { ph = CHG_FULL; return false; }
  startMs = lastBleMs = millis();
  ph = CHG_CHARGING;
  drive(true);
  return true;
}

void stop() {
  if (on) finish(cfg.mode == CHG_MODE_MANUAL ? CHG_PAUSED : CHG_IDLE, millis());
  else if (ph == CHG_FAULT || ph == CHG_FULL) ph = CHG_IDLE;
}

bool setKey(uint8_t key, uint16_t v) {
  switch (key) {
    case CHGKEY_TARGET:        if (v < 50 || v > 100) return false; cfg.target = v; break;
    case CHGKEY_LIMIT_DECI_A:  if (v < 5 || v > 50) return false;   cfg.limitDeciA = v; break;
    default: return false;
  }
  save();
  return true;
}

uint8_t mode() { return cfg.mode; }
uint8_t phase() { return ph; }
uint8_t target() { return cfg.target; }
uint8_t limitDeciA() { return cfg.limitDeciA; }
int16_t currentCentiA() { return (int16_t)(amps * 100.0f); }
uint16_t elapsedMin() { return on ? (millis() - startMs) / 60000UL : elapsedAtStop; }
bool hasSense() { return CHARGE_HAS_SENSE; }
bool outputOn() { return on; }

}  // namespace charger
