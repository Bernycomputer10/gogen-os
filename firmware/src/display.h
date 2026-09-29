#pragma once
#include <Arduino.h>

namespace display {
  bool begin();            // true = OLED nalezen
  bool ok();
  void render(uint32_t now, bool bleConnected);
  void message(const char *line1, const char *line2 = nullptr);  // krátké hlášení (např. passkey)
}
