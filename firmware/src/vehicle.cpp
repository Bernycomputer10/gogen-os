#include "vehicle.h"
#include "config.h"
#include "protocol.h"
#include "settings.h"
#include "battery.h"

namespace vehicle {

static bool front = false, tail = false, led = false;
static uint8_t ledBright = 128, ledMd = LED_MODE_STATIC;
static uint8_t blink = BLINK_OFF;
static uint32_t blinkStart = 0;
static bool brakeIn = false;
static uint32_t brakeAt = 0;
static bool low = false, crit = false;
static uint8_t outMask = 0;
static uint32_t nowCache = 0;

static inline void writeOut(uint8_t pin, bool on) {
  digitalWrite(pin, (on == OUT_ACTIVE_HIGH) ? HIGH : LOW);
}

static bool applyArg(bool cur, uint8_t arg) {
  if (arg == ARG_TOGGLE) return !cur;
  return arg == ARG_ON;
}

void begin() {
  const uint8_t pins[] = {PIN_FRONT, PIN_TAIL, PIN_BRAKE, PIN_LEFT, PIN_RIGHT};
  for (uint8_t p : pins) { writeOut(p, false); pinMode(p, OUTPUT); writeOut(p, false); }
  ledcSetup(LED_PWM_CH, LED_PWM_FREQ, LED_PWM_BITS);
  ledcAttachPin(PIN_LED, LED_PWM_CH);
  ledcWrite(LED_PWM_CH, OUT_ACTIVE_HIGH ? 0 : 255);

  ledBright = settings::cfg.ledBrightness;
  ledMd = settings::cfg.ledMode;
  if (settings::cfg.lightsOnBoot) setLights(ARG_ON);
}

void setFront(uint8_t arg) {
  front = applyArg(front, arg);
  if (settings::cfg.tailWithFront) tail = front;
}
void setTail(uint8_t arg) { tail = applyArg(tail, arg); }
void setLights(uint8_t arg) { front = applyArg(front, arg); tail = front; }

void setBlink(uint8_t mode) {
  if (mode > BLINK_HAZARD) mode = BLINK_OFF;
  if (mode != blink) blinkStart = nowCache;
  blink = mode;
}
void toggleBlink(uint8_t mode) { setBlink(blink == mode ? BLINK_OFF : mode); }

void setLed(uint8_t arg) { led = applyArg(led, arg); }
void setLedBrightness(uint8_t v) { ledBright = v; }
void setLedMode(uint8_t m) { if (m < LED_MODE_COUNT) ledMd = m; }
void cycleLedMode() { ledMd = (ledMd + 1) % LED_MODE_COUNT; led = true; }


void setBrakeInput(bool pressed) {
  if (pressed && !brakeIn) brakeAt = nowCache;
  brakeIn = pressed;
}

void allOff() {
  front = tail = led = false;
  blink = BLINK_OFF;
}

static uint8_t ledDuty(uint32_t now) {
  if (!led || crit) return 0;
  switch (ledMd) {
    case LED_MODE_BREATHE: {
      // trojúhelník 2 s, gamma ~2
      uint32_t t = now % 2000;
      float x = t < 1000 ? t / 1000.0f : (2000 - t) / 1000.0f;
      return (uint8_t)(ledBright * x * x);
    }
    case LED_MODE_STROBE:
      return (now % 500) < 60 ? ledBright : 0;
    case LED_MODE_BRAKE:
      return brakeIn ? 255 : ledBright / 4;
    default:
      return ledBright;
  }
}

void tick(uint32_t now) {
  nowCache = now;
  const Settings &c = settings::cfg;

  // Baterie – hystereze 0,5 V
  float v = battery::volts();
  if (v > 5.0f) {  // ignoruj, dokud není měření
    float lowV = c.battLow10mV / 100.0f, critV = c.battCrit10mV / 100.0f;
    if (!low && v < lowV) low = true;
    else if (low && v > lowV + 0.5f) low = false;
    if (!crit && v < critV) crit = true;
    else if (crit && v > critV + 0.5f) crit = false;
  }

  // Automatické vypnutí blinkrů (výstražná světla se nevypínají)
  if ((blink == BLINK_LEFT || blink == BLINK_RIGHT) && c.blinkAutoOffS &&
      now - blinkStart > (uint32_t)c.blinkAutoOffS * 1000UL)
    blink = BLINK_OFF;

  bool phase = ((now - blinkStart) % c.blinkPeriodMs) < c.blinkPeriodMs / 2;
  bool left = phase && (blink == BLINK_LEFT || blink == BLINK_HAZARD);
  bool right = phase && (blink == BLINK_RIGHT || blink == BLINK_HAZARD);

  // Brzdové světlo, volitelně 3× probliknutí
  bool brake = brakeIn;
  if (brakeIn && c.brakeFlash && now - brakeAt < 600) brake = ((now - brakeAt) / 100) % 2 == 0;

  writeOut(PIN_FRONT, front);
  writeOut(PIN_TAIL, tail);
  writeOut(PIN_BRAKE, brake);
  writeOut(PIN_LEFT, left);
  writeOut(PIN_RIGHT, right);
  uint8_t duty = ledDuty(now);
  ledcWrite(LED_PWM_CH, OUT_ACTIVE_HIGH ? duty : 255 - duty);

  outMask = (front ? OUT_FRONT : 0) | (tail ? OUT_TAIL : 0) | (brake ? OUT_BRAKE : 0) |
            (left ? OUT_LEFT : 0) | (right ? OUT_RIGHT : 0) |
            (led ? OUT_LED : 0);
}

uint8_t outputsMask() { return outMask; }
uint8_t blinkMode() { return blink; }
bool frontOn() { return front; }
bool ledEnabled() { return led; }
uint8_t ledBrightness() { return ledBright; }
uint8_t ledMode() { return ledMd; }
bool brakeInput() { return brakeIn; }
bool battLow() { return low; }
bool battCrit() { return crit; }

}  // namespace vehicle
