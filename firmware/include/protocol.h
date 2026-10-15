#pragma once
// =====================================================================
//  GoGEN BLE protokol v3  (zrcadleno v android-app/src/ble/protocol.ts)
// =====================================================================
#include <stdint.h>

#define GOGEN_SVC_UUID    "a8f30001-4c1e-4b8e-9a52-6f6f67656e32"
#define GOGEN_STATE_UUID  "a8f30002-4c1e-4b8e-9a52-6f6f67656e32"  // READ | NOTIFY
#define GOGEN_CMD_UUID    "a8f30003-4c1e-4b8e-9a52-6f6f67656e32"  // WRITE
#define GOGEN_CFG_UUID    "a8f30004-4c1e-4b8e-9a52-6f6f67656e32"  // READ
#define GOGEN_TELEM_UUID  "a8f30005-4c1e-4b8e-9a52-6f6f67656e32"  // READ | NOTIFY

#define PROTO_VERSION 3

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
// 13  u8  input buttons (bit = stisknuto)
// 14  u8  light mode (LIGHT_*)
// 15  u8  status flags 2 (bit ST2_*)
// 16  u16 rychlost [0,1 km/h]  (zadní kolo, jinak přední)
// 18  u8  stav nabíjení (CHG_*)
// 19  u8  stav předního pohonu (DRV_*)
#define STATE_LEN 20

enum : uint8_t {
  OUT_FRONT = 1 << 0,
  OUT_TAIL  = 1 << 1,
  OUT_BRAKE = 1 << 2,
  OUT_LEFT  = 1 << 3,   // fyzický stav (bliká)
  OUT_RIGHT = 1 << 4,
  OUT_LED   = 1 << 5,   // LED pásky povoleny
  OUT_CHG   = 1 << 6,   // CHG_EN sepnuto
  OUT_PHASE = 1 << 7,   // relé fází předního motoru sepnuto
};

enum : uint8_t {
  ST_BLE_BONDED  = 1 << 0,
  ST_BAT_LOW     = 1 << 1,
  ST_BAT_CRIT    = 1 << 2,
  ST_INPUT_PCF   = 1 << 3,
  ST_INPUT_OK    = 1 << 4,
  ST_DISPLAY_OK  = 1 << 5,
  ST_BRAKE_IN    = 1 << 6,  // páka stisknuta
  ST_AUTO_BRAKE  = 1 << 7,  // brzdové světlo z akcelerometru
};

enum : uint8_t {
  ST2_LUX_OK     = 1 << 0,  // BH1750 odpovídá
  ST2_MPU_OK     = 1 << 1,  // MPU6050 odpovídá
  ST2_DAC_OK     = 1 << 2,  // MCP4725 odpovídá
  ST2_SPEED_OK   = 1 << 3,  // rychlost je platná (přišly pulzy)
  ST2_EMERGENCY  = 1 << 4,  // prudké brzdění
  ST2_OTA        = 1 << 5,  // Wi-Fi AP pro aktualizaci běží
  ST2_OFF_SOON   = 1 << 6,  // za < 60 s automatické vypnutí
  ST2_DRIVE_HW   = 1 << 7,  // firmware podporuje přední pohon (varianta PCF)
};

enum : uint8_t { BLINK_OFF = 0, BLINK_LEFT = 1, BLINK_RIGHT = 2, BLINK_HAZARD = 3 };
enum : uint8_t { LED_MODE_STATIC = 0, LED_MODE_BREATHE = 1, LED_MODE_STROBE = 2, LED_MODE_BRAKE = 3, LED_MODE_COUNT = 4 };
enum : uint8_t { LIGHT_OFF = 0, LIGHT_ON = 1, LIGHT_AUTO = 2 };
// shodné s chg::State a drv::State
enum : uint8_t { CHG_IDLE = 0, CHG_WAITING = 1, CHG_CHARGING = 2, CHG_FULL = 3, CHG_FAULT = 4 };
enum : uint8_t { DRV_OFF = 0, DRV_ARMING = 1, DRV_READY = 2, DRV_ACTIVE = 3, DRV_RELEASING = 4, DRV_FAULT = 5 };

