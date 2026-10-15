#include "power.h"
#include <esp_sleep.h>
#include <esp_task_wdt.h>
#include <driver/rtc_io.h>
#include "config.h"
#include "protocol.h"
#include "settings.h"
#include "battery.h"
#include "vehicle.h"
#include "charge.h"
#include "drive.h"
#include "speed.h"
#include "display.h"

namespace power {

static pwr::AutoOff ao;

void begin() {
  pinMode(PIN_PWR_HOLD, OUTPUT);
  digitalWrite(PIN_PWR_HOLD, HIGH);
  ao.activity(millis());
}

void activity() { ao.activity(millis()); }

pwr::OffReason tick(uint32_t now, bool busy) {
  ao.idleMin = settings::get(CFG_AUTO_OFF_MIN);
  ao.cutoffV = settings::get(CFG_BATT_OFF) / 100.0f;
  return ao.update(now, busy, battery::volts(), battery::valid());
}

uint16_t secondsLeft(uint32_t now) { return ao.secondsLeft(now); }

void shutdown(pwr::OffReason why) {
  esp_task_wdt_delete(NULL);            // vypínání trvá ~3 s, watchdog by ho přerušil
  drive::forceOff();
  charge::forceOff();
  vehicle::forceOff();
  speed::saveNow();
  const char *txt = why == pwr::OffReason::UNDERVOLT ? "VYBITO" :
                    why == pwr::OffReason::IDLE ? "NECINNOST" : "VYPINAM";
  display::message("GoGEN se vypina", txt);
  display::render(millis(), false);
  Serial.printf("Vypinam (%s)\n", txt);
  Serial.flush();
  delay(1200);
  display::sleep();
  digitalWrite(PIN_PWR_HOLD, LOW);       // latch odpojí napájení
  delay(1500);
  // Napájení stále drží (není latch / napájení z USB) → hluboký spánek,
  // probudí stisk brzdy (GPIO13 → GND).
  rtc_gpio_pullup_en((gpio_num_t)PIN_BRAKE_IN);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_BRAKE_IN);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BRAKE_IN, 0);
  esp_deep_sleep_start();
}

}  // namespace power
