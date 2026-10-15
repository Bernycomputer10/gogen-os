// =====================================================================
//  GoGEN OS – automatická světla (BH1750) a automatické brzdové světlo
//  (MPU6050). Čistá logika bez Arduina → testovatelné na PC.
// =====================================================================
#pragma once
#include <stdint.h>
#include <math.h>

namespace autologic {

// ---------------------------------------------------------------------
//  AutoLight – hystereze + zpoždění, aby světla neblikala pod stromy
// ---------------------------------------------------------------------
struct AutoLight {
  uint16_t luxOn = 40;          // pod tím rozsvítit
  uint16_t luxOff = 120;        // nad tím zhasnout
  uint32_t onDelayMs = 2000;    // jak dlouho musí být tma
  uint32_t offDelayMs = 10000;  // jak dlouho musí být světlo

  bool lit = false;
  bool primed = false;
  uint32_t since = 0;           // od kdy platí podmínka pro změnu
  bool pending = false;

  // lux < 0 = senzor neodpovídá → bezpečně svítit
  bool update(float lux, uint32_t now) {
    if (lux < 0) { lit = true; pending = false; return lit; }
    if (!primed) { primed = true; lit = lux < luxOn; pending = false; return lit; }
    bool want = lit ? !(lux > luxOff) : (lux < luxOn);
    if (want == lit) { pending = false; return lit; }
    if (!pending) { pending = true; since = now; }
    uint32_t need = lit ? offDelayMs : onDelayMs;
    if (now - since >= need) { lit = want; pending = false; }
    return lit;
  }
};

// ---------------------------------------------------------------------
//  AutoBrake – zpomalení v ose jízdy proti klouzavému „nulovému“ bodu
//  (kompenzuje sklon kopce a montáž senzoru)
// ---------------------------------------------------------------------
struct AutoBrake {
  float onG = 0.15f;            // práh sepnutí
  float offG = 0.08f;           // práh uvolnění
  float emergencyG = 0.45f;     // prudké brzdění
  uint32_t onMs = 120;          // musí trvat
  uint32_t offMs = 300;
  uint32_t maxOnMs = 10000;     // zaseknutý stav → znovu nulovat
  float lpTau = 0.05f;          // filtr signálu [s]
  float baseTau = 3.0f;         // jak rychle se nulový bod přizpůsobí [s]

  bool active = false;
  bool emergency = false;
  bool primed = false;
  float filt = 0, base = 0;
  uint32_t condSince = 0, activeSince = 0, lastMs = 0;
  bool cond = false;

  float decel() const { return base - filt; }   // kladné = brzdí

  // a = zrychlení v ose jízdy [g] (dopředu kladné), ok = senzor platný
  bool update(float a, bool ok, uint32_t now) {
    if (!ok) { active = emergency = false; primed = false; return false; }
    if (!primed) { filt = base = a; primed = true; lastMs = now; return false; }
    float dt = (now - lastMs) / 1000.0f;
    lastMs = now;
    if (dt <= 0) return active;
    if (dt > 0.5f) dt = 0.5f;
    filt += (a - filt) * (dt / (lpTau + dt));
    // nulový bod se učí jen mimo brzdění
    if (!active) base += (filt - base) * (dt / (baseTau + dt));

    float d = decel();
    bool c = active ? d > offG : d > onG;
    if (c != cond) { cond = c; condSince = now; }

    if (!active && cond && now - condSince >= onMs) { active = true; activeSince = now; }
    else if (active && !cond && now - condSince >= offMs) { active = false; }

    if (active && now - activeSince > maxOnMs) {
      // dlouhé „brzdění“ = nejspíš změna sklonu nebo posunutý senzor
      active = false; base = filt; cond = false; condSince = now;
    }
    emergency = active && d > emergencyG;
    return active;
  }
};

// osa 0..5 = +X −X +Y −Y +Z −Z
inline float axisValue(uint8_t axis, float x, float y, float z) {
  float v = axis < 2 ? x : axis < 4 ? y : z;
  return (axis & 1) ? -v : v;
}

}  // namespace autologic
