// =====================================================================
//  GoGEN OS – přední pohon ze sekundární baterie
//  Plyn jezdce (z primáru přes optočlen) → MCP4725 → přední kontrolér.
//  PI regulátor dorovnává otáčky předního kola na zadní.
//
//  Bezpečnostní zásady:
//   - výstup nikdy nepřekročí plyn jezdce (přední kolo neumí „předjet“ jezdce)
//   - brzda = okamžitě klidový plyn + brzdový vstup předního kontroléru
//   - relé fází se spíná/rozpíná jen s klidovým plynem
//   - nad maxKmh se fáze odpojí (48V motor by jinak nabíjel 36V baterii)
//   - ztráta DAC, nesmyslný signál plynu nebo samovolný rozběh = porucha,
//     fáze se odpojí a pohon čeká na ruční reset
//  Čistá logika bez Arduina → testovatelné na PC.
// =====================================================================
#pragma once
#include <stdint.h>

namespace drv {

enum class State : uint8_t { OFF = 0, ARMING = 1, READY = 2, ACTIVE = 3, RELEASING = 4, FAULT = 5 };

enum Reason : uint8_t {
  R_NONE      = 0,
  R_DISABLED  = 1 << 0,   // vypnuto uživatelem / v nastavení
  R_NO_DAC    = 1 << 1,   // MCP4725 neodpovídá
  R_BATT_LOW  = 1 << 2,   // sekundární baterie pod minimem
  R_OVERSPEED = 1 << 3,   // nad maxKmh → fáze odpojeny
  R_THROTTLE  = 1 << 4,   // plyn držený při zapínání nebo bez signálu (primár vypnutý)
  R_BRAKE     = 1 << 5,
  R_SLIP      = 1 << 6,   // přední kolo prokluzuje
  R_NO_SYNC   = 1 << 7,   // chybí otáčky → jen podíl plynu bez regulace
};

enum Fault : uint8_t {
  FT_NONE     = 0,
  FT_THROTTLE = 1,        // signál plynu mimo rozsah (přerušený / zkrat)
  FT_DAC      = 2,        // DAC zmizel při sepnutých fázích
  FT_RUNAWAY  = 3,        // přední kolo zrychluje bez příkazu
};

struct Config {
  float    assist        = 0.6f;    // podíl plynu pro přední motor
  float    maxKmh        = 25.0f;   // nad tím odpojit fáze
  float    rearmHystKmh  = 5.0f;    // znovu připojit až pod maxKmh − hyst
  uint8_t  minBattPct    = 20;
  uint8_t  battHystPct   = 5;
  float    kp            = 0.8f;
  float    ki            = 0.4f;    // [1/s]
  float    integLimit    = 0.4f;
  float    syncMinKmh    = 5.0f;    // pod tím jen podíl plynu
  float    deadband      = 0.05f;
  float    rampUpPerS    = 1.5f;    // 0 → 100 % za ~0,7 s
  float    rampDownPerS  = 8.0f;
  uint32_t armMs         = 250;     // relé dosedne
  uint32_t releaseMs     = 150;     // klidový plyn před rozepnutím relé
  float    slipRatio     = 1.25f;
  float    slipKmh       = 3.0f;
  uint32_t slipMs        = 300;
  float    runawayKmh    = 8.0f;    // přední o tolik rychlejší při klidovém plynu
  uint32_t runawayMs     = 1500;
};

struct Inputs {
  bool    enabled;        // uživatel zapnul + povoleno v nastavení
  bool    dacOk;
  float   throttle;       // 0..1, záporné = chybný signál
  bool    brake;
  float   rearKmh;  bool rearValid;
  float   frontKmh; bool frontValid;
  uint8_t battPct;  bool battValid;
};

struct Outputs {
  float dac = 0;          // 0..1 (0 = klidový plyn)
  bool  phase = false;    // relé fází
  bool  frontBrake = true;
};

class Drive {
 public:
  Config cfg;

