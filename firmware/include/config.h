#pragma once
// =====================================================================
//  GoGEN OS 3 – konfigurace hardwaru (GoGEN V2, jedna ESP-WROOM-32)
// =====================================================================
#include <Arduino.h>

#define GOGEN_FW_MAJOR 3
#define GOGEN_FW_MINOR 0

// ---------------------------------------------------------------------
//  Režim vstupů: 1 = PCF8574 přes I2C, 0 = přímé GPIO
//  (nastavuje se v platformio.ini přes -DGOGEN_INPUT_MODE)
// ---------------------------------------------------------------------
#ifndef GOGEN_INPUT_MODE
#define GOGEN_INPUT_MODE 1
#endif
#define INPUT_MODE_GPIO 0
#define INPUT_MODE_PCF  1

// Rychloměr a přední pohon potřebují GPIO4/14/18/19, které varianta
// s tlačítky na GPIO používá pro ovladač → jsou jen ve variantě PCF.
#define FEATURE_SPEED  (GOGEN_INPUT_MODE == INPUT_MODE_PCF)
#define FEATURE_DRIVE  (GOGEN_INPUT_MODE == INPUT_MODE_PCF)
#define FEATURE_CHARGE 1
#define FEATURE_OTA    1

// =====================================================================
//  PINOUT v4 – ESP-WROOM-32 (ESP32 DevKit), jediná řídicí jednotka
//
//  GPIO  typ            signál       připojeno k
//  --------------------------------------------------------------------
//  34    ADC1_CH6, IN   BAT_SENSE    dělič 120k/7,5k + 100 nF
//  35    ADC1_CH7, IN   GEN_SENSE    dělič 270k/10k + 100 nF (generátor)
//  36    ADC1_CH0, IN   THROTTLE_IN  plyn primáru přes lineární optočlen
//  39    ADC1_CH3, IN   (rezerva)    např. měření proudu
//  13                   BRAKE_IN     brzdová páka → GND, pull-up, probuzení
//  25    PWM            LED          Q7 low-side – LED pásky
//  26                   FRONT        Q1 low-side – přední světlo
//  27                   TAIL         HS1 high-side – koncové světlo
//  33                   BRAKE        HS2 high-side – brzdové světlo
//  32                   LEFT         Q4 low-side – blinkry L (přední + zadní)
//  23                   RIGHT        Q5 low-side – blinkry P (přední + zadní)
//  21/22 I2C            SDA/SCL      PCF8574 0x20, OLED 0x3C, BH1750 0x23,
//                                    MPU6050 0x68, MCP4725 0x60
//  16                   PWR_HOLD     drží napájecí latch (LOW = vypnout)
//  17                   CHG_EN       povolení nabíjení z předního kola
//   4    IN, IRQ        REAR_SPEED   hall zadního motoru přes optočlen
//  14    IN, IRQ        FRONT_SPEED  hall předního motoru (dělič z 5 V!)
//  18                   PHASE_EN     relé fází předního motoru
//  19                   FRONT_BRAKE  brzdový vstup předního kontroléru
//  1/3   UART0          konzole + nahrávání přes USB-UART – nezapojovat
//
//  GPIO36/39 mají errata (falešné krátké pulzy při běhu ADC/Wi-Fi),
//  proto jsou jen jako analogové vstupy, nikdy ne na přerušení.
//  NEPOUŽÍVAT: 6–11 (flash), 12 (strapping, při bootu LOW), 0/2/5/15.
//  Všechny výstupy po resetu plavou → 10 kΩ pulldown na každém gate.
//
//  Varianta gogen_gpio: tlačítka na 4/18/19/14 → bez rychloměru a pohonu.
// =====================================================================

// ---------------------------------------------------------------------
//  Výstupy osvětlení
// ---------------------------------------------------------------------
#define PIN_LED       25
#define PIN_FRONT     26
#define PIN_TAIL      27
#define PIN_BRAKE     33
#define PIN_LEFT      32
#define PIN_RIGHT     23

// Polarita výstupů (true = log. 1 zapíná). High-side moduly ověř!
#define OUT_ACTIVE_HIGH true

#define LED_PWM_CH     0
#define LED_PWM_FREQ   5000
#define LED_PWM_BITS   8

// ---------------------------------------------------------------------
//  Napájení, nabíjení, přední pohon
// ---------------------------------------------------------------------
#define PIN_PWR_HOLD   16
#define PIN_CHG_EN     17
#define PIN_PHASE_EN   18
#define PIN_FRONT_BRK  19
#define PIN_REAR_SPEED  4
#define PIN_FRONT_SPEED 14

// ---------------------------------------------------------------------
//  Analogové vstupy (vše ADC1 – ADC2 nejde používat současně s Wi-Fi/BLE)
// ---------------------------------------------------------------------
#define PIN_BAT_SENSE  34
#define PIN_GEN_SENSE  35
#define PIN_THROTTLE   36

#define BAT_R1_OHM     120000.0f
#define BAT_R2_OHM       7500.0f   // 42 V → 2,47 V
#define BAT_CELLS      10
#define BAT_SAMPLES    32

#define GEN_R1_OHM     270000.0f
#define GEN_R2_OHM      10000.0f   // 70 V → 2,5 V

// ---------------------------------------------------------------------
//  I2C sběrnice
// ---------------------------------------------------------------------
#define PIN_I2C_SDA   21
#define PIN_I2C_SCL   22
#define I2C_FREQ       400000
#define OLED_ADDR      0x3C
#define PCF8574_ADDR   0x20
#define BH1750_ADDR    0x23
#define MPU6050_ADDR   0x68
#define MCP4725_ADDR   0x60

// ---------------------------------------------------------------------
//  Vstupy
// ---------------------------------------------------------------------
#define PIN_BRAKE_IN  13
#define BRAKE_ACTIVE_LOW true

#if GOGEN_INPUT_MODE == INPUT_MODE_PCF
  #define PCF_BIT_LIGHT  0
  #define PCF_BIT_LEFT   1
  #define PCF_BIT_RIGHT  2
  #define PCF_BIT_MODE   3
#else
  #define PIN_BTN_LIGHT   4
  #define PIN_BTN_LEFT   18
  #define PIN_BTN_RIGHT  19
  #define PIN_BTN_MODE   14
#endif

#define GOGEN_CONSOLE  1

// ---------------------------------------------------------------------
//  BLE, Wi-Fi OTA
// ---------------------------------------------------------------------
#define BLE_NAME_PREFIX "GoGEN-"
#define BLE_SECURE      1
#define BLE_DEFAULT_PASSKEY 123456

#define OTA_TIMEOUT_S   600       // AP se sám vypne po 10 min
#define OTA_MAX_KMH     1.0f      // aktualizace jen ve stoji

// ---------------------------------------------------------------------
//  Časování
// ---------------------------------------------------------------------
#define LOOP_TICK_MS        5
#define INPUT_POLL_MS      10
#define DEBOUNCE_MS        30
#define LONG_PRESS_MS     800
#define COMBO_OFF_MS     2000     // L + P držet 2 s = vypnout
#define BATTERY_POLL_MS   100
#define SENSOR_POLL_MS     10
#define LUX_POLL_MS       250
#define DRIVE_TICK_MS      10
#define DISPLAY_REFRESH_MS 200
#define BLE_NOTIFY_MS     250
#define WDT_TIMEOUT_S       2     // zaseknutá smyčka → reset → fáze se odpojí (pulldown)
