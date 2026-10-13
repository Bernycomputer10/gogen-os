// =====================================================================
//  GoGEN OS – řídicí firmware sekundárního 36V okruhu GoGEN V2
//  ESP-WROOM-32 (ESP32 DevKit) · PlatformIO / Arduino · NimBLE
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
#include "display.h"
#include "ble_service.h"

static uint32_t tInput = 0, tBatt = 0, tDisp = 0, tNotify = 0, tLastNotify = 0;
static uint8_t lastState[STATE_LEN];
static bool rebootPending = false;
static uint32_t rebootAt = 0;

#define LOG(...) do { if (GOGEN_CONSOLE) Serial.printf(__VA_ARGS__); } while (0)

// ---------------------------------------------------------------------
//  Pakety
// ---------------------------------------------------------------------
static inline void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static inline void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = v >> (8 * i); }
static inline uint16_t get16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static inline uint32_t get32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

static void buildState(uint8_t *b, uint32_t now) {
  memset(b, 0, STATE_LEN);
  b[0] = PROTO_VERSION;
  b[1] = vehicle::outputsMask();
  b[2] = vehicle::ledBrightness();
  b[3] = vehicle::ledMode();
  put16(b + 4, (uint16_t)(battery::volts() * 100.0f + 0.5f));
  b[6] = battery::percent();
  b[7] = (ble::secured() ? ST_BLE_BONDED : 0) | (vehicle::battLow() ? ST_BAT_LOW : 0) |
         (vehicle::battCrit() ? ST_BAT_CRIT : 0) |
         (GOGEN_INPUT_MODE == INPUT_MODE_PCF ? ST_INPUT_PCF : 0) |
         (inputs::expanderOk() ? ST_INPUT_OK : 0) | (display::ok() ? ST_DISPLAY_OK : 0) |
         (vehicle::brakeInput() ? ST_BRAKE_IN : 0);
  put32(b + 8, now / 1000);
  b[12] = vehicle::blinkMode();
  b[13] = inputs::rawMask();
}

static void buildConfig(uint8_t *b) {
  const Settings &c = settings::cfg;
  memset(b, 0, CFG_LEN);
  b[0] = PROTO_VERSION;
  b[1] = GOGEN_FW_MAJOR;
  b[2] = GOGEN_FW_MINOR;
  b[3] = GOGEN_INPUT_MODE;
  put16(b + 4, c.blinkPeriodMs);
  put16(b + 6, c.blinkAutoOffS);
  put16(b + 8, c.battCalPermille);
  put16(b + 10, c.battLow10mV);
  put16(b + 12, c.battCrit10mV);
  b[14] = c.brakeFlash;
  b[15] = c.tailWithFront;
  b[16] = c.ledBrightness;
  b[17] = c.ledMode;
  b[18] = c.lightsOnBoot;
}

static void refreshConfig() {
  uint8_t b[CFG_LEN];
  buildConfig(b);
  ble::publishConfig(b, CFG_LEN);
}

// ---------------------------------------------------------------------
//  Příkazy (BLE i konzole)
// ---------------------------------------------------------------------
static void handleCommand(const uint8_t *d, size_t len) {
  if (len == 0) return;
  uint8_t a = len > 1 ? d[1] : 0;
  switch (d[0]) {
    case CMD_FRONT:      vehicle::setFront(a); break;
    case CMD_TAIL:       vehicle::setTail(a); break;
    case CMD_LIGHTS:     vehicle::setLights(a); break;
    case CMD_BLINK:      vehicle::setBlink(a); break;
    case CMD_LED:        vehicle::setLed(a); break;
    case CMD_LED_BRIGHT: vehicle::setLedBrightness(a); break;
    case CMD_LED_MODE:   vehicle::setLedMode(a); break;
    case CMD_ALL_OFF:    vehicle::allOff(); break;
    case CMD_SET_CFG:
      if (len >= 4 && settings::setKey(d[1], get16(d + 2))) {
        if (d[1] == CFG_LED_BRIGHT) vehicle::setLedBrightness(settings::cfg.ledBrightness);
        if (d[1] == CFG_LED_MODE) vehicle::setLedMode(settings::cfg.ledMode);
        refreshConfig();
      }
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
    case CMD_REBOOT:
      rebootPending = true;
      rebootAt = millis() + 300;
      break;
  }
}

// ---------------------------------------------------------------------
//  Tlačítka na řídítkách
// ---------------------------------------------------------------------
static void onButton(Button b, BtnEvent e) {
  switch (b) {
    case BTN_LIGHT:
      if (e == BtnEvent::Click) vehicle::setLights(ARG_TOGGLE);
      else if (e == BtnEvent::LongPress) vehicle::setLed(ARG_TOGGLE);
      break;
    case BTN_LEFT:
      if (e == BtnEvent::Click) vehicle::toggleBlink(BLINK_LEFT);
      else if (e == BtnEvent::LongPress) vehicle::toggleBlink(BLINK_HAZARD);
      break;
    case BTN_RIGHT:
      if (e == BtnEvent::Click) vehicle::toggleBlink(BLINK_RIGHT);
      else if (e == BtnEvent::LongPress) vehicle::toggleBlink(BLINK_HAZARD);
      break;
    case BTN_MODE:
      if (e == BtnEvent::Click) vehicle::cycleLedMode();
      else if (e == BtnEvent::LongPress) vehicle::toggleBlink(BLINK_HAZARD);
      break;
    case BTN_BRAKE:
      if (e == BtnEvent::Press) vehicle::setBrakeInput(true);
      else if (e == BtnEvent::Release) vehicle::setBrakeInput(false);
      break;
    default: break;
  }
}

