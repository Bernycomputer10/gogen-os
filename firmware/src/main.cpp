// =====================================================================
//  GoGEN OS 3 – řídicí firmware sekundárního 36V okruhu GoGEN V2
//  ESP-WROOM-32 (ESP32 DevKit) · PlatformIO / Arduino · NimBLE
//
//  Moduly: osvětlení (vehicle), tlačítka (inputs), baterie, senzory
//  (BH1750, MPU6050, MCP4725), rychloměr, dobíjení z předního kola,
//  přední pohon, napájení, Wi-Fi OTA, OLED, BLE, sériová konzole.
// =====================================================================
#include <Arduino.h>
#include <Wire.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "protocol.h"
#include "settings.h"
#include "vehicle.h"
#include "inputs.h"
#include "battery.h"
#include "sensors.h"
#include "speed.h"
#include "charge.h"
#include "drive.h"
#include "power.h"
#include "ota.h"
#include "display.h"
#include "ble_service.h"

static uint32_t tInput = 0, tSensor = 0, tDrive = 0, tBatt = 0, tDisp = 0;
static uint32_t tNotify = 0, tLastNotify = 0, tTelem = 0;
static uint8_t lastState[STATE_LEN];
static bool rebootPending = false, offPending = false;
static uint32_t rebootAt = 0;

#define LOG(...) do { if (GOGEN_CONSOLE) Serial.printf(__VA_ARGS__); } while (0)

// ---------------------------------------------------------------------
//  Pakety
// ---------------------------------------------------------------------
static inline void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static inline void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = v >> (8 * i); }
static inline uint16_t get16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static inline uint32_t get32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static inline uint16_t clamp16(float v) { return v < 0 ? 0 : v > 65534 ? 65534 : (uint16_t)(v + 0.5f); }

static uint8_t outputs() {
  return vehicle::outputsMask() | (charge::output() ? OUT_CHG : 0) | (drive::phaseClosed() ? OUT_PHASE : 0);
}

static void buildState(uint8_t *b, uint32_t now) {
  memset(b, 0, STATE_LEN);
  b[0] = PROTO_VERSION;
  b[1] = outputs();
  b[2] = vehicle::ledBrightness();
  b[3] = vehicle::ledMode();
  put16(b + 4, clamp16(battery::volts() * 100.0f));
  b[6] = battery::percent();
  b[7] = (ble::secured() ? ST_BLE_BONDED : 0) | (vehicle::battLow() ? ST_BAT_LOW : 0) |
         (vehicle::battCrit() ? ST_BAT_CRIT : 0) |
         (GOGEN_INPUT_MODE == INPUT_MODE_PCF ? ST_INPUT_PCF : 0) |
         (inputs::expanderOk() ? ST_INPUT_OK : 0) | (display::ok() ? ST_DISPLAY_OK : 0) |
         (vehicle::brakeInput() ? ST_BRAKE_IN : 0) | (vehicle::autoBrake() ? ST_AUTO_BRAKE : 0);
  put32(b + 8, now / 1000);
  b[12] = vehicle::blinkMode();
  b[13] = inputs::rawMask();
  b[14] = vehicle::lightMode();
  uint16_t left = power::secondsLeft(now);
  b[15] = (sensors::luxOk() ? ST2_LUX_OK : 0) | (sensors::mpuOk() ? ST2_MPU_OK : 0) |
          (sensors::dacOk() ? ST2_DAC_OK : 0) | (speed::known() ? ST2_SPEED_OK : 0) |
          (vehicle::emergency() ? ST2_EMERGENCY : 0) | (ota::active() ? ST2_OTA : 0) |
          (left < 60 ? ST2_OFF_SOON : 0) | (FEATURE_DRIVE ? ST2_DRIVE_HW : 0);
  put16(b + 16, clamp16(speed::kmh() * 10));
  b[18] = (uint8_t)charge::state();
  b[19] = (uint8_t)drive::state();
}