  State state() const { return st_; }
  uint8_t reasons() const { return reasons_; }
  Fault fault() const { return fault_; }
  const Outputs &out() const { return o_; }
  bool phaseClosed() const { return o_.phase; }

  void resetFault() {
    if (st_ == State::FAULT) { st_ = State::OFF; fault_ = FT_NONE; }
  }

  const Outputs &update(const Inputs &in, uint32_t now) {
    float dt = primed_ ? (now - last_) / 1000.0f : 0;
    primed_ = true; last_ = now;
    if (dt > 0.2f) dt = 0.2f;
    reasons_ = R_NONE;

    float v = 0;
    if (in.rearValid && in.rearKmh > v) v = in.rearKmh;
    if (in.frontValid && in.frontKmh > v) v = in.frontKmh;

    // baterie s hysterezí
    if (!in.battValid) battOk_ = false;
    else if (battOk_ && in.battPct < cfg.minBattPct) battOk_ = false;
    else if (!battOk_ && in.battPct >= cfg.minBattPct + cfg.battHystPct) battOk_ = true;

    // rychlostní limit s hysterezí
    if (v > cfg.maxKmh) overspeed_ = true;
    else if (v <= cfg.maxKmh - cfg.rearmHystKmh) overspeed_ = false;

    if (!in.enabled) reasons_ |= R_DISABLED;
    if (!in.dacOk) reasons_ |= R_NO_DAC;
    if (!battOk_) reasons_ |= R_BATT_LOW;
    if (overspeed_) reasons_ |= R_OVERSPEED;
    if (in.brake) reasons_ |= R_BRAKE;

    bool armed = st_ == State::ARMING || st_ == State::READY || st_ == State::ACTIVE;

    // poruchy: chybný plyn během jízdy = porucha, v klidu jen odpojit
    bool thrBad = in.throttle < 0;
    if (thrBad) reasons_ |= R_THROTTLE;
    if (thrBad && st_ == State::ACTIVE && o_.dac > 0.001f) trip(FT_THROTTLE, now);
    if (armed && !in.dacOk) trip(FT_DAC, now);
    bool runaway = armed && o_.dac <= 0.001f && in.frontValid && in.rearValid &&
                   in.frontKmh > in.rearKmh + cfg.runawayKmh;
    if (runaway != runawayCond_) { runawayCond_ = runaway; runawaySince_ = now; }
    if (runaway && now - runawaySince_ >= cfg.runawayMs) trip(FT_RUNAWAY, now);

    bool allowed = in.enabled && in.dacOk && battOk_ && !overspeed_ && !thrBad &&
                   st_ != State::FAULT && st_ != State::RELEASING;
    float thr = in.throttle < cfg.deadband ? 0 : (in.throttle - cfg.deadband) / (1 - cfg.deadband);
    if (thr > 1) thr = 1;

    switch (st_) {
      case State::OFF:
        o_.dac = 0; o_.phase = false;
        if (allowed) {
          if (thr > 0) reasons_ |= R_THROTTLE;
          else { st_ = State::ARMING; since_ = now; o_.phase = true; }
        }
        break;
      case State::ARMING:
        o_.dac = 0; o_.phase = true;
        if (!allowed) startRelease(now);
        else if (now - since_ >= cfg.armMs) st_ = State::READY;
        break;
      case State::READY:
        o_.dac = 0; o_.phase = true; integ_ = 0; slip_ = false;
        if (!allowed) startRelease(now);
        else if (thr > 0 && !in.brake) st_ = State::ACTIVE;
        break;
      case State::ACTIVE:
        if (!allowed) { o_.dac = 0; startRelease(now); break; }
        if (in.brake || thr <= 0) { o_.dac = 0; integ_ = 0; st_ = State::READY; break; }
        control(in, thr, dt, now);
        break;
      case State::RELEASING:
        o_.dac = 0;
        if (now - since_ >= cfg.releaseMs) { o_.phase = false; st_ = pendingFault_ ? State::FAULT : State::OFF; }
        break;
      case State::FAULT:
        o_.dac = 0; o_.phase = false;
        break;
    }
    o_.frontBrake = in.brake || st_ != State::ACTIVE;
    return o_;
  }

