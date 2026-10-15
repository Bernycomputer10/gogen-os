#pragma once
#include <Arduino.h>

typedef void (*CommandHandler)(const uint8_t *data, size_t len);

namespace ble {
  void begin(CommandHandler handler);
  void process();                               // vyzvedne příkazy z fronty (hlavní smyčka)
  void publishState(const uint8_t *buf, size_t len);
  void publishTelemetry(const uint8_t *buf, size_t len);
  void publishConfig(const uint8_t *buf, size_t len);
  bool connected();
  bool secured();
  void deleteBonds();
  const char *name();
}