static void buildTelemetry(uint8_t *b, uint32_t now) {
  memset(b, 0, TELEM_LEN);
  b[0] = PROTO_VERSION;
  b[1] = (uint8_t)charge::mode();
  b[2] = charge::reasons();
  b[3] = charge::fault();
  b[4] = drive::reasons() | (drive::enabled() ? 0 : drv::R_DISABLED);
  b[5] = drive::fault();
  b[6] = (uint8_t)(drive::output() * 100 + 0.5f);
  float t = drive::throttle();
  b[7] = t < 0 ? 0xFF : (uint8_t)(t * 100 + 0.5f);
  put16(b + 8, clamp16(speed::rearKmh() * 10));
  put16(b + 10, clamp16(speed::frontKmh() * 10));
  put16(b + 12, clamp16(charge::genVolts() * 10));
  float lx = sensors::lux();
  put16(b + 14, lx < 0 ? 0xFFFF : clamp16(lx));
  int16_t mg = (int16_t)(sensors::decelG() * 1000);
  b[16] = mg & 0xFF; b[17] = (mg >> 8) & 0xFF;
  put32(b + 18, speed::odometerM());
  put32(b + 22, speed::tripM());
  put16(b + 26, power::secondsLeft(now));
  put16(b + 28, charge::chargingMin(now));
}

static void buildConfig(uint8_t *b) {
  memset(b, 0, CFG_LEN);
  b[0] = PROTO_VERSION;
  b[1] = GOGEN_FW_MAJOR;
  b[2] = GOGEN_FW_MINOR;
  b[3] = GOGEN_INPUT_MODE;
  for (int k = 1; k <= CFG_KEY_COUNT; k++) put16(b + 4 + 2 * (k - 1), settings::get(k));
  b[4 + 2 * CFG_KEY_COUNT] = (FEATURE_SPEED ? HW_SPEED : 0) | (FEATURE_DRIVE ? HW_DRIVE : 0) |
                             (FEATURE_CHARGE ? HW_CHARGE : 0) | (FEATURE_OTA ? HW_OTA : 0);
}

static void refreshConfig() {
  uint8_t b[CFG_LEN];
  buildConfig(b);
  ble::publishConfig(b, CFG_LEN);
}

static void applyKey(uint8_t key) {
  uint16_t v = settings::get(key);
  if (key == CFG_LED_BRIGHT) vehicle::setLedBrightness(v);
  if (key == CFG_LED_MODE) vehicle::setLedMode(v);
  if (key == CFG_CHG_MODE) charge::setMode(v ? chg::Mode::OFF : chg::Mode::AUTO);
  if (key == CFG_DRV_ALLOWED && !v) drive::setEnabled(ARG_OFF);
}

static bool startOta() {
  if (speed::kmh() > OTA_MAX_KMH || drive::phaseClosed()) {
    display::message("Aktualizace", "ZASTAV");
    return false;
  }
  drive::setEnabled(ARG_OFF);
  bool ok = ota::start(ble::name(), settings::passkey());
  LOG(ok ? "OTA: Wi-Fi %s, heslo %s, http://192.168.4.1\n" : "OTA: nelze spustit%s%s\n",
      ok ? ota::ssid() : "", ok ? ota::password() : "");
  return ok;
}

