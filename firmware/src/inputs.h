#pragma once
#include <Arduino.h>

enum Button : uint8_t { BTN_LIGHT = 0, BTN_LEFT, BTN_RIGHT, BTN_MODE, BTN_BRAKE, BTN_COUNT };

// Combo = Levý + Pravý drženy současně COMBO_OFF_MS (hlásí se na BTN_LEFT)
enum class BtnEvent : uint8_t { None, Press, Release, Click, LongPress, Combo };

typedef void (*ButtonHandler)(Button b, BtnEvent e);

namespace inputs {
  void begin(ButtonHandler handler);
  void poll(uint32_t now);          // volat každých INPUT_POLL_MS
  bool isDown(Button b);
  uint8_t rawMask();                // bit = stisknuto (po debounce)
  bool expanderOk();                // PCF8574 odpovídá (v GPIO režimu vždy true)
}
