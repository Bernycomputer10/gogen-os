#include "battery.h"
#include "config.h"
#include "settings.h"
#include "protocol.h"

namespace battery {

static float vFilt = 0;
static uint16_t lastAdcMv = 0;
static bool primed = false;

static const float DIVIDER = (BAT_R1_OHM + BAT_R2_OHM) / BAT_R2_OHM;

// Klidové napětí Li-ion článku → % (hrubá křivka)
static const float LUT_V[] = {3.00f, 3.30f, 3.40f, 3.50f, 3.60f, 3.70f, 3.80f, 3.90f, 4.00f, 4.10f, 4.20f};
static const uint8_t LUT_P[] = {0,     1,     4,     10,    22,    38,    52,    65,    78,    90,    100};

void begin() {
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BAT_SENSE, ADC_11db);
  pinMode(PIN_BAT_SENSE, INPUT);
}

static float rawToVolts(uint16_t mv) {
  return mv / 1000.0f * DIVIDER * settings::get(CFG_BATT_CAL) / 1000.0f;
}

void poll() {
  uint32_t sum = 0;
  for (int i = 0; i < BAT_SAMPLES; i++) sum += analogReadMilliVolts(PIN_BAT_SENSE);
  lastAdcMv = sum / BAT_SAMPLES;
  float v = rawToVolts(lastAdcMv);
  if (!primed) { vFilt = v; primed = true; }
  else vFilt += (v - vFilt) * 0.05f;   // EMA ~2 s při 100 ms
}

float volts() { return vFilt; }

// ADC u dorazu = přerušený dělič nebo zkrat → měření nelze věřit
bool valid() { return primed && lastAdcMv > 150 && lastAdcMv < 3000; }
uint16_t adcMilliVolts() { return lastAdcMv; }

uint8_t percent() {
  float cell = vFilt / BAT_CELLS;
  const int n = sizeof(LUT_V) / sizeof(LUT_V[0]);
  if (cell <= LUT_V[0]) return 0;
  if (cell >= LUT_V[n - 1]) return 100;
  for (int i = 1; i < n; i++) {
    if (cell < LUT_V[i]) {
      float t = (cell - LUT_V[i - 1]) / (LUT_V[i] - LUT_V[i - 1]);
      return (uint8_t)(LUT_P[i - 1] + t * (LUT_P[i] - LUT_P[i - 1]) + 0.5f);
    }
  }
  return 100;
}

uint16_t calibrate(float realVolts) {
  if (lastAdcMv < 100) return 0;
  float uncal = lastAdcMv / 1000.0f * DIVIDER;
  uint32_t cal = (uint32_t)(realVolts / uncal * 1000.0f + 0.5f);
  if (cal < 800 || cal > 1200) return 0;
  settings::setKey(CFG_BATT_CAL, cal);
  vFilt = realVolts;
  return cal;
}

}  // namespace battery