 private:
  State st_ = State::OFF;
  Fault fault_ = FT_NONE;
  uint8_t reasons_ = R_NONE;
  Outputs o_;
  bool primed_ = false, battOk_ = false, overspeed_ = false, slip_ = false;
  bool slipCond_ = false, runawayCond_ = false, pendingFault_ = false;
  uint32_t last_ = 0, since_ = 0, slipSince_ = 0, runawaySince_ = 0;
  float integ_ = 0;

  void startRelease(uint32_t now) {
    o_.dac = 0;
    if (st_ == State::RELEASING || st_ == State::FAULT) return;
    if (!o_.phase) { st_ = pendingFault_ ? State::FAULT : State::OFF; return; }
    st_ = State::RELEASING; since_ = now;
  }

  void trip(Fault f, uint32_t now) {
    if (fault_ != FT_NONE) return;
    fault_ = f; pendingFault_ = true; o_.dac = 0;
    if (st_ == State::OFF) { st_ = State::FAULT; return; }
    startRelease(now);
  }

  void control(const Inputs &in, float thr, float dt, uint32_t now) {
    float target = thr * cfg.assist;
    bool sync = in.rearValid && in.frontValid && in.rearKmh >= cfg.syncMinKmh;
    if (!sync) { reasons_ |= R_NO_SYNC; integ_ = 0; }
    else {
      float err = (in.rearKmh - in.frontKmh) / in.rearKmh;
      float u = target + cfg.kp * err + integ_;
      bool satHi = u >= thr && err > 0, satLo = u <= 0 && err < 0;
      if (!satHi && !satLo) integ_ += cfg.ki * err * dt;
      if (integ_ > cfg.integLimit) integ_ = cfg.integLimit;
      if (integ_ < -cfg.integLimit) integ_ = -cfg.integLimit;
      target += cfg.kp * err + integ_;
    }

    // prokluz předního kola
    bool slip = in.frontValid && in.rearValid &&
                in.frontKmh > in.rearKmh * cfg.slipRatio + cfg.slipKmh;
    if (slip != slipCond_) { slipCond_ = slip; slipSince_ = now; }
    if (slip && now - slipSince_ >= cfg.slipMs) slip_ = true;
    if (slip_ && in.frontKmh <= in.rearKmh * 1.1f + 1.0f) slip_ = false;
    if (slip_) { reasons_ |= R_SLIP; target = 0; integ_ = 0; }

    if (target > thr) target = thr;   // nikdy víc než jezdec
    if (target < 0) target = 0;

    float up = cfg.rampUpPerS * dt, down = cfg.rampDownPerS * dt;
    if (target > o_.dac) o_.dac = (target - o_.dac > up) ? o_.dac + up : target;
    else o_.dac = (o_.dac - target > down) ? o_.dac - down : target;
    if (o_.dac > thr) o_.dac = thr;
  }
};

// Převod 0..1 → napětí pro kontrolér (0 = klidové napětí pod prahem reakce)
inline float dacVolts(float frac, float idleV, float startV, float maxV) {
  if (frac <= 0.0005f) return idleV;
  if (frac > 1) frac = 1;
  return startV + frac * (maxV - startV);
}

// ADC mV → plyn 0..1, záporné = signál mimo rozsah (přerušený vodič / zkrat)
inline float throttleFromMv(float mv, float minMv, float maxMv) {
  float span = maxMv - minMv;
  if (span < 200) return -1;                      // nekalibrováno
  float lowSlack = 0.25f * span;
  if (lowSlack > 0.5f * minMv) lowSlack = 0.5f * minMv;  // „živá nula“ odhalí přerušení
  if (minMv >= 100 && mv < minMv - lowSlack) return -1;
  if (mv > maxMv + 0.15f * span) return -1;
  float t = (mv - minMv) / span;
  return t < 0 ? 0 : t > 1 ? 1 : t;
}

}  // namespace drv
