#pragma once
#include <Arduino.h>

namespace display {
  bool begin();
  bool ok();
  void render(uint32_t now, bool bleConnected);
  void message(const char *line1, const char *line2);   // dočasná hláška (4 s)
  void sleep();                                         // vypnout OLED
}
