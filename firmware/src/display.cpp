#include "display.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "protocol.h"
#include "vehicle.h"
#include "battery.h"
#include "inputs.h"

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
  oled.setCursor(40, 44);
  oled.printf("v%d.%d", GOGEN_FW_MAJOR, GOGEN_FW_MINOR);
  oled.display();
  return true;
}

bool ok() { return present; }

void message(const char *l1, const char *l2) {
  strlcpy(msg1, l1 ? l1 : "", sizeof(msg1));
  strlcpy(msg2, l2 ? l2 : "", sizeof(msg2));
  msgUntil = millis() + 4000;
}

static void arrow(int x, int y, bool left, bool filled) {
  // trojúhelník 14×14
  int x0 = left ? x + 13 : x, x1 = left ? x : x + 13;
  if (filled) oled.fillTriangle(x0, y, x0, y + 13, x1, y + 6, SSD1306_WHITE);
  else oled.drawTriangle(x0, y, x0, y + 13, x1, y + 6, SSD1306_WHITE);
}

static void tag(int x, int y, const char *txt, bool on) {
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
}

void render(uint32_t now, bool bleConnected) {
  if (!present) return;
  oled.clearDisplay();
  oled.setTextSize(1);

  if ((int32_t)(msgUntil - now) > 0) {
    oled.setTextSize(1);
    oled.setCursor(0, 8);
    oled.print(msg1);
    oled.setTextSize(2);
    oled.setCursor(0, 28);
    oled.print(msg2);
    oled.display();
    return;
  }

  uint8_t out = vehicle::outputsMask();

  // horní řádek
  oled.setCursor(0, 0);
  oled.print("GoGEN");
  if (!inputs::expanderOk()) { oled.setCursor(40, 0); oled.print("!BTN"); }
  if (bleConnected) { oled.setCursor(110, 0); oled.print("BT"); }

  // napětí + procenta
  float v = battery::volts();
  uint8_t pct = battery::percent();
  bool warn = vehicle::battLow() && (now / 500) % 2;
  oled.setTextSize(2);
  oled.setCursor(0, 12);
  if (warn) oled.print("LOW BAT");
  else oled.printf("%4.1fV", v);
  oled.setCursor(92, 12);
  oled.printf("%3u", pct);
  oled.setTextSize(1);
  oled.setCursor(122, 19);
  oled.print("%");

  // lišta baterie
  oled.drawRect(0, 31, 128, 7, SSD1306_WHITE);
  oled.fillRect(2, 33, (124 * pct) / 100, 3, SSD1306_WHITE);

  // blinkry
  uint8_t bm = vehicle::blinkMode();
  arrow(0, 44, true, out & OUT_LEFT);
  if (bm == BLINK_LEFT || bm == BLINK_HAZARD) oled.drawFastHLine(0, 60, 14, SSD1306_WHITE);
  arrow(114, 44, false, out & OUT_RIGHT);
  if (bm == BLINK_RIGHT || bm == BLINK_HAZARD) oled.drawFastHLine(114, 60, 14, SSD1306_WHITE);

  // stavové štítky
  tag(22, 46, "SV", out & OUT_FRONT);
  tag(46, 46, "LED", out & OUT_LED);
  tag(76, 46, "BRZ", vehicle::brakeInput());

  oled.display();
}

}  // namespace display
