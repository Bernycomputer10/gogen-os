#include "drive.h"
#include "config.h"
#include "protocol.h"
#include "settings.h"
#include "battery.h"
#include "sensors.h"
#include "speed.h"
#include "vehicle.h"
#include "charge.h"

namespace drive {

static drv::Drive d;
static bool userOn = false;
static bool halted = false;
static float thr = -1;
static uint16_t thrMv = 0;

[[maybe_unused]] static void outputsSafe() {
  digitalWrite(PIN_PHASE_EN, LOW);
  digitalWrite(PIN_FRONT_BRK, HIGH);   // brzda předního kontroléru aktivní
}

void begin() {
#if FEATURE_DRIVE
  outputsSafe();
  pinMode(PIN_PHASE_EN, OUTPUT);
  pinMode(PIN_FRONT_BRK, OUTPUT);
  outputsSafe();
  analogSetPinAttenuation(PIN_THROTTLE, ADC_11db);
  pinMode(PIN_THROTTLE, INPUT);
  userOn = false;                      // po startu vždy vypnuto, zapíná se ručně
#endif
}

void forceOff() {
  halted = true;
  userOn = false;
#if FEATURE_DRIVE
  sensors::dacWrite(settings::get(CFG_DAC_IDLE) / 100.0f);
  delay(20);
  outputsSafe();
#endif
}

void tick(uint32_t now) {
#if FEATURE_DRIVE
  if (halted) return;
  uint32_t sum = 0;
  for (int i = 0; i < 4; i++) sum += analogReadMilliVolts(PIN_THROTTLE);
  thrMv = sum / 4;
  thr = drv::throttleFromMv(thrMv, settings::get(CFG_THR_MIN_MV), settings::get(CFG_THR_MAX_MV));

  drv::Config &c = d.cfg;
  c.assist = settings::get(CFG_DRV_ASSIST) / 100.0f;
  c.maxKmh = settings::get(CFG_DRV_MAX_KMH);
  c.minBattPct = settings::get(CFG_DRV_MIN_BATT);
  c.kp = settings::get(CFG_DRV_KP) / 100.0f;
  c.ki = settings::get(CFG_DRV_KI) / 100.0f;

  drv::Inputs in;
  // fáze se nepřipojí, dokud je sepnuté nabíjení (charge ho vypne, jakmile je pohon zapnutý)
  in.enabled = userOn && settings::get(CFG_DRV_ALLOWED) && !charge::output();
  in.dacOk = sensors::dacOk();
  in.throttle = thr;
  in.brake = vehicle::brakeInput();
  in.rearKmh = speed::rearKmh();   in.rearValid = speed::rearValid();
  in.frontKmh = speed::frontKmh(); in.frontValid = speed::frontValid();
  in.battPct = battery::percent(); in.battValid = battery::valid();

  const drv::Outputs &o = d.update(in, now);
  float v = drv::dacVolts(o.dac, settings::get(CFG_DAC_IDLE) / 100.0f,
                          settings::get(CFG_DAC_START) / 100.0f, settings::get(CFG_DAC_MAX) / 100.0f);
  sensors::dacWrite(v);
  digitalWrite(PIN_FRONT_BRK, o.frontBrake ? HIGH : LOW);
  digitalWrite(PIN_PHASE_EN, o.phase ? HIGH : LOW);
  if (d.state() == drv::State::FAULT) userOn = false;
#else
  (void)now;
#endif
}

void setEnabled(uint8_t arg) {
#if FEATURE_DRIVE
  bool on = arg == ARG_TOGGLE ? !userOn : arg == ARG_ON;
  userOn = on && settings::get(CFG_DRV_ALLOWED) && !halted;
#else
  (void)arg;
#endif
}

bool enabled() { return userOn; }
void resetFault() { d.resetFault(); }

uint16_t calibrateThrottle(uint8_t which) {
#if FEATURE_DRIVE
  if (d.phaseClosed()) return 0;     // kalibrace jen s odpojeným pohonem
  uint32_t sum = 0;
  for (int i = 0; i < 64; i++) sum += analogReadMilliVolts(PIN_THROTTLE);
  uint16_t mv = sum / 64;
  if (!settings::setKey(which ? CFG_THR_MAX_MV : CFG_THR_MIN_MV, mv)) return 0;
  return mv;
#else
  (void)which;
  return 0;
#endif
}

drv::State state() { return d.state(); }
uint8_t reasons() { return d.reasons(); }
drv::Fault fault() { return d.fault(); }
bool phaseClosed() { return d.phaseClosed(); }
float throttle() { return thr; }
uint16_t throttleMv() { return thrMv; }
float output() { return d.out().dac; }

}  // namespace drive
