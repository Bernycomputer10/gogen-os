#include "inputs.h"
#include <Wire.h>
#include "config.h"

namespace inputs {

struct BtnState {
  bool stable = false;       // debounced
  bool lastRaw = false;
  uint32_t changedAt = 0;    // kdy se změnil raw
  uint32_t pressedAt = 0;
  bool longFired = false;
};

static BtnState st[BTN_COUNT];
static ButtonHandler cb = nullptr;
static bool pcfOk = true;
static uint32_t pcfRetryAt = 0;

#if GOGEN_INPUT_MODE == INPUT_MODE_PCF
static uint8_t pcfLast = 0xFF;

static bool pcfRead(uint8_t &out) {
  if (Wire.requestFrom((uint8_t)PCF8574_ADDR, (uint8_t)1) != 1) return false;
  out = Wire.read();
  return true;
}

static void pcfInit() {
  Wire.beginTransmission(PCF8574_ADDR);
  Wire.write(0xFF);  // všechny piny jako kvazi-vstupy s pull-upem
  pcfOk = Wire.endTransmission() == 0;
}
#else
static const int8_t gpioPins[BTN_COUNT - 1] = {PIN_BTN_LIGHT, PIN_BTN_LEFT, PIN_BTN_RIGHT,
                                               PIN_BTN_MODE};
#endif

void begin(ButtonHandler handler) {
  cb = handler;
  pinMode(PIN_BRAKE_IN, INPUT_PULLUP);
#if GOGEN_INPUT_MODE == INPUT_MODE_PCF
  pcfInit();
#else
  for (int8_t p : gpioPins)
    if (p >= 0) pinMode(p, INPUT_PULLUP);
#endif
}

static void readRaw(bool raw[BTN_COUNT], uint32_t now) {
  bool brake = digitalRead(PIN_BRAKE_IN);
  raw[BTN_BRAKE] = BRAKE_ACTIVE_LOW ? !brake : brake;

#if GOGEN_INPUT_MODE == INPUT_MODE_PCF
  uint8_t v = 0xFF;
  if (pcfOk) {
    if (pcfRead(v)) pcfLast = v;
    else { pcfOk = false; pcfRetryAt = now + 1000; }
  } else if ((int32_t)(now - pcfRetryAt) >= 0) {
    pcfInit();
    pcfRetryAt = now + 1000;
  }
  // Při výpadku expandéru se tlačítka chovají jako puštěná (bezpečný stav)
  v = pcfOk ? pcfLast : 0xFF;
  raw[BTN_LIGHT] = !(v & (1 << PCF_BIT_LIGHT));
  raw[BTN_LEFT]  = !(v & (1 << PCF_BIT_LEFT));
  raw[BTN_RIGHT] = !(v & (1 << PCF_BIT_RIGHT));
  raw[BTN_MODE]  = !(v & (1 << PCF_BIT_MODE));
#else
  for (int i = 0; i < BTN_COUNT - 1; i++)
    raw[i] = gpioPins[i] >= 0 ? !digitalRead(gpioPins[i]) : false;
#endif
}

void poll(uint32_t now) {
  bool raw[BTN_COUNT];
  readRaw(raw, now);

  for (int i = 0; i < BTN_COUNT; i++) {
    BtnState &b = st[i];
    if (raw[i] != b.lastRaw) { b.lastRaw = raw[i]; b.changedAt = now; }

    if (raw[i] != b.stable && now - b.changedAt >= DEBOUNCE_MS) {
      b.stable = raw[i];
      if (b.stable) {
        b.pressedAt = now;
        b.longFired = false;
        if (cb) cb((Button)i, BtnEvent::Press);
      } else {
        if (cb) cb((Button)i, BtnEvent::Release);
        if (!b.longFired && cb) cb((Button)i, BtnEvent::Click);
      }
    }
    if (b.stable && !b.longFired && now - b.pressedAt >= LONG_PRESS_MS) {
      b.longFired = true;
      if (cb) cb((Button)i, BtnEvent::LongPress);
    }
  }
}

bool isDown(Button b) { return st[b].stable; }

uint8_t rawMask() {
  uint8_t m = 0;
  for (int i = 0; i < BTN_COUNT; i++) if (st[i].stable) m |= 1 << i;
  return m;
}

bool expanderOk() { return pcfOk; }

}  // namespace inputs
