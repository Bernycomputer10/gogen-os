#include "display.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "protocol.h"
#include "vehicle.h"
#include "battery.h"
#include "inputs.h"
#include "speed.h"
#include "charge.h"
#include "drive.h"
#include "ota.h"

namespace display {

static Adafruit_SSD1306 oled(128, 64, &Wire, -1);
static bool present = false;
static char msg1[22] = {0}, msg2[22] = {0};
static uint32_t msgUntil = 0;

bool begin() {
  Wire.beginTransmission(OLED_ADDR);
  if (Wire.endTransmission() != 0) return present = false;
  present = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR, false, false);
  if (!present) return false;
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(2);
  oled.setCursor(16, 16);
  oled.print("GoGEN OS");
  oled.setTextSize(1);
  oled.setCursor(46, 44);
  oled.printf("v%d.%d", GOGEN_FW_MAJOR, GOGEN_FW_MINOR);
  oled.display();
  return true;
}

bool ok() { return present; }

void sleep() {
  if (!present) return;
  oled.clearDisplay();
  oled.display();
  oled.ssd1306_command(SSD1306_DISPLAYOFF);
}

void message(const char *l1, const char *l2) {
  strlcpy(msg1, l1 ? l1 : "", sizeof(msg1));
  strlcpy(msg2, l2 ? l2 : "", sizeof(msg2));
  msgUntil = millis() + 4000;
}

static void arrow(int x, int y, bool left, bool filled) {
  int x0 = left ? x + 11 : x, x1 = left ? x : x + 11;
  if (filled) oled.fillTriangle(x0, y, x0, y + 11, x1, y + 5, SSD1306_WHITE);
  else oled.drawTriangle(x0, y, x0, y + 11, x1, y + 5, SSD1306_WHITE);
}

static int tag(int x, int y, const char *txt, bool on) {
  int w = strlen(txt) * 6 + 3;
  if (on) {
    oled.fillRoundRect(x, y, w, 11, 2, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
  } else {
    oled.drawRoundRect(x, y, w, 11, 2, SSD1306_WHITE);
    oled.setTextColor(SSD1306_WHITE);
  }
  oled.setCursor(x + 2, y + 2);
  oled.print(txt);
  oled.setTextColor(SSD1306_WHITE);
  return x + w + 3;
}

void render(uint32_t now, bool bleConnected) {
  if (!present) return;
  oled.clearDisplay();
  oled.setTextSize(1);

  if ((int32_t)(msgUntil - now) > 0) {
    oled.setCursor(0, 8);
    oled.print(msg1);
    oled.setTextSize(2);
    oled.setCursor(0, 28);
    oled.print(msg2);
    oled.display();
    return;
  }

  if (ota::active()) {
    oled.setCursor(0, 0);  oled.print("Wi-Fi aktualizace");
    oled.setCursor(0, 16); oled.print(ota::ssid());
    oled.setCursor(0, 28); oled.printf("heslo %s", ota::password());
    oled.setCursor(0, 44); oled.print("http://192.168.4.1");
    oled.display();
    return;
  }

  uint8_t out = vehicle::outputsMask();
  float v = battery::volts();
  uint8_t pct = battery::percent();

  // horní řádek: napětí, %, ikony
  bool warn = vehicle::battLow() && (now / 500) % 2;
  oled.setCursor(0, 0);
  if (warn) oled.print("SLABA BAT");
  else oled.printf("%4.1fV %3u%%", v, pct);
  int x = 128;
  if (bleConnected) { x -= 12; oled.setCursor(x, 0); oled.print("BT"); }
  if (!inputs::expanderOk()) { x -= 24; oled.setCursor(x, 0); oled.print("!BTN"); }
  if (charge::state() == chg::State::FAULT || drive::state() == drv::State::FAULT) {
    x -= 12; oled.setCursor(x, 0); oled.print("!!");
  }

  // střed: rychlost (nebo velké napětí, když rychloměr chybí)
  oled.setTextSize(3);
  if (speed::known()) {
    oled.setCursor(0, 12);
    oled.printf("%3d", (int)(speed::kmh() + 0.5f));
    oled.setTextSize(1);
    oled.setCursor(56, 26);
    oled.print("km/h");
    oled.setCursor(86, 12);
    oled.printf("%5.1f", speed::tripM() / 1000.0f);
    oled.setCursor(86, 22);
    oled.print("km");
  } else {
    oled.setCursor(0, 12);
    oled.printf("%4.1f", v);
    oled.setTextSize(1);
    oled.setCursor(76, 26);
    oled.print("V");
  }
  oled.setTextSize(1);

  // lišta baterie
  oled.drawRect(0, 37, 128, 6, SSD1306_WHITE);
  oled.fillRect(2, 39, (124 * pct) / 100, 2, SSD1306_WHITE);

  // blinkry + štítky
  uint8_t bm = vehicle::blinkMode();
  arrow(0, 48, true, out & OUT_LEFT);
  if (bm == BLINK_LEFT || bm == BLINK_HAZARD) oled.drawFastHLine(0, 62, 12, SSD1306_WHITE);
  arrow(116, 48, false, out & OUT_RIGHT);
  if (bm == BLINK_RIGHT || bm == BLINK_HAZARD) oled.drawFastHLine(116, 62, 12, SSD1306_WHITE);

  int tx = 15;
  tx = tag(tx, 49, vehicle::lightMode() == LIGHT_AUTO ? "SA" : "SV", out & OUT_FRONT);
  tx = tag(tx, 49, "LED", out & OUT_LED);
  tx = tag(tx, 49, "BR", out & OUT_BRAKE);
  chg::State cs = charge::state();
  drv::State ds = drive::state();
  if (ds == drv::State::ACTIVE || ds == drv::State::READY || ds == drv::State::ARMING)
    tag(tx, 49, "2x", ds == drv::State::ACTIVE);
  else if (cs == chg::State::CHARGING || cs == chg::State::WAITING)
    tag(tx, 49, "NB", cs == chg::State::CHARGING);

  oled.display();
}

}  // namespace display
