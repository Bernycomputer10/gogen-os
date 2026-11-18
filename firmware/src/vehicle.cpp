#include "vehicle.h"
#include "config.h"
#include "protocol.h"
#include "settings.h"
#include "battery.h"

namespace vehicle {

static uint8_t lightMd = LIGHT_OFF;
static bool autoLit = false, tailManual = false, led = false;
static uint8_t ledBright = 128, ledMd = LED_MODE_STATIC;
static uint8_t blink = BLINK_OFF;
static uint32_t blinkStart = 0;
static bool brakeIn = false, autoBrk = false, emerg = false;
static bool brakeWas = false;
static uint32_t brakeAt = 0;
static bool low = false, crit = false;
static uint8_t outMask = 0;
static uint32_t nowCache = 0;
static bool halted = false;

static const uint8_t PINS[] = {PIN_FRONT, PIN_TAIL, PIN_BRAKE, PIN_LEFT, PIN_RIGHT};

static inline void writeOut(uint8_t pin, bool on) {
  digitalWrite(pin, (on == OUT_ACTIVE_HIGH) ? HIGH : LOW);
}

static bool applyArg(bool cur, uint8_t arg) {
  if (arg == ARG_TOGGLE) return !cur;
  return arg == ARG_ON;
}

static bool frontLit() { return lightMd == LIGHT_ON || (lightMd == LIGHT_AUTO && autoLit); }

void begin() {
  for (uint8_t p : PINS) { writeOut(p, false); pinMode(p, OUTPUT); writeOut(p, false); }
  ledcSetup(LED_PWM_CH, LED_PWM_FREQ, LED_PWM_BITS);
  ledcAttachPin(PIN_LED, LED_PWM_CH);
  ledcWrite(LED_PWM_CH, OUT_ACTIVE_HIGH ? 0 : 255);

  ledBright = settings::get(CFG_LED_BRIGHT);
  ledMd = settings::get(CFG_LED_MODE);
  lightMd = settings::get(CFG_LIGHTS_ON_BOOT);
  tailManual = false;
  halted = false;
}

void forceOff() {
  halted = true;
  for (uint8_t p : PINS) writeOut(p, false);
  ledcWrite(LED_PWM_CH, OUT_ACTIVE_HIGH ? 0 : 255);
  outMask = 0;
}

void setFront(uint8_t arg) { lightMd = applyArg(frontLit(), arg) ? LIGHT_ON : LIGHT_OFF; }
void setTail(uint8_t arg) { tailManual = applyArg(tailManual || (settings::get(CFG_TAIL_WITH_FRONT) && frontLit()), arg); }
void setLights(uint8_t arg) {
  bool on = applyArg(frontLit(), arg);
  lightMd = on ? LIGHT_ON : LIGHT_OFF;
  tailManual = on;
}
void setLightMode(uint8_t m) { if (m <= LIGHT_AUTO) { lightMd = m; if (m == LIGHT_OFF) tailManual = false; } }
void cycleLightMode(bool autoAvailable) {
  if (lightMd == LIGHT_OFF) lightMd = LIGHT_ON;
  else if (lightMd == LIGHT_ON && autoAvailable) lightMd = LIGHT_AUTO;
  else { lightMd = LIGHT_OFF; tailManual = false; }
}
void setAutoLight(bool lit) { autoLit = lit; }
void setAutoBrake(bool active, bool emergency) { autoBrk = active; emerg = active && emergency; }

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

void setBrakeInput(bool pressed) { brakeIn = pressed; }

void allOff() {
  lightMd = LIGHT_OFF;
  tailManual = led = false;
  blink = BLINK_OFF;
}

static uint8_t ledDuty(uint32_t now, bool brake) {
  if (!led || crit) return 0;
  switch (ledMd) {
    case LED_MODE_BREATHE: {
      uint32_t t = now % 2000;
      float x = t < 1000 ? t / 1000.0f : (2000 - t) / 1000.0f;
      return (uint8_t)(ledBright * x * x);
    }
    case LED_MODE_STROBE:
      return (now % 500) < 60 ? ledBright : 0;
    case LED_MODE_BRAKE:
      return brake ? 255 : ledBright / 4;
    default:
      return ledBright;
  }
}

void tick(uint32_t now) {
  nowCache = now;
  if (halted) return;

  // Baterie – hystereze 0,5 V
  float v = battery::volts();
  if (battery::valid()) {
    float lowV = settings::get(CFG_BATT_LOW) / 100.0f, critV = settings::get(CFG_BATT_CRIT) / 100.0f;
    if (!low && v < lowV) low = true;
    else if (low && v > lowV + 0.5f) low = false;
    if (!crit && v < critV) crit = true;
    else if (crit && v > critV + 0.5f) crit = false;
  }

  // Automatické vypnutí blinkrů (výstražná světla se nevypínají)
  uint16_t autoOff = settings::get(CFG_BLINK_AUTOOFF), period = settings::get(CFG_BLINK_PERIOD);
  if ((blink == BLINK_LEFT || blink == BLINK_RIGHT) && autoOff &&
      now - blinkStart > (uint32_t)autoOff * 1000UL)
    blink = BLINK_OFF;

  bool phase = ((now - blinkStart) % period) < period / 2;
  bool left = phase && (blink == BLINK_LEFT || blink == BLINK_HAZARD);
  bool right = phase && (blink == BLINK_RIGHT || blink == BLINK_HAZARD);

  // Brzdové světlo: páka nebo akcelerometr; úvodní 3× probliknutí, prudké brzdění bliká 4 Hz
  bool brakeReq = brakeIn || autoBrk;
  if (brakeReq && !brakeWas) brakeAt = now;
  brakeWas = brakeReq;
  bool brake = brakeReq;
  if (brakeReq && settings::get(CFG_BRAKE_FLASH) && now - brakeAt < 600) brake = ((now - brakeAt) / 100) % 2 == 0;
  else if (emerg) brake = (now / 125) % 2 == 0;

  bool front = frontLit();
  bool tail = tailManual || (settings::get(CFG_TAIL_WITH_FRONT) && front);

  writeOut(PIN_FRONT, front);
  writeOut(PIN_TAIL, tail);
  writeOut(PIN_BRAKE, brake);
  writeOut(PIN_LEFT, left);
  writeOut(PIN_RIGHT, right);
  uint8_t duty = ledDuty(now, brakeReq);
  ledcWrite(LED_PWM_CH, OUT_ACTIVE_HIGH ? duty : 255 - duty);

  outMask = (front ? OUT_FRONT : 0) | (tail ? OUT_TAIL : 0) | (brake ? OUT_BRAKE : 0) |
            (left ? OUT_LEFT : 0) | (right ? OUT_RIGHT : 0) | (led ? OUT_LED : 0);
}

uint8_t outputsMask() { return outMask; }
uint8_t lightMode() { return lightMd; }
uint8_t blinkMode() { return blink; }
bool frontOn() { return frontLit(); }
bool ledEnabled() { return led; }
uint8_t ledBrightness() { return ledBright; }
uint8_t ledMode() { return ledMd; }
bool brakeInput() { return brakeIn; }
bool brakeLight() { return brakeIn || autoBrk; }
bool autoBrake() { return autoBrk; }
bool emergency() { return emerg; }
bool battLow() { return low; }
bool battCrit() { return crit; }

}  // namespace vehicle