// ---------------------------------------------------------------------
//  Příkazy (BLE i konzole)
// ---------------------------------------------------------------------
static void handleCommand(const uint8_t *d, size_t len) {
  if (len == 0) return;
  power::activity();
  uint8_t a = len > 1 ? d[1] : 0;
  switch (d[0]) {
    case CMD_FRONT:      vehicle::setFront(a); break;
    case CMD_TAIL:       vehicle::setTail(a); break;
    case CMD_LIGHTS:     vehicle::setLights(a); break;
    case CMD_LIGHT_MODE: vehicle::setLightMode(a); break;
    case CMD_BLINK:      vehicle::setBlink(a); break;
    case CMD_LED:        vehicle::setLed(a); break;
    case CMD_LED_BRIGHT: vehicle::setLedBrightness(a); break;
    case CMD_LED_MODE:   vehicle::setLedMode(a); break;
    case CMD_ALL_OFF:    vehicle::allOff(); break;
    case CMD_SET_CFG:
      if (len >= 4 && settings::setKey(d[1], get16(d + 2))) { applyKey(d[1]); refreshConfig(); }
      break;
    case CMD_SAVE_CFG:
      settings::save();
      display::message("Nastaveni", "ULOZENO");
      break;
    case CMD_CALIBRATE:
      if (len >= 3 && battery::calibrate(get16(d + 1) / 100.0f)) {
        settings::save();
        refreshConfig();
        display::message("Kalibrace", "OK");
      }
      break;
    case CMD_SET_PASSKEY:
      if (len >= 5) {
        uint32_t pk = get32(d + 1);
        if (pk >= 1 && pk <= 999999) {
          settings::cfg.passkey = pk;
          settings::save();
          ble::deleteBonds();
          display::message("Novy PIN - restart", "");
          rebootPending = true;
          rebootAt = millis() + 1500;
        }
      }
      break;
    case CMD_FACTORY:
      settings::factoryReset();
      ble::deleteBonds();
      display::message("Tovarni reset", "RESTART");
      rebootPending = true;
      rebootAt = millis() + 1500;
      break;
    case CMD_POWER_OFF:  offPending = true; break;
    case CMD_CHG_MODE:   if (a <= 2) charge::setMode((chg::Mode)a); break;
    case CMD_CHG_RESET:  charge::resetFault(); break;
    case CMD_DRIVE:
      drive::setEnabled(a);
      if (a != ARG_OFF && !settings::get(CFG_DRV_ALLOWED)) display::message("Predni pohon", "ZAKAZAN");
      break;
    case CMD_DRIVE_RESET: drive::resetFault(); break;
    case CMD_THR_CAL:
      if (drive::calibrateThrottle(a)) { refreshConfig(); display::message("Plyn", a ? "MAX OK" : "MIN OK"); }
      break;
    case CMD_TRIP_RESET: speed::resetTrip(); break;
    case CMD_REBOOT:
      rebootPending = true;
      rebootAt = millis() + 300;
      break;
    case CMD_OTA:
      if (a) startOta(); else ota::stop();
      break;
  }
}

// ---------------------------------------------------------------------
//  Tlačítka na řídítkách
// ---------------------------------------------------------------------
static void onButton(Button b, BtnEvent e) {
  power::activity();
  switch (b) {
    case BTN_LIGHT:
      if (e == BtnEvent::Click) vehicle::cycleLightMode(sensors::luxOk());
      else if (e == BtnEvent::LongPress) vehicle::setLed(ARG_TOGGLE);
      break;
    case BTN_LEFT:
      if (e == BtnEvent::Combo) { offPending = true; break; }
      if (e == BtnEvent::Click) vehicle::toggleBlink(BLINK_LEFT);
      else if (e == BtnEvent::LongPress) vehicle::toggleBlink(BLINK_HAZARD);
      break;
    case BTN_RIGHT:
      if (e == BtnEvent::Click) vehicle::toggleBlink(BLINK_RIGHT);
      else if (e == BtnEvent::LongPress) vehicle::toggleBlink(BLINK_HAZARD);
      break;
    case BTN_MODE:
      if (e == BtnEvent::Click) vehicle::cycleLedMode();
      else if (e == BtnEvent::LongPress) {
        if (FEATURE_DRIVE && settings::get(CFG_DRV_ALLOWED)) {
          drive::setEnabled(ARG_TOGGLE);
          display::message("Predni pohon", drive::enabled() ? "ZAPNUT" : "VYPNUT");
        } else vehicle::toggleBlink(BLINK_HAZARD);
      }
      break;
    case BTN_BRAKE:
      if (e == BtnEvent::Press) vehicle::setBrakeInput(true);
      else if (e == BtnEvent::Release) vehicle::setBrakeInput(false);
      break;
    default: break;
  }
}

// ---------------------------------------------------------------------
//  Sériová konzole (UART0 přes USB-UART převodník, 115 200 Bd)
// ---------------------------------------------------------------------
#if GOGEN_CONSOLE
static char line[64];
static uint8_t lineLen = 0;

static uint8_t onoff(const char *s) {
  if (!s) return ARG_TOGGLE;
  if (!strcmp(s, "on") || !strcmp(s, "1")) return ARG_ON;
  if (!strcmp(s, "off") || !strcmp(s, "0")) return ARG_OFF;
  return ARG_TOGGLE;
}

static const char *CHG_ST[] = {"ceka na 25 %", "chce nabijet", "NABIJI", "nabito", "PORUCHA"};
static const char *DRV_ST[] = {"vypnuto", "pripojuji", "pripraven", "POHANI", "odpojuji", "PORUCHA"};
static const char *LIGHT_ST[] = {"vyp", "zap", "auto"};