// ---------------------------------------------------------------------
//  Sériová konzole na UART0 (USB-UART převodník na desce)
// ---------------------------------------------------------------------
#if GOGEN_CONSOLE
static char line[48];
static uint8_t lineLen = 0;

static uint8_t onoff(const char *s) {
  if (!s) return ARG_TOGGLE;
  if (!strcmp(s, "on") || !strcmp(s, "1")) return ARG_ON;
  if (!strcmp(s, "off") || !strcmp(s, "0")) return ARG_OFF;
  return ARG_TOGGLE;
}

static void printStatus() {
  const Settings &c = settings::cfg;
  Serial.printf("GoGEN OS v%d.%d  BLE=%s %s\n", GOGEN_FW_MAJOR, GOGEN_FW_MINOR, ble::name(),
                ble::connected() ? "(pripojeno)" : "");
  Serial.printf("Baterie %.2f V  %u %%  ADC %u mV  cal %u\n", battery::volts(),
                battery::percent(), battery::adcMilliVolts(), c.battCalPermille);
  Serial.printf("Vystupy 0x%02X  blink %u  LED %u/%u  brzda %d  tlacitka 0x%02X  PCF %s  OLED %s\n",
                vehicle::outputsMask(), vehicle::blinkMode(), vehicle::ledBrightness(),
                vehicle::ledMode(), vehicle::brakeInput(), inputs::rawMask(),
                inputs::expanderOk() ? "ok" : "CHYBA", display::ok() ? "ok" : "-");
}

static void consoleExec(char *s) {
  char *cmd = strtok(s, " ");
  char *arg = strtok(nullptr, " ");
  char *arg2 = strtok(nullptr, " ");
  if (!cmd) return;
  uint8_t d[5] = {0};
  if (!strcmp(cmd, "status") || !strcmp(cmd, "st")) printStatus();
  else if (!strcmp(cmd, "front")) { d[0] = CMD_FRONT; d[1] = onoff(arg); handleCommand(d, 2); }
  else if (!strcmp(cmd, "tail"))  { d[0] = CMD_TAIL; d[1] = onoff(arg); handleCommand(d, 2); }
  else if (!strcmp(cmd, "lights")){ d[0] = CMD_LIGHTS; d[1] = onoff(arg); handleCommand(d, 2); }
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
    Serial.println(settings::setKey(atoi(arg), atoi(arg2)) ? "OK (ulozit: save)" : "Neplatne");
    refreshConfig();
  }
  else if (!strcmp(cmd, "save")) { settings::save(); Serial.println("Ulozeno"); }
  else if (!strcmp(cmd, "pin") && arg) {
    d[0] = CMD_SET_PASSKEY; uint32_t pk = atol(arg);
    for (int i = 0; i < 4; i++) d[1 + i] = pk >> (8 * i);
    handleCommand(d, 5);
  }
  else if (!strcmp(cmd, "factory")) { d[0] = CMD_FACTORY; handleCommand(d, 1); }
  else if (!strcmp(cmd, "reboot")) ESP.restart();
  else Serial.println("Prikazy: status | front/tail/lights [on|off] | left | right | hazard |"
                      " led [on|off|0-255] | mode 0-3 | off | cal <V> | set <klic> <hodnota> | save |"
                      " pin <000000> | factory | reboot");
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
  vehicle::begin();              // jako první: všechny výstupy bezpečně vypnout
#if GOGEN_CONSOLE
  Serial.begin(115200);
#endif
  settings::load();
  vehicle::begin();              // znovu s načteným nastavením

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ);
  Wire.setTimeOut(20);
  inputs::begin(onButton);
  display::begin();
  battery::begin();
  battery::poll();
  ble::begin(handleCommand);
  refreshConfig();

  esp_task_wdt_init(WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);

  LOG("\nGoGEN OS v%d.%d start, BLE %s, PIN %06u\n", GOGEN_FW_MAJOR, GOGEN_FW_MINOR, ble::name(),
      (unsigned)settings::passkey());
}

void loop() {
  uint32_t now = millis();
  esp_task_wdt_reset();

  ble::process();
#if GOGEN_CONSOLE
  consolePoll();
#endif

  if (now - tInput >= INPUT_POLL_MS) { tInput = now; inputs::poll(now); }
  if (now - tBatt >= BATTERY_POLL_MS) { tBatt = now; battery::poll(); }

  vehicle::tick(now);

  // BLE notifikace: při změně (max 20×/s) nebo periodicky
  uint8_t st[STATE_LEN];
  buildState(st, now);
  bool changed = memcmp(st, lastState, 8) != 0 || memcmp(st + 12, lastState + 12, 2) != 0;
  if ((changed && now - tLastNotify >= 50) || now - tNotify >= BLE_NOTIFY_MS) {
    ble::publishState(st, STATE_LEN);
    memcpy(lastState, st, STATE_LEN);
    tNotify = tLastNotify = now;
  }

  if (now - tDisp >= DISPLAY_REFRESH_MS) { tDisp = now; display::render(now, ble::connected()); }

  if (rebootPending && (int32_t)(now - rebootAt) >= 0) ESP.restart();

  delay(LOOP_TICK_MS);
}
