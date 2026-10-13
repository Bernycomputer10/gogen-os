#include "ktlcd.h"
#include <Preferences.h>
#include "config.h"

// Rámec řadič → displej (12 B): [0]=0x41, [2..3]=perioda otáčky kola v ms (big-endian).
// Ostatní bajty se zatím nedekódují – formát podle dokumentace KT-LCD3 nebyl ověřen proti konkrétnímu
// řadiči; `kt` v konzoli vypíše surový rámec pro porovnání.
namespace ktlcd {

static HardwareSerial &uart = Serial2;
static Preferences prefs;
static uint8_t buf[KT_FRAME_LEN], last[KT_FRAME_LEN];
static uint8_t idx = 0;
static uint32_t lastByteMs = 0, lastFrameMs = 0, frames = 0, resyncs = 0;
static uint16_t periodMs = 0, wheel = 0;
static float speed = 0;
static uint32_t odoSaved = 0, odoStart = 0;
static double odoM_ = 0, tripM_ = 0, sinceSave = 0;
static uint32_t lastIntegrate = 0;
static bool movedSinceSave = false;

void begin() {
  prefs.begin("gogen-kt", true);
  wheel = prefs.getUShort("wheel", 0);
  odoStart = prefs.getUInt("odo", 0);
  odoSaved = odoStart;
  tripM_ = prefs.getUInt("trip", 0);
  prefs.end();
  odoM_ = odoStart;
  uart.begin(KT_BAUD, SERIAL_8N1, PIN_KT_RX, -1);   // TX = -1: ESP linku nikdy nebudí
}

static void save() {
  prefs.begin("gogen-kt", false);
  prefs.putUInt("odo", (uint32_t)odoM_);
  prefs.putUInt("trip", (uint32_t)tripM_);
  prefs.end();
  odoSaved = (uint32_t)odoM_;
  sinceSave = 0;
  movedSinceSave = false;
}

static void onFrame(uint32_t now) {
  memcpy(last, buf, KT_FRAME_LEN);
  periodMs = ((uint16_t)buf[2] << 8) | buf[3];
  lastFrameMs = now;
  frames++;
  if (periodMs == 0 || periodMs > KT_MAX_PERIOD_MS || wheel == 0) speed = 0;
  else speed = wheel * 3.6f / periodMs;              // mm/ms = m/s
}

void poll(uint32_t now) {
  while (uart.available()) {
    uint8_t c = uart.read();
    if (now - lastByteMs > 25) { if (idx) resyncs++; idx = 0; }   // mezera mezi rámci
    lastByteMs = now;
    if (idx == 0 && c != KT_FRAME_START) continue;
    buf[idx++] = c;
    if (idx == KT_FRAME_LEN) { onFrame(now); idx = 0; }
  }

  // integrace ujeté vzdálenosti
  if (lastIntegrate) {
    float dt = (now - lastIntegrate) / 1000.0f;
    if (speedValid(now) && speed > 0.5f) {
      double d = speed / 3.6 * dt;
      odoM_ += d; tripM_ += d; sinceSave += d;
      movedSinceSave = true;
    }
  }
  lastIntegrate = now;
  // zápis do flash: po zastavení (po ≥200 m) nebo každé 2 km – šetří cykly
  if ((movedSinceSave && speed < 0.5f && sinceSave >= 200) || sinceSave >= 2000) save();
}

bool linkOk(uint32_t now) { return frames > 0 && now - lastFrameMs < KT_STALE_MS; }
bool speedValid(uint32_t now) { return linkOk(now) && wheel > 0; }
float speedKmh() { return speed; }
uint32_t odoM() { return (uint32_t)odoM_; }
uint32_t tripM() { return (uint32_t)tripM_; }
void resetTrip() { tripM_ = 0; save(); }
uint16_t wheelMm() { return wheel; }

void setWheelMm(uint16_t mm) {
  wheel = mm;
  prefs.begin("gogen-kt", false);
  prefs.putUShort("wheel", mm);
  prefs.end();
}

void printDebug(Print &out, uint32_t now) {
  out.printf("KT: rámců %lu, resync %lu, link %s, perioda %u ms, kolo %u mm, rychlost %.1f km/h\n",
             (unsigned long)frames, (unsigned long)resyncs, linkOk(now) ? "OK" : "ticho", periodMs, wheel, speed);
  out.print("KT: poslední rámec:");
  for (int i = 0; i < KT_FRAME_LEN; i++) out.printf(" %02X", last[i]);
  out.println();
}

}  // namespace ktlcd