static void printStatus() {
  uint32_t now = millis();
  Serial.printf("GoGEN OS v%d.%d  BLE=%s %s  bezi %lu s\n", GOGEN_FW_MAJOR, GOGEN_FW_MINOR, ble::name(),
                ble::connected() ? "(pripojeno)" : "", (unsigned long)(now / 1000));
  Serial.printf("Baterie %.2f V  %u %%  ADC %u mV  cal %u  %s\n", battery::volts(), battery::percent(),
                battery::adcMilliVolts(), settings::get(CFG_BATT_CAL), battery::valid() ? "" : "NEPLATNE");
  Serial.printf("Svetla %s  vystupy 0x%02X  blink %u  LED %u/%u  brzda %d auto %d%s\n",
                LIGHT_ST[vehicle::lightMode()], outputs(), vehicle::blinkMode(), vehicle::ledBrightness(),
                vehicle::ledMode(), vehicle::brakeInput(), vehicle::autoBrake(), vehicle::emergency() ? " PRUDCE" : "");
  Serial.printf("Senzory: PCF %s  OLED %s  BH1750 %s (%.0f lx)  MPU %s (%.2f g)  DAC %s\n",
                inputs::expanderOk() ? "ok" : "CHYBA", display::ok() ? "ok" : "-",
                sensors::luxOk() ? "ok" : "-", sensors::lux(), sensors::mpuOk() ? "ok" : "-",
                sensors::decelG(), sensors::dacOk() ? "ok" : "-");
  Serial.printf("Rychlost zadni %.1f %s  predni %.1f %s km/h  vylet %.2f km  celkem %.1f km\n",
                speed::rearKmh(), speed::rearValid() ? "" : "(bez signalu)", speed::frontKmh(),
                speed::frontValid() ? "" : "(bez signalu)", speed::tripM() / 1000.0f, speed::odometerM() / 1000.0f);
  Serial.printf("Nabijeni %s  rezim %u  generator %.1f V  duvody 0x%02X  porucha %u\n",
                CHG_ST[(int)charge::state()], (unsigned)charge::mode(), charge::genVolts(), charge::reasons(),
                charge::fault());
  Serial.printf("Pohon %s %s  plyn %d %% (%u mV)  vystup %d %%  duvody 0x%02X  porucha %u\n",
                DRV_ST[(int)drive::state()], drive::enabled() ? "[zap]" : "[vyp]",
                (int)(drive::throttle() * 100), drive::throttleMv(), (int)(drive::output() * 100),
                drive::reasons(), drive::fault());
  uint16_t left = power::secondsLeft(now);
  if (left != 0xFFFF) Serial.printf("Automaticke vypnuti za %u s\n", left);
  if (ota::active()) Serial.printf("OTA: %s / %s  http://192.168.4.1\n", ota::ssid(), ota::password());
}

static void printSettings() {
  for (int k = 1; k <= CFG_KEY_COUNT; k++) {
    const KeyInfo *ki = settings::info(k);
    Serial.printf("%2d %-13s %5u  (%u..%u, vychozi %u)\n", k, ki->name, settings::get(k), ki->min, ki->max, ki->def);
  }
}

static void help() {
  Serial.println(
    "status | settings | set <klic|cislo> <hodnota> | save | factory | reboot | poweroff\n"
    "front/tail/lights [on|off] | light off|on|auto | left | right | hazard | off\n"
    "led [on|off|0-255] | mode 0-3 | cal <V>\n"
    "chg [auto|off|force|reset] | drv [on|off|reset] | drv cal min|max | drv dacinit\n"
    "trip reset | ota [on|off] | pin <000000>");
}

