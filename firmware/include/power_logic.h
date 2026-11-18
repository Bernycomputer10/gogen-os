// =====================================================================
//  GoGEN OS – automatické vypnutí a ochrana článků proti hlubokému vybití.
//  Čistá logika bez Arduina → testovatelné na PC.
// =====================================================================
#pragma once
#include <stdint.h>

namespace pwr {

enum class OffReason : uint8_t { NONE = 0, USER = 1, IDLE = 2, UNDERVOLT = 3 };

struct AutoOff {
  uint16_t idleMin = 15;          // 0 = nikdy
  float    cutoffV = 30.0f;       // 3,0 V/čl.
  uint32_t cutoffMs = 30000;      // pod cutoffV musí trvat (ne propad při zátěži)

  uint32_t lastActivity = 0;
  uint32_t lowSince = 0;
  bool low = false;

  void activity(uint32_t now) { lastActivity = now; }

  // busy = něco se děje (jízda, nabíjení, BLE, OTA) → počítadlo stojí
  OffReason update(uint32_t now, bool busy, float battV, bool battValid) {
    if (busy) lastActivity = now;
    bool isLow = battValid && battV > 5.0f && battV < cutoffV;
    if (isLow != low) { low = isLow; lowSince = now; }
    if (low && now - lowSince >= cutoffMs) return OffReason::UNDERVOLT;
    if (idleMin && now - lastActivity >= (uint32_t)idleMin * 60000UL) return OffReason::IDLE;
    return OffReason::NONE;
  }

  // sekundy do vypnutí z nečinnosti, 0xFFFF = vypnuto
  uint16_t secondsLeft(uint32_t now) const {
    if (!idleMin) return 0xFFFF;
    uint32_t lim = (uint32_t)idleMin * 60000UL, el = now - lastActivity;
    return el >= lim ? 0 : (uint16_t)((lim - el) / 1000);
  }
};

// perioda mezi pulzy hallu [µs] → km/h
inline float kmhFromPeriod(uint32_t periodUs, uint16_t pulsesPerRev, uint16_t wheelMm) {
  if (!periodUs || !pulsesPerRev) return 0;
  float revPerS = 1e6f / periodUs / pulsesPerRev;
  return revPerS * wheelMm / 1000.0f * 3.6f;
}

}  // namespace pwr
