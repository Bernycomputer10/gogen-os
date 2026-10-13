#pragma once
// =====================================================================
//  GoGEN BLE protokol v2 (bez klaksonu)  (zrcadleno v android-app/src/ble/protocol.ts)
// =====================================================================
#include <stdint.h>

#define GOGEN_SVC_UUID    "a8f30001-4c1e-4b8e-9a52-6f6f67656e32"
#define GOGEN_STATE_UUID  "a8f30002-4c1e-4b8e-9a52-6f6f67656e32"  // READ | NOTIFY
#define GOGEN_CMD_UUID    "a8f30003-4c1e-4b8e-9a52-6f6f67656e32"  // WRITE
#define GOGEN_CFG_UUID    "a8f30004-4c1e-4b8e-9a52-6f6f67656e32"  // READ
#define GOGEN_TELEM_UUID  "a8f30005-4c1e-4b8e-9a52-6f6f67656e32"  // NOTIFY      (v3)
#define GOGEN_RIDE_UUID   "a8f30006-4c1e-4b8e-9a52-6f6f67656e32"  // READ        (v3, zatím neimplementováno)
#define GOGEN_CHARGE_UUID "a8f30007-4c1e-4b8e-9a52-6f6f67656e32"  // READ|NOTIFY (v3)

#define PROTO_VERSION 2

// ---------------- STATE (20 B, little-endian) ----------------
//  0  u8  version
//  1  u8  outputs  (bit OUT_*)
//  2  u8  led brightness 0..255
//  3  u8  led mode (LED_MODE_*)
//  4  u16 battery voltage [10 mV]
//  6  u8  battery %
//  7  u8  status flags (bit ST_*)
//  8  u32 uptime [s]
// 12  u8  blink mode (BLINK_*)
// 13  u8  input buttons raw (bit = pressed)
// 14  u16 trip seconds since power-on with lights on (reserve)
// 16  u32 reserved
#define STATE_LEN 20

enum : uint8_t {
  OUT_FRONT = 1 << 0,
  OUT_TAIL  = 1 << 1,
  OUT_BRAKE = 1 << 2,
  OUT_LEFT  = 1 << 3,   // aktuální fyzický stav (bliká)
  OUT_RIGHT = 1 << 4,
  OUT_LED   = 1 << 5,   // LED pásky povoleny
};

enum : uint8_t {
  ST_BLE_BONDED  = 1 << 0,
  ST_BAT_LOW     = 1 << 1,
  ST_BAT_CRIT    = 1 << 2,
  ST_INPUT_PCF   = 1 << 3,
  ST_INPUT_OK    = 1 << 4,  // PCF odpovídá (nebo GPIO režim)
  ST_DISPLAY_OK  = 1 << 5,
  ST_BRAKE_IN    = 1 << 6,  // brzdová páka stisknuta
};

enum : uint8_t { BLINK_OFF = 0, BLINK_LEFT = 1, BLINK_RIGHT = 2, BLINK_HAZARD = 3 };
enum : uint8_t { LED_MODE_STATIC = 0, LED_MODE_BREATHE = 1, LED_MODE_STROBE = 2, LED_MODE_BRAKE = 3, LED_MODE_COUNT = 4 };

// ---------------- COMMANDS (zápis do CMD) ----------------
//  [cmd, args...]   arg ON/OFF/TOGGLE = 1/0/2
enum : uint8_t {
  CMD_FRONT       = 0x01,  // u8 on/off/toggle
  CMD_TAIL        = 0x02,  // u8 on/off/toggle
  CMD_BLINK       = 0x03,  // u8 BLINK_*
  CMD_LED         = 0x05,  // u8 on/off/toggle
  CMD_LED_BRIGHT  = 0x06,  // u8 0..255
  CMD_LED_MODE    = 0x07,  // u8 LED_MODE_*
  CMD_LIGHTS      = 0x08,  // u8 on/off/toggle – přední + zadní současně
  CMD_ALL_OFF     = 0x09,
  CMD_SET_CFG     = 0x10,  // u8 key, u16 value
  CMD_SAVE_CFG    = 0x11,
  CMD_CALIBRATE   = 0x12,  // u16 skutečné napětí [10 mV]
  CMD_SET_PASSKEY = 0x13,  // u32 passkey (platí po restartu)
  CMD_FACTORY     = 0x14,  // reset nastavení + smazání párování
  CMD_REBOOT      = 0x20,
  // ---- v3 ----
  CMD_TRIP_RESET   = 0x32,
  CMD_CHARGE_MODE  = 0x40,  // u8 CHG_MODE_*
  CMD_CHARGE_START = 0x41,  // jen v ručním režimu
  CMD_CHARGE_STOP  = 0x42,
  CMD_CHARGE_SET   = 0x43,  // u8 key, u16 value (CHGKEY_*)
};
enum : uint8_t { ARG_OFF = 0, ARG_ON = 1, ARG_TOGGLE = 2 };