static void consoleExec(char *s) {
  char *cmd = strtok(s, " ");
  char *arg = strtok(nullptr, " ");
  char *arg2 = strtok(nullptr, " ");
  if (!cmd) return;
  uint8_t d[5] = {0};
  power::activity();
  if (!strcmp(cmd, "status") || !strcmp(cmd, "st")) printStatus();
  else if (!strcmp(cmd, "settings") || !strcmp(cmd, "get")) printSettings();
  else if (!strcmp(cmd, "front")) { d[0] = CMD_FRONT; d[1] = onoff(arg); handleCommand(d, 2); }
  else if (!strcmp(cmd, "tail"))  { d[0] = CMD_TAIL; d[1] = onoff(arg); handleCommand(d, 2); }
  else if (!strcmp(cmd, "lights")){ d[0] = CMD_LIGHTS; d[1] = onoff(arg); handleCommand(d, 2); }
  else if (!strcmp(cmd, "light") && arg) {
    d[0] = CMD_LIGHT_MODE;
    d[1] = !strcmp(arg, "auto") ? LIGHT_AUTO : !strcmp(arg, "on") ? LIGHT_ON : LIGHT_OFF;
    handleCommand(d, 2);
  }
  else if (!strcmp(cmd, "left"))  vehicle::toggleBlink(BLINK_LEFT);
  else if (!strcmp(cmd, "right")) vehicle::toggleBlink(BLINK_RIGHT);
  else if (!strcmp(cmd, "hazard"))vehicle::toggleBlink(BLINK_HAZARD);
  else if (!strcmp(cmd, "led")) {
    if (arg && isdigit(arg[0])) vehicle::setLedBrightness(atoi(arg)), vehicle::setLed(ARG_ON);
    else vehicle::setLed(onoff(arg));
  }
  else if (!strcmp(cmd, "mode") && arg) vehicle::setLedMode(atoi(arg));
  else if (!strcmp(cmd, "off")) vehicle::allOff();
  else if (!strcmp(cmd, "cal") && arg) {
    uint16_t c = battery::calibrate(atof(arg));
    if (c) { settings::save(); refreshConfig(); Serial.printf("Kalibrace %u ulozena\n", c); }
    else Serial.println("Kalibrace mimo rozsah");
  }
  else if (!strcmp(cmd, "set") && arg && arg2) {
    int k = isdigit(arg[0]) ? atoi(arg) : settings::keyByName(arg);
    if (k > 0 && settings::setKey(k, atoi(arg2))) { applyKey(k); refreshConfig(); Serial.println("OK (ulozit: save)"); }
    else Serial.println("Neplatny klic nebo hodnota (viz settings)");
  }
  else if (!strcmp(cmd, "save")) { settings::save(); Serial.println("Ulozeno"); }
  else if (!strcmp(cmd, "pin") && arg) {
    d[0] = CMD_SET_PASSKEY; uint32_t pk = atol(arg);
    for (int i = 0; i < 4; i++) d[1 + i] = pk >> (8 * i);
    handleCommand(d, 5);
  }
  else if (!strcmp(cmd, "factory")) { d[0] = CMD_FACTORY; handleCommand(d, 1); }
  else if (!strcmp(cmd, "reboot")) ESP.restart();
  else if (!strcmp(cmd, "poweroff")) offPending = true;
  else if (!strcmp(cmd, "chg")) {
    if (!arg) printStatus();
    else if (!strcmp(arg, "auto")) charge::setMode(chg::Mode::AUTO);
    else if (!strcmp(arg, "off")) charge::setMode(chg::Mode::OFF);
    else if (!strcmp(arg, "force")) charge::setMode(chg::Mode::FORCE);
    else if (!strcmp(arg, "reset")) charge::resetFault();
    Serial.printf("Nabijeni: %s\n", CHG_ST[(int)charge::state()]);
  }
  else if (!strcmp(cmd, "drv")) {
    if (!arg) printStatus();
    else if (!strcmp(arg, "reset")) drive::resetFault();
    else if (!strcmp(arg, "cal") && arg2) {
      uint16_t mv = drive::calibrateThrottle(!strcmp(arg2, "max"));
      if (mv) { refreshConfig(); Serial.printf("Plyn %s = %u mV (ulozit: save)\n", arg2, mv); }
      else Serial.println("Kalibrace plynu selhala (pohon musi byt odpojeny)");
    }
    else if (!strcmp(arg, "dacinit"))
      Serial.println(sensors::dacSetPowerOnZero() ? "MCP4725: vychozi 0 V ulozeno" : "MCP4725 neodpovida");
    else { drive::setEnabled(onoff(arg)); if (!settings::get(CFG_DRV_ALLOWED)) Serial.println("Povol: set drv_allowed 1"); }
    Serial.printf("Pohon: %s\n", DRV_ST[(int)drive::state()]);
  }
  else if (!strcmp(cmd, "trip")) { speed::resetTrip(); Serial.println("Vylet vynulovan"); }
  else if (!strcmp(cmd, "ota")) { if (arg && !strcmp(arg, "off")) ota::stop(); else startOta(); }
  else help();
}

