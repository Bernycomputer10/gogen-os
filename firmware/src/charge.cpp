#include "charge.h"
#include "config.h"
#include "protocol.h"
#include "settings.h"
#include "battery.h"

namespace charge {

static chg::Charger ch;
static float genV = 0;
static bool halted = false;

void begin() {
  digitalWrite(PIN_CHG_EN, LOW);
  pinMode(PIN_CHG_EN, OUTPUT);
  digitalWrite(PIN_CHG_EN, LOW);
  analogSetPinAttenuation(PIN_GEN_SENSE, ADC_11db);
  pinMode(PIN_GEN_SENSE, INPUT);
  ch.setMode(settings::get(CFG_CHG_MODE) ? chg::Mode::OFF : chg::Mode::AUTO);
}

void forceOff() {
  halted = true;
  digitalWrite(PIN_CHG_EN, LOW);
}

void tick(uint32_t now, bool frontDriveConnected) {
  if (halted) return;
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(PIN_GEN_SENSE);
  float v = sum / 8 / 1000.0f * (GEN_R1_OHM + GEN_R2_OHM) / GEN_R2_OHM;
  genV += (v - genV) * 0.3f;

  ch.cfg.startPct = settings::get(CFG_CHG_START);
  ch.cfg.stopPct = settings::get(CFG_CHG_STOP);

  chg::Inputs in{battery::volts(), battery::percent(), battery::valid(), genV, frontDriveConnected};
  bool on = ch.update(in, now);
  digitalWrite(PIN_CHG_EN, on ? HIGH : LOW);
}

void setMode(chg::Mode m) { ch.setMode(m); }
void resetFault() { ch.resetFault(); }
chg::State state() { return ch.state(); }
chg::Mode mode() { return ch.mode(); }
uint8_t reasons() { return ch.reasons(); }
chg::Fault fault() { return ch.fault(); }
bool output() { return ch.output() && !halted; }
float genVolts() { return genV; }
uint16_t chargingMin(uint32_t now) { return ch.chargingMs(now) / 60000; }

}  // namespace charge