// ---------------- TELEMETRY (32 B) ----------------
//  0  u8  version
//  1  u8  charge mode (0 auto, 1 off, 2 force)
//  2  u8  charge reason bits (chg::Reason)
//  3  u8  charge fault (chg::Fault)
//  4  u8  drive reason bits (drv::Reason)
//  5  u8  drive fault (drv::Fault)
//  6  u8  výstup DAC předního pohonu [%]
//  7  u8  plyn jezdce [%]
//  8  u16 zadní kolo [0,1 km/h]
// 10  u16 přední kolo [0,1 km/h]
// 12  u16 napětí generátoru [0,1 V]
// 14  u16 osvětlení [lx] (65535 = bez senzoru)
// 16  i16 zpomalení [mg] (kladné = brzdí)
// 18  u32 celkem ujeto [m]
// 22  u32 výlet [m]
// 26  u16 do automatického vypnutí [s] (65535 = vypnuto)
// 28  u16 nabíjení běží [min]
// 30  u16 rezerva
#define TELEM_LEN 32

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
  CMD_LIGHT_MODE  = 0x0A,  // u8 LIGHT_*
  CMD_SET_CFG     = 0x10,  // u8 key, u16 value
  CMD_SAVE_CFG    = 0x11,
  CMD_CALIBRATE   = 0x12,  // u16 skutečné napětí [10 mV]
  CMD_SET_PASSKEY = 0x13,  // u32 passkey (platí po restartu)
  CMD_FACTORY     = 0x14,  // reset nastavení + smazání párování
  CMD_POWER_OFF   = 0x15,
  CMD_CHG_MODE    = 0x16,  // u8 0 auto, 1 off, 2 force
  CMD_CHG_RESET   = 0x17,  // zrušení poruchy nabíjení
  CMD_DRIVE       = 0x18,  // u8 on/off/toggle – zapnutí předního pohonu
  CMD_DRIVE_RESET = 0x19,  // zrušení poruchy pohonu
  CMD_THR_CAL     = 0x1A,  // u8 0 = plyn puštěný (min), 1 = plný plyn (max)
  CMD_TRIP_RESET  = 0x1B,
  CMD_REBOOT      = 0x20,
  CMD_OTA         = 0x21,  // u8 1 = spustit Wi-Fi AP, 0 = zastavit
};
enum : uint8_t { ARG_OFF = 0, ARG_ON = 1, ARG_TOGGLE = 2 };

// ---------------- CONFIG (čtení) ----------------
//  0  u8  version   1 u8 fw major   2 u8 fw minor   3 u8 input mode
//  4  u16 × CFG_KEY_COUNT  – hodnota klíče k je na offsetu 4 + 2·(k−1)
//  potom u8 hw flags (bit HW_*)
enum : uint8_t {
  CFG_BLINK_PERIOD = 1,  // ms 300..2000
  CFG_BLINK_AUTOOFF,     // s 0..600 (0 = nevypínat)
  CFG_BATT_CAL,          // ‰ 800..1200
  CFG_BATT_LOW,          // 10 mV
  CFG_BATT_CRIT,         // 10 mV – vypne LED pásky
  CFG_BRAKE_FLASH,       // 0/1
  CFG_TAIL_WITH_FRONT,   // 0/1
  CFG_LED_BRIGHT,        // 0..255
  CFG_LED_MODE,          // LED_MODE_*
  CFG_LIGHTS_ON_BOOT,    // LIGHT_* po startu
  CFG_LUX_ON,            // lx – pod tím rozsvítit (AUTO)
  CFG_LUX_OFF,           // lx – nad tím zhasnout
  CFG_AUTO_BRAKE,        // 0/1
  CFG_BRAKE_DECEL,       // mg – práh zpomalení
  CFG_MPU_AXIS,          // 0..5 = +X −X +Y −Y +Z −Z (osa ve směru jízdy)
  CFG_AUTO_OFF_MIN,      // min nečinnosti do vypnutí (0 = nikdy)
  CFG_BATT_OFF,          // 10 mV – pod tím 30 s → vypnutí (ochrana článků)
  CFG_CHG_START,         // % – sepnout nabíjení
  CFG_CHG_STOP,          // % – vypnout nabíjení
  CFG_CHG_MODE,          // výchozí režim po startu 0 auto / 1 off
  CFG_WHEEL_MM,          // obvod kola [mm]
  CFG_REAR_PPR,          // pulzů na otáčku – zadní hall
  CFG_FRONT_PPR,         // pulzů na otáčku – přední hall
  CFG_DRV_ALLOWED,       // 0/1 – přední pohon povolen
  CFG_DRV_ASSIST,        // % podílu plynu pro přední motor
  CFG_DRV_MAX_KMH,       // km/h – nad tím odpojit fáze
  CFG_DRV_MIN_BATT,      // % – pod tím pohon vypnout
  CFG_DRV_KP,            // ×100
  CFG_DRV_KI,            // ×100
  CFG_THR_MIN_MV,        // mV na ADC při puštěném plynu
  CFG_THR_MAX_MV,        // mV na ADC při plném plynu
  CFG_DAC_IDLE,          // 10 mV – výstup DAC v klidu
  CFG_DAC_START,         // 10 mV – začátek reakce kontroléru
  CFG_DAC_MAX,           // 10 mV – plný plyn kontroléru
  CFG_KEY_COUNT_PLUS_ONE
};
#define CFG_KEY_COUNT (CFG_KEY_COUNT_PLUS_ONE - 1)
#define CFG_LEN (4 + 2 * CFG_KEY_COUNT + 1)

enum : uint8_t {
  HW_SPEED = 1 << 0,
  HW_DRIVE = 1 << 1,
  HW_CHARGE = 1 << 2,
  HW_OTA = 1 << 3,
};
