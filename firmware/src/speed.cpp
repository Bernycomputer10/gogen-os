#include "speed.h"
#include "config.h"
#include "protocol.h"
#include "settings.h"
#include "power_logic.h"

namespace speed {

#if FEATURE_SPEED
struct Chan {
  volatile uint32_t lastUs = 0;
  volatile uint32_t periodUs = 0;     // průměr ze 2 posledních period
  volatile uint32_t pulses = 0;
  volatile uint32_t prevPeriod = 0;
};
static Chan rear, front;

// Nejkratší rozumná perioda: 120 pulzů/ot, 80 km/h → ~170 µs. Kratší = rušení.
#define MIN_PERIOD_US 150

static void IRAM_ATTR edge(Chan &c) {
  uint32_t t = micros();
  uint32_t p = t - c.lastUs;
  if (p < MIN_PERIOD_US) return;
  c.lastUs = t;
  c.periodUs = c.prevPeriod ? (p + c.prevPeriod) / 2 : p;
  c.prevPeriod = p;
  c.pulses++;
}
static void IRAM_ATTR isrRear() { edge(rear); }
static void IRAM_ATTR isrFront() { edge(front); }
#endif

static float vRear = 0, vFront = 0;
static bool okRear = false, okFront = false;
static uint32_t odoBase = 0;            // uložené metry
static float distM = 0, tripDist = 0;   // od startu
static uint32_t lastSavedM = 0;
#if FEATURE_SPEED
static uint32_t lastRearPulses = 0, lastFrontPulses = 0;
static uint32_t everRear = 0, everFront = 0;
#endif

void begin() {
  odoBase = settings::loadOdometer();
  lastSavedM = odoBase;
#if FEATURE_SPEED
  pinMode(PIN_REAR_SPEED, INPUT_PULLUP);
  pinMode(PIN_FRONT_SPEED, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_REAR_SPEED), isrRear, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_FRONT_SPEED), isrFront, FALLING);
#endif
}

#if FEATURE_SPEED
static float chanKmh(Chan &c, uint16_t ppr, uint16_t wheel, uint32_t nowUs) {
  noInterrupts();
  uint32_t last = c.lastUs, per = c.periodUs;
  interrupts();
  uint32_t since = nowUs - last;
  if (!per || since > 600000UL) return 0;   // 0,6 s bez pulzu → stojí
  // zpomalování: když pulz dlouho nepřišel, perioda je aspoň „since“
  if (since > per) per = since;
  return pwr::kmhFromPeriod(per, ppr, wheel);
}
#endif

void update(uint32_t now) {
  (void)now;
#if FEATURE_SPEED
  uint16_t wheel = settings::get(CFG_WHEEL_MM);
  uint16_t rppr = settings::get(CFG_REAR_PPR), fppr = settings::get(CFG_FRONT_PPR);
  uint32_t us = micros();
  vRear = chanKmh(rear, rppr, wheel, us);
  vFront = chanKmh(front, fppr, wheel, us);

  uint32_t rp = rear.pulses, fp = front.pulses;
  if (rp != lastRearPulses) everRear = now;
  if (fp != lastFrontPulses) everFront = now;
  // signál platí, pokud kdy přišel pulz (kolo se točilo) – stojící kolo je 0 km/h
  okRear = everRear != 0;
  okFront = everFront != 0;

  // vzdálenost: přednostně zadní kolo
  float dm = 0;
  if (okRear) dm = (rp - lastRearPulses) * (wheel / 1000.0f) / rppr;
  else if (okFront) dm = (fp - lastFrontPulses) * (wheel / 1000.0f) / fppr;
  lastRearPulses = rp; lastFrontPulses = fp;
  distM += dm; tripDist += dm;

  // ukládat po 500 m (NVS zvládne statisíce zápisů)
  if (odometerM() - lastSavedM >= 500) saveNow();
#endif
}

bool rearValid() { return okRear; }
bool frontValid() { return okFront; }
float rearKmh() { return vRear; }
float frontKmh() { return vFront; }
float kmh() { return okRear ? vRear : vFront; }
bool known() { return okRear || okFront; }
uint32_t odometerM() { return odoBase + (uint32_t)distM; }
uint32_t tripM() { return (uint32_t)tripDist; }
void resetTrip() { tripDist = 0; }
void saveNow() {
  uint32_t m = odometerM();
  if (m == lastSavedM) return;
  settings::saveOdometer(m);
  lastSavedM = m;
}

}  // namespace speed