// ---------------- CONFIG (čtení, 24 B) ----------------
//  0  u8  version
//  1  u8  fw major   2 u8 fw minor   3 u8 input mode
//  4  u16 blinkPeriodMs      (key 1)
//  6  u16 blinkAutoOffS      (key 2)  0 = vypnuto
//  8  u16 battCalPermille    (key 3)
// 10  u16 battLow [10 mV]    (key 4)
// 12  u16 battCrit [10 mV]   (key 5)
// 14  u8  brakeFlash         (key 6)
// 15  u8  tailWithFront      (key 7)
// 16  u8  ledBrightnessDef   (key 8)
// 17  u8  ledModeDef         (key 9)
// 18  u8  lightsOnBoot       (key 10)
// 19..23  reserved
#define CFG_LEN 24
enum : uint8_t {
  CFG_BLINK_PERIOD = 1, CFG_BLINK_AUTOOFF, CFG_BATT_CAL, CFG_BATT_LOW, CFG_BATT_CRIT,
  CFG_BRAKE_FLASH, CFG_TAIL_WITH_FRONT, CFG_LED_BRIGHT, CFG_LED_MODE, CFG_LIGHTS_ON_BOOT,
};

// =====================================================================
//  v3 – telemetrie z řadiče KT (UART2, jen poslech) – TELEM, notify 20 B
//  Pole, která firmware nezná, mají vyhrazenou hodnotu "není k dispozici":
//   0  u8  version (3)
//   1  u8  flags (TF_*)
//   2  u16 rychlost [0,1 km/h]          TELEM_NA_U16 = není (kolo nenastaveno / řadič mlčí)
//   4  i16 proud [0,1 A]                TELEM_NA_I16
//   6  u16 napětí hlavní baterie [10mV] TELEM_NA_U16
//   8  u8  hlavní baterie [%]           TELEM_NA_U8
//   9  i8  teplota motoru [°C]          TELEM_NA_I8
//  10  i8  teplota řadiče [°C]          TELEM_NA_I8
//  11  u8  kód chyby
//  12  u32 celkem ujeto [m]
//  16  u32 trip [m]
// =====================================================================
#define TELEM_LEN 20
#define TELEM_NA_U16 0xFFFF
#define TELEM_NA_I16 0x7FFF
#define TELEM_NA_U8  0xFF
#define TELEM_NA_I8  (-128)
enum : uint8_t { TF_MOVING = 1 << 0, TF_FAULT = 1 << 1, TF_BRAKE = 1 << 2, TF_CRUISE = 1 << 3, TF_LINK = 1 << 4 };

// ---------------------------------------------------------------------
//  v3 – dobíjení sekundární baterie – CHARGE, read/notify 12 B
//   0 u8 version   1 u8 mode   2 u8 phase   3 u8 cíl [%]
//   4 u8 proudová pojistka [0,1 A]   5 u8 flags (CF_HAS_SENSE)
//   6 i16 proud [10 mA] (jen s CF_HAS_SENSE, jinak 0)   8 u16 doba [min]   10 u16 rezerva
// ---------------------------------------------------------------------
#define CHARGE_LEN 12
enum : uint8_t { CHG_MODE_AUTO = 0, CHG_MODE_MANUAL = 1 };
enum : uint8_t { CHG_IDLE = 0, CHG_CHARGING = 1, CHG_FULL = 2, CHG_PAUSED = 3, CHG_FAULT = 4 };
enum : uint8_t { CHGKEY_TARGET = 1, CHGKEY_LIMIT_DECI_A = 2 };
enum : uint8_t { CF_HAS_SENSE = 1 << 0 };
