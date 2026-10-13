#pragma once
// =====================================================================
//  GoGEN OS – konfigurace hardwaru (GoGEN V2, ESP-WROOM-32)
// =====================================================================
#include <Arduino.h>

#define GOGEN_FW_MAJOR 2
#define GOGEN_FW_MINOR 2

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
//  PINOUT v3 – ESP-WROOM-32 (ESP32 DevKit), bez klaksonu
//
//  GPIO    typ               funkce      poznámka
//  ------------------------------------------------------------------
//  GPIO34  ADC1_CH6, jen IN  BAT_SENSE   dělič baterie (ADC2 nejde používat s BLE)
//  GPIO13                    BRAKE_IN    brzdová páka → GND (interní pull-up)
//  GPIO25  PWM               LED         Q7 low-side – LED pásky
//  GPIO26                    FRONT       Q1 low-side – přední světlo
//  GPIO27                    TAIL        HS1 high-side – zadní TAIL
//  GPIO33                    BRAKE       HS2 high-side – zadní BRAKE
//  GPIO32                    LEFT        Q4 low-side – levé blinkry
//  GPIO23                    RIGHT       Q5 low-side – pravé blinkry
//  GPIO21  I2C SDA           OLED + PCF8574 (pull-up 4k7)
//  GPIO22  I2C SCL
//  GPIO4                     BTN_LIGHT   jen GPIO varianta
//  GPIO18                    BTN_LEFT    jen GPIO varianta
//  GPIO19                    BTN_RIGHT   jen GPIO varianta
//  GPIO14                    BTN_MODE    jen GPIO varianta (na WROOM-32 je pinů dost)
//  GPIO1/3 UART0             konzole + nahrávání přes USB-UART převodník – nezapojovat
//  GPIO16/17 UART2           rezerva: propojka na zadní jednotku / telemetrie KT
//  GPIO35/36/39 jen IN       rezerva ADC1 (např. měření proudu)
//
//  NEPOUŽÍVAT: GPIO6–11 (interní SPI flash), GPIO12 (strapping – při bootu
//  musí být LOW, jinak 1,8V flash), GPIO0/2/5/15 (strapping).
//  Výstupy 23/25/26/27/32/33 po resetu plavou → na každém gate 10k pulldown.
// =====================================================================

// ---------------------------------------------------------------------
//  Výstupy
// ---------------------------------------------------------------------
#define PIN_LED       25   // Q7  low-side PWM       – LED pásky
#define PIN_FRONT     26   // Q1  low-side N-MOSFET  – přední světlo
#define PIN_TAIL      27   // HS1 high-side          – zadní TAIL
#define PIN_BRAKE     33   // HS2 high-side          – zadní BRAKE
#define PIN_LEFT      32   // Q4  low-side           – levé blinkry
#define PIN_RIGHT     23   // Q5  low-side           – pravé blinkry
#define PIN_BAT_SENSE 34   // ADC1_CH6, dělič 120k / 7,5k

// Polarita výstupů (true = log. 1 zapíná). High-side moduly ověř!
#define OUT_ACTIVE_HIGH true

// LED PWM (LEDC)
#define LED_PWM_CH     0
#define LED_PWM_FREQ   5000
#define LED_PWM_BITS   8

// ---------------------------------------------------------------------
//  I2C sběrnice (OLED + PCF8574) – výchozí piny ESP32
// ---------------------------------------------------------------------
#define PIN_I2C_SDA   21
#define PIN_I2C_SCL   22
#define I2C_FREQ       400000
#define OLED_ADDR      0x3C
#define PCF8574_ADDR   0x20

// ---------------------------------------------------------------------
//  Vstupy
// ---------------------------------------------------------------------
// Brzda je v obou variantách na přímém GPIO (rychlá odezva, není strapping)
#define PIN_BRAKE_IN  13

#if GOGEN_INPUT_MODE == INPUT_MODE_PCF
  // Piny PCF8574 (P0..P7), tlačítka spínají proti GND
  #define PCF_BIT_LIGHT  0
  #define PCF_BIT_LEFT   1
  #define PCF_BIT_RIGHT  2
  #define PCF_BIT_MODE   3
  // P4..P7 volné
#else
  // Přímé GPIO – tlačítko → GND, interní pull-up
  #define PIN_BTN_LIGHT   4
  #define PIN_BTN_LEFT   18
  #define PIN_BTN_RIGHT  19
  #define PIN_BTN_MODE   14
#endif

// Sériová konzole na UART0 (USB-UART převodník na desce) – v obou variantách
#define GOGEN_CONSOLE  1

// Brzdový spínač KT „Low-Level“: sepnuto = GND
#define BRAKE_ACTIVE_LOW true

// ---------------------------------------------------------------------
//  Měření baterie
// ---------------------------------------------------------------------
#define BAT_R1_OHM     120000.0f
#define BAT_R2_OHM       7500.0f   // 42 V → 2,47 V (lineární oblast ADC)
#define BAT_CELLS      10
#define BAT_SAMPLES    32

// ---------------------------------------------------------------------
//  Dobíjení sekundární baterie z druhé baterie (DC-DC / nabíječka)
//  ESP32 pouze spíná MOSFET/relé (PIN_CHARGE). Proud a napětí ořezává HARDWARE
//  měniče – musí být nastaven na max. 42,0 V (10S) a sekundární pack má mít BMS.
//  GPIO5 je strapping pin: při resetu krátce plave/je HIGH → na gate patří 10k pulldown.
// ---------------------------------------------------------------------
#define PIN_CHARGE          5
#define CHARGE_ACTIVE_HIGH  true
#define CHARGE_MAX_CELL_V   4.15f    // tvrdý strop na článek (~95 %), bez ohledu na cíl
#define CHARGE_MAX_MIN      240      // maximální doba jednoho nabíjení
#define CHARGE_AUTO_START_PCT 20     // auto režim: start pod touto hodnotou
#define CHARGE_BLE_LOSS_S   60       // ruční režim: ztráta BLE déle než X s → pozastavit
// Volitelné měření nabíjecího proudu (ACS712 / zesílený bočník) na GPIO35 (ADC1, jen vstup)
#define CHARGE_HAS_SENSE    0
#define PIN_CHARGE_SENSE    35
#define CHARGE_SENSE_ZERO_MV   1650.0f
#define CHARGE_SENSE_MV_PER_A  100.0f

// ---------------------------------------------------------------------
//  Řadič KT-LCD3 – UART2, pouze POSLOUCHÁNÍ (TX se nepoužívá, ESP nic nevysílá)
//  GPIO16 (RX2) → vodič TX řadiče (paralelně s displejem). 1200 baud 8N1.
// ---------------------------------------------------------------------
#define PIN_KT_RX           16
#define KT_BAUD             1200
#define KT_FRAME_LEN        12
#define KT_FRAME_START      0x41
#define KT_STALE_MS         1500     // bez rámce déle → telemetrie "není k dispozici"
#define KT_MAX_PERIOD_MS    5000     // delší perioda otáčky = stojí

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
