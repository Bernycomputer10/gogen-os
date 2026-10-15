#pragma once
#include <Arduino.h>

// Aktualizace firmwaru přes Wi-Fi: ESP vytvoří přístupový bod „GoGEN-xxxx-OTA“,
// v telefonu/PC otevři http://192.168.4.1 a nahraj firmware.bin.
namespace ota {
  bool start(const char *bleName, uint32_t passkey);   // false = nepovoleno (jízda, pohon)
  void stop();
  void handle(uint32_t now);
  bool active();
  bool busy();              // právě se zapisuje firmware
  const char *ssid();
  const char *password();
}
