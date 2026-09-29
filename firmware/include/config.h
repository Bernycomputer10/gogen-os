#pragma once
// =====================================================================
//  GoGEN OS – konfigurace hardwaru (GoGEN V2, dokumentace rev 0.1)
// =====================================================================
#include <Arduino.h>

#define GOGEN_FW_MAJOR 2
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

// =====================================================================
//  PINOUT v2 (bez klaksonu)
//
//  ESP32-C3 Super Mini            funkce
//  ------------------------------------------------------------------
//  GPIO0   ADC1_CH0   BAT_SENSE   dělič baterie
//  GPIO1              BRAKE_IN    brzdová páka → GND (interní pull-up)
//  GPIO2   strapping  BTN_LIGHT   jen GPIO varianta, jinak volný
//  GPIO3   PWM        LED         Q7 low-side – LED pásky
//  GPIO4              FRONT       Q1 low-side – přední světlo
//  GPIO5              TAIL        HS1 high-side – zadní TAIL
//  GPIO6              BRAKE       HS2 high-side – zadní BRAKE
//  GPIO7              LEFT        Q4 low-side – levé blinkry
//  GPIO8   strapping  I2C SDA     OLED + PCF8574 (pull-up 4k7)
//  GPIO9   strapping  I2C SCL     (BOOT tlačítko na desce)
//  GPIO10             RIGHT       Q5 low-side – pravé blinkry
//  GPIO18/19          USB         nechány volné → USB konzole a nahrávání vždy fungují
//  GPIO20  UART RX    BTN_LEFT    jen GPIO varianta, jinak volný
//  GPIO21  UART TX    BTN_RIGHT   jen GPIO varianta – 1k do série (ROM log při bootu)
// =====================================================================

// ---------------------------------------------------------------------
//  Výstupy
// ---------------------------------------------------------------------
#define PIN_LED        3   // Q7  low-side PWM       – LED pásky
#define PIN_FRONT      4   // Q1  low-side N-MOSFET  – přední světlo
#define PIN_TAIL       5   // HS1 high-side          – zadní TAIL
#define PIN_BRAKE      6   // HS2 high-side          – zadní BRAKE
#define PIN_LEFT       7   // Q4  low-side           – levé blinkry
#define PIN_RIGHT     10   // Q5  low-side           – pravé blinkry
#define PIN_BAT_SENSE  0   // ADC1_CH0, dělič 120k / 8,2k (doporučeno 7,5k)

// Polarita výstupů (true = log. 1 zapíná). High-side moduly ověř!
#define OUT_ACTIVE_HIGH true

// LED PWM (LEDC)
#define LED_PWM_CH     0
#define LED_PWM_FREQ   5000
#define LED_PWM_BITS   8

// ---------------------------------------------------------------------
//  I2C sběrnice (OLED + PCF8574). GPIO8/9 jsou strapping piny, ale
//  I2C s pull-upy je drží v log. 1 → bootování je v pořádku.
// ---------------------------------------------------------------------
#define PIN_I2C_SDA    8
#define PIN_I2C_SCL    9
#define I2C_FREQ       400000
#define OLED_ADDR      0x3C
#define PCF8574_ADDR   0x20

// ---------------------------------------------------------------------
//  Vstupy
// ---------------------------------------------------------------------
// Brzda je v obou variantách na přímém GPIO (rychlá odezva, není strapping)
#define PIN_BRAKE_IN   1

#if GOGEN_INPUT_MODE == INPUT_MODE_PCF
  // Piny PCF8574 (P0..P7), tlačítka spínají proti GND
  #define PCF_BIT_LIGHT  0
  #define PCF_BIT_LEFT   1
  #define PCF_BIT_RIGHT  2
  #define PCF_BIT_MODE   3
  // P4..P7 volné
#else
  // Přímé GPIO – tlačítko → GND, interní pull-up
  #define PIN_BTN_LIGHT   2  // strapping – nedrž tlačítko při zapínání
  #define PIN_BTN_LEFT   20
  #define PIN_BTN_RIGHT  21  // UART TX – zapoj přes 1k rezistor
  #define PIN_BTN_MODE   -1  // nezapojeno (LED se ovládají dlouhým stiskem Světla)
#endif

// USB konzole je teď dostupná v obou variantách
#define GOGEN_CONSOLE  1

// Brzdový spínač KT „Low-Level“: sepnuto = GND
#define BRAKE_ACTIVE_LOW true

// ---------------------------------------------------------------------
//  Měření baterie
// ---------------------------------------------------------------------
#define BAT_R1_OHM     120000.0f
#define BAT_R2_OHM       8200.0f
#define BAT_CELLS      10
#define BAT_SAMPLES    32

// ---------------------------------------------------------------------
//  BLE
// ---------------------------------------------------------------------
#define BLE_NAME_PREFIX "GoGEN-"
#define BLE_SECURE      1          // párování s PINem (passkey)
#define BLE_DEFAULT_PASSKEY 123456 // změň z aplikace nebo konzole!

// ---------------------------------------------------------------------
//  Časování
// ---------------------------------------------------------------------
#define LOOP_TICK_MS        5
#define INPUT_POLL_MS      10
#define DEBOUNCE_MS        30
#define LONG_PRESS_MS     800
#define BATTERY_POLL_MS   100
#define DISPLAY_REFRESH_MS 200
#define BLE_NOTIFY_MS     250
#define WDT_TIMEOUT_S       5
