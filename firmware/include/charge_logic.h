// =====================================================================
//  GoGEN OS – řízení dobíjení sekundární baterie z předního kola
//  Při ≤ startPct sepne CHG_EN, nabíjí do stopPct, pak čeká na pokles.
//  ESP nabíjení jen POVOLUJE: napětí 42 V musí hlídat CC/CV měnič a BMS.
//  Čistá logika bez Arduina → testovatelné na PC.
// =====================================================================
#pragma once
#include <stdint.h>

namespace chg {

enum class Mode : uint8_t { AUTO = 0, OFF = 1, FORCE = 2 };
enum class State : uint8_t { IDLE = 0, WAITING = 1, CHARGING = 2, FULL = 3, FAULT = 4 };

enum Reason : uint8_t {          // proč se právě nenabíjí (bity)
  R_NONE         = 0,
  R_NO_GENERATOR = 1 << 0,       // kolo stojí / generátor pod minimem
  R_GEN_OVERVOLT = 1 << 1,       // generátor nad vstupem měniče
  R_FRONT_DRIVE  = 1 << 2,       // přední motor je připojený k pohonu
  R_SENSOR_FAULT = 1 << 3,       // měření baterie nespolehlivé
  R_MODE_OFF     = 1 << 4,
  R_MIN_OFF_TIME = 1 << 5,       // ochrana relé proti cvakání
};

enum Fault : uint8_t {
  FT_NONE        = 0,
  FT_OVERVOLTAGE = 1,            // baterie ≥ faultV – nabíjení nejde vypnout?
  FT_TIMEOUT     = 2,            // nabíjí příliš dlouho
  FT_NO_RISE     = 3,            // sepnuto, generátor jede, napětí neroste
};

struct Config {
  uint8_t  startPct      = 25;
  uint8_t  stopPct       = 95;
  float    stopV         = 41.5f;   // tvrdá mez (4,15 V/čl.)
  float    faultV        = 42.2f;   // porucha s latchem
  float    genMinV       = 20.0f;
  float    genMaxV       = 58.0f;
  uint32_t genDebounceMs = 2000;
  uint32_t minOnMs       = 10000;
  uint32_t minOffMs      = 10000;
  uint32_t noRiseMs      = 600000;  // 10 min bez nárůstu o riseV
  float    riseV         = 0.1f;
  uint32_t maxChargeMs   = 6UL * 3600UL * 1000UL;
};

struct Inputs {
  float   battV;          // filtrované napětí baterie
  uint8_t battPct;
  bool    battValid;
  float   genV;           // napětí generátoru (nebo genMinV+1, když se neměří)
  bool    frontDrive;     // fáze připojené k pohonu
};

class Charger {
 public:
  Config cfg;

  void setMode(Mode m) {
    mode_ = m;
    if (m == Mode::FORCE) { wantCharge_ = true; reachedFull_ = false; }
  }
  Mode mode() const { return mode_; }
  State state() const { return state_; }
  uint8_t reasons() const { return reasons_; }
  Fault fault() const { return fault_; }
  bool output() const { return out_; }
  uint32_t chargingMs(uint32_t now) const { return out_ ? now - onAt_ : 0; }

  void resetFault() {
    if (state_ == State::FAULT) { state_ = State::IDLE; fault_ = FT_NONE; }
  }

  bool update(const Inputs &in, uint32_t now) {
    reasons_ = R_NONE;

    // generátor stabilně v rozsahu?
    bool genInRange = in.genV >= cfg.genMinV && in.genV <= cfg.genMaxV;
    if (genInRange != genRaw_) { genRaw_ = genInRange; genSince_ = now; }
    bool genOk = genRaw_ && now - genSince_ >= cfg.genDebounceMs;
    if (in.genV > cfg.genMaxV) reasons_ |= R_GEN_OVERVOLT;
    else if (!genOk) reasons_ |= R_NO_GENERATOR;
    if (in.frontDrive) reasons_ |= R_FRONT_DRIVE;
    if (!in.battValid) reasons_ |= R_SENSOR_FAULT;
    if (mode_ == Mode::OFF) reasons_ |= R_MODE_OFF;

    // poruchy s latchem
    if (in.battValid && in.battV >= cfg.faultV) trip(FT_OVERVOLTAGE, now);
    if (out_ && now - onAt_ > cfg.maxChargeMs) trip(FT_TIMEOUT, now);
    if (out_ && genOk && now - riseRefAt_ > cfg.noRiseMs) {
      if (in.battV < riseRefV_ + cfg.riseV) trip(FT_NO_RISE, now);
      riseRefV_ = in.battV; riseRefAt_ = now;
    }
    if (out_ && in.battV >= riseRefV_ + cfg.riseV) { riseRefV_ = in.battV; riseRefAt_ = now; }

    if (state_ == State::FAULT) { setOut(false, now, true); return out_; }

    // hystereze cíle
    if (in.battValid) {
      bool full = in.battPct >= cfg.stopPct || in.battV >= cfg.stopV;
      if (full) { wantCharge_ = false; reachedFull_ = true; }
      else if (in.battPct <= cfg.startPct) { wantCharge_ = true; reachedFull_ = false; }
    }
    if (mode_ == Mode::FORCE && !reachedFull_) wantCharge_ = true;

    bool blocked = (reasons_ & (R_NO_GENERATOR | R_GEN_OVERVOLT | R_FRONT_DRIVE |
                                R_SENSOR_FAULT | R_MODE_OFF)) != 0;
    bool hard = (reasons_ & (R_GEN_OVERVOLT | R_FRONT_DRIVE | R_SENSOR_FAULT | R_MODE_OFF)) != 0 ||
                (in.battValid && in.battV >= cfg.stopV);
    bool target = wantCharge_ && !blocked;

    if (target && !out_) {
      if (now - offAt_ < cfg.minOffMs && everOff_) reasons_ |= R_MIN_OFF_TIME;
      else setOut(true, now, false);
    } else if (!target && out_) {
      // bezpečnostní důvody vypínají hned, ostatní až po minOnMs
      if (hard || now - onAt_ >= cfg.minOnMs) setOut(false, now, false);
    }

    if (out_) state_ = State::CHARGING;
    else if (wantCharge_) state_ = State::WAITING;
    else state_ = reachedFull_ ? State::FULL : State::IDLE;
    if (mode_ == Mode::FORCE && state_ != State::CHARGING && !wantCharge_) mode_ = Mode::AUTO;
    return out_;
  }

 private:
  Mode mode_ = Mode::AUTO;
  State state_ = State::IDLE;
  Fault fault_ = FT_NONE;
  uint8_t reasons_ = R_NONE;
  bool out_ = false, wantCharge_ = false, reachedFull_ = false, everOff_ = false;
  bool genRaw_ = false;
  uint32_t genSince_ = 0, onAt_ = 0, offAt_ = 0, riseRefAt_ = 0;
  float riseRefV_ = 0;

  void trip(Fault f, uint32_t now) {
    if (state_ == State::FAULT) return;
    fault_ = f; state_ = State::FAULT;
    setOut(false, now, true);
  }
  void setOut(bool on, uint32_t now, bool force) {
    (void)force;
    if (on == out_) return;
    out_ = on;
    if (on) { onAt_ = now; riseRefAt_ = now; riseRefV_ = 0; }
    else { offAt_ = now; everOff_ = true; }
  }
};

}  // namespace chg