static void consolePoll() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') { line[lineLen] = 0; consoleExec(line); lineLen = 0; }
    else if (lineLen < sizeof(line) - 1) line[lineLen++] = c;
  }
}
#endif

// ---------------------------------------------------------------------
void setup() {
  power::begin();                // PWR_HOLD hned do HIGH, ať latch drží
  vehicle::begin();              // všechny výstupy bezpečně vypnout
  charge::begin();
  drive::begin();
#if GOGEN_CONSOLE
  Serial.begin(115200);
#endif
  settings::load();
  vehicle::begin();              // znovu s načteným nastavením
  charge::begin();

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ);
  Wire.setTimeOut(20);
  sensors::begin();              // MCP4725 hned na klidový plyn
  inputs::begin(onButton);
  display::begin();
  battery::begin();
  battery::poll();
  speed::begin();
  ble::begin(handleCommand);
  refreshConfig();

  esp_task_wdt_init(WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);

  LOG("\nGoGEN OS v%d.%d start (%s), BLE %s, PIN %06u\n", GOGEN_FW_MAJOR, GOGEN_FW_MINOR,
      GOGEN_INPUT_MODE == INPUT_MODE_PCF ? "PCF8574" : "GPIO", ble::name(), (unsigned)settings::passkey());
}

void loop() {
  uint32_t now = millis();
  esp_task_wdt_reset();

  ble::process();
#if GOGEN_CONSOLE
  consolePoll();
#endif

  if (now - tInput >= INPUT_POLL_MS) { tInput = now; inputs::poll(now); }
  if (now - tSensor >= SENSOR_POLL_MS) { tSensor = now; sensors::poll(now); speed::update(now); }
  if (now - tDrive >= DRIVE_TICK_MS) { tDrive = now; drive::tick(now); }
  if (now - tBatt >= BATTERY_POLL_MS) {
    tBatt = now;
    battery::poll();
    // zapnutý přední pohon má přednost: nabíjení se nejdřív vypne, pak se připojí fáze
    charge::tick(now, drive::enabled() || drive::phaseClosed());
  }

  vehicle::setAutoLight(sensors::autoLightLit());
  vehicle::setAutoBrake(sensors::autoBrakeActive(), sensors::emergency());
  vehicle::tick(now);

  ota::handle(now);

  // automatické vypnutí: počítadlo stojí při jízdě, nabíjení, BLE spojení a OTA
  bool busy = speed::kmh() > 1.0f || charge::output() || ble::connected() || ota::active() ||
              drive::phaseClosed();
  pwr::OffReason off = power::tick(now, busy);
  if (offPending) off = pwr::OffReason::USER;
  if (off != pwr::OffReason::NONE && !ota::busy()) {
    ota::stop();
    power::shutdown(off);
  }

  // BLE notifikace: při změně (max 20×/s) nebo periodicky
  uint8_t st[STATE_LEN];
  buildState(st, now);
  bool changed = memcmp(st, lastState, 8) != 0 || memcmp(st + 12, lastState + 12, STATE_LEN - 12) != 0;
  if ((changed && now - tLastNotify >= 50) || now - tNotify >= BLE_NOTIFY_MS) {
    ble::publishState(st, STATE_LEN);
    memcpy(lastState, st, STATE_LEN);
    tNotify = tLastNotify = now;
  }
  if (now - tTelem >= BLE_NOTIFY_MS) {
    tTelem = now;
    uint8_t te[TELEM_LEN];
    buildTelemetry(te, now);
    ble::publishTelemetry(te, TELEM_LEN);
  }

  if (now - tDisp >= DISPLAY_REFRESH_MS) { tDisp = now; display::render(now, ble::connected()); }

  if (rebootPending && (int32_t)(now - rebootAt) >= 0) { speed::saveNow(); ESP.restart(); }

  delay(ota::active() ? 1 : LOOP_TICK_MS);
}
