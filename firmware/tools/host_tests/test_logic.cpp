// Testy logiky GoGEN OS na PC (bez Arduina):
//   g++ -std=c++17 -Wall -Wextra -I../../include test_logic.cpp -o t && ./t
#include <cstdio>
#include <cmath>
#include "autologic.h"
#include "charge_logic.h"
#include "drive_logic.h"
#include "power_logic.h"

static int fails = 0, passes = 0;
#define CHECK(c) do { if (c) passes++; else { fails++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
#define TEST(n) static void n()
#define RUN(n) do { int f0 = fails; n(); printf("%-34s %s\n", #n, fails == f0 ? "ok" : "CHYBA"); } while (0)

using namespace autologic;

// ------------------------------------------------------------ AutoLight
TEST(light_dark_at_start) { AutoLight l; CHECK(l.update(10, 0)); }
TEST(light_bright_at_start) { AutoLight l; CHECK(!l.update(500, 0)); }
TEST(light_needs_delay_to_turn_on) {
  AutoLight l; l.update(500, 0);
  CHECK(!l.update(10, 1000));
  CHECK(!l.update(10, 2500));
  CHECK(l.update(10, 3100));
}
TEST(light_hysteresis_and_shadow) {
  AutoLight l; l.update(10, 0);
  CHECK(l.update(80, 1000));          // mezi prahy → beze změny
  CHECK(l.update(500, 2000));         // stín/světlo krátce
  CHECK(l.update(20, 5000));          // znovu tma → čítač se zruší
  CHECK(l.update(500, 6000));
  CHECK(!l.update(500, 16500));       // 10 s světla → zhasne
}
TEST(light_sensor_fail_is_on) { AutoLight l; l.update(500, 0); CHECK(l.update(-1, 10)); }

// ------------------------------------------------------------ AutoBrake
static bool runBrake(AutoBrake &b, float a, uint32_t &t, uint32_t ms) {
  bool r = false;
  for (uint32_t i = 0; i < ms; i += 10) { t += 10; r = b.update(a, true, t); }
  return r;
}
TEST(brake_detects_deceleration) {
  AutoBrake b; uint32_t t = 0;
  CHECK(!runBrake(b, 0.0f, t, 2000));
  CHECK(runBrake(b, -0.25f, t, 400));
  CHECK(!b.emergency);
  CHECK(!runBrake(b, 0.0f, t, 600));
}
TEST(brake_ignores_pothole_spike) {
  AutoBrake b; uint32_t t = 0;
  runBrake(b, 0, t, 2000);
  CHECK(!runBrake(b, -0.6f, t, 40));
  CHECK(!runBrake(b, 0, t, 300));
}
TEST(brake_emergency) {
  AutoBrake b; uint32_t t = 0;
  runBrake(b, 0, t, 2000);
  runBrake(b, -0.6f, t, 400);
  CHECK(b.active && b.emergency);
}
TEST(brake_slope_compensated) {
  AutoBrake b; uint32_t t = 0;
  runBrake(b, 0, t, 2000);
  // pomalý přechod do kopce (−0,1 g gravitační složka) během 20 s
  bool any = false;
  for (int i = 0; i < 2000; i++) { t += 10; any |= b.update(-0.1f * i / 2000.0f, true, t); }
  CHECK(!any);
  CHECK(!runBrake(b, -0.1f, t, 5000));
}
TEST(brake_stuck_recovers) {
  AutoBrake b; uint32_t t = 0;
  runBrake(b, 0, t, 2000);
  CHECK(runBrake(b, -0.3f, t, 1000));
  runBrake(b, -0.3f, t, 10000);
  CHECK(!runBrake(b, -0.3f, t, 500));
}
TEST(brake_sensor_lost) {
  AutoBrake b; uint32_t t = 0;
  runBrake(b, 0, t, 2000); runBrake(b, -0.3f, t, 400);
  CHECK(!b.update(0, false, t + 10));
}
TEST(axis_select) {
  CHECK(axisValue(0, 1, 2, 3) == 1); CHECK(axisValue(1, 1, 2, 3) == -1);
  CHECK(axisValue(3, 1, 2, 3) == -2); CHECK(axisValue(4, 1, 2, 3) == 3);
}

// ------------------------------------------------------------ Charger
static chg::Inputs ci(float v, uint8_t pct, float gen = 40, bool fd = false) {
  return chg::Inputs{v, pct, true, gen, fd};
}
static void step(chg::Charger &c, chg::Inputs in, uint32_t &t, uint32_t ms) {
  for (uint32_t i = 0; i < ms; i += 100) { t += 100; c.update(in, t); }
}
TEST(chg_starts_at_25_stops_at_95) {
  chg::Charger c; uint32_t t = 0;
  step(c, ci(37, 50), t, 5000); CHECK(!c.output()); CHECK(c.state() == chg::State::IDLE);
  step(c, ci(35, 25), t, 3000); CHECK(c.output()); CHECK(c.state() == chg::State::CHARGING);
  step(c, ci(38, 60), t, 20000); CHECK(c.output());
  step(c, ci(41, 95), t, 1000); CHECK(!c.output()); CHECK(c.state() == chg::State::FULL);
  step(c, ci(39, 70), t, 30000); CHECK(!c.output());
  step(c, ci(35, 24), t, 3000); CHECK(c.output());
}
TEST(chg_needs_generator) {
  chg::Charger c; uint32_t t = 0;
  step(c, ci(35, 20, 5), t, 5000); CHECK(!c.output());
  CHECK(c.reasons() & chg::R_NO_GENERATOR); CHECK(c.state() == chg::State::WAITING);
  step(c, ci(35, 20, 40), t, 1000); CHECK(!c.output());   // debounce 2 s
  step(c, ci(35, 20, 40), t, 1500); CHECK(c.output());
}
TEST(chg_generator_overvolt_cuts_now) {
  chg::Charger c; uint32_t t = 0;
  step(c, ci(35, 20), t, 3000); CHECK(c.output());
  t += 100; c.update(ci(35, 20, 60), t); CHECK(!c.output());
}
TEST(chg_front_drive_blocks) {
  chg::Charger c; uint32_t t = 0;
  step(c, ci(35, 20), t, 3000); CHECK(c.output());
  t += 100; c.update(ci(35, 20, 40, true), t); CHECK(!c.output());
  CHECK(c.reasons() & chg::R_FRONT_DRIVE);
}
TEST(chg_hard_voltage_stop) {
  chg::Charger c; uint32_t t = 0;
  step(c, ci(35, 20), t, 3000);
  t += 100; c.update(ci(41.6f, 90), t); CHECK(!c.output());
}
TEST(chg_overvoltage_latch) {
  chg::Charger c; uint32_t t = 0;
  step(c, ci(35, 20), t, 3000);
  t += 100; c.update(ci(42.3f, 99), t);
  CHECK(c.state() == chg::State::FAULT); CHECK(c.fault() == chg::FT_OVERVOLTAGE);
  step(c, ci(35, 20), t, 30000); CHECK(!c.output());
  c.resetFault(); step(c, ci(35, 20), t, 15000); CHECK(c.output());
}
TEST(chg_no_rise_fault) {
  chg::Charger c; uint32_t t = 0;
  step(c, ci(35, 20), t, 3000); CHECK(c.output());
  step(c, ci(35, 20), t, 610000);
  CHECK(c.state() == chg::State::FAULT); CHECK(c.fault() == chg::FT_NO_RISE);
}
TEST(chg_rising_no_fault) {
  chg::Charger c; uint32_t t = 0; float v = 35;
  step(c, ci(v, 20), t, 3000);
  for (int i = 0; i < 40; i++) { v += 0.05f; step(c, ci(v, 22), t, 60000); }
  CHECK(c.state() == chg::State::CHARGING);
}
TEST(chg_relay_min_off_time) {
  chg::Charger c; uint32_t t = 0;
  step(c, ci(35, 20), t, 3000); CHECK(c.output());
  step(c, ci(35, 20, 5), t, 500); CHECK(c.output());      // kolo zastavilo: relé drží min. 10 s
  step(c, ci(35, 20, 5), t, 10000); CHECK(!c.output());
  step(c, ci(35, 20, 40), t, 3000); CHECK(!c.output());   // ještě běží min. pauza
  step(c, ci(35, 20, 40), t, 8000); CHECK(c.output());
}
TEST(chg_mode_off_and_force) {
  chg::Charger c; uint32_t t = 0;
  c.setMode(chg::Mode::OFF); step(c, ci(35, 20), t, 5000); CHECK(!c.output());
  c.setMode(chg::Mode::FORCE); step(c, ci(38, 60), t, 15000); CHECK(c.output());
  step(c, ci(41, 95), t, 15000); CHECK(!c.output()); CHECK(c.mode() == chg::Mode::AUTO);
}
TEST(chg_sensor_fault_blocks) {
  chg::Charger c; uint32_t t = 0;
  chg::Inputs in = ci(35, 20); in.battValid = false;
  step(c, in, t, 5000); CHECK(!c.output()); CHECK(c.reasons() & chg::R_SENSOR_FAULT);
}

// ------------------------------------------------------------ Drive
static drv::Inputs di(float thr, float rear, float front, bool brake = false) {
  drv::Inputs in{};
  in.enabled = true; in.dacOk = true; in.throttle = thr; in.brake = brake;
  in.rearKmh = rear; in.rearValid = true; in.frontKmh = front; in.frontValid = true;
  in.battPct = 80; in.battValid = true;
  return in;
}
static void dstep(drv::Drive &d, drv::Inputs in, uint32_t &t, uint32_t ms) {
  for (uint32_t i = 0; i < ms; i += 10) { t += 10; d.update(in, t); }
}
TEST(drv_arms_and_follows_throttle) {
  drv::Drive d; uint32_t t = 0;
  dstep(d, di(0, 0, 0), t, 400);
  CHECK(d.state() == drv::State::READY); CHECK(d.phaseClosed()); CHECK(d.out().dac == 0);
  dstep(d, di(1, 2, 2), t, 2000);
  CHECK(d.state() == drv::State::ACTIVE);
  CHECK(fabsf(d.out().dac - 0.6f) < 0.02f);     // assist 60 %, bez regulace pod 5 km/h
  CHECK(!d.out().frontBrake);
}
TEST(drv_never_exceeds_rider) {
  drv::Drive d; uint32_t t = 0;
  dstep(d, di(0, 0, 0), t, 400);
  dstep(d, di(0.3f, 20, 10), t, 5000);          // přední zaostává → PI tlačí nahoru
  float thr = (0.3f - 0.05f) / 0.95f;
  CHECK(d.out().dac <= thr + 1e-4f);
  CHECK(d.out().dac > thr * 0.6f);
}
TEST(drv_sync_reduces_when_front_faster) {
  drv::Drive d; uint32_t t = 0;
  dstep(d, di(0, 0, 0), t, 400);
  dstep(d, di(0.8f, 20, 20), t, 3000); float even = d.out().dac;
  dstep(d, di(0.8f, 20, 22), t, 3000);
  CHECK(d.out().dac < even);
}
TEST(drv_brake_cuts_immediately) {
  drv::Drive d; uint32_t t = 0;
  dstep(d, di(0, 0, 0), t, 400); dstep(d, di(1, 10, 10), t, 2000);
  t += 10; d.update(di(1, 10, 10, true), t);
  CHECK(d.out().dac == 0); CHECK(d.out().frontBrake); CHECK(d.phaseClosed());
}
TEST(drv_throttle_held_at_enable) {
  drv::Drive d; uint32_t t = 0;
  dstep(d, di(0.5f, 0, 0), t, 1000);
  CHECK(d.state() == drv::State::OFF); CHECK(d.reasons() & drv::R_THROTTLE);
  dstep(d, di(0, 0, 0), t, 400); CHECK(d.state() == drv::State::READY);
}
TEST(drv_overspeed_opens_phases) {
  drv::Drive d; uint32_t t = 0;
  dstep(d, di(0, 0, 0), t, 400); dstep(d, di(1, 20, 20), t, 1000);
  dstep(d, di(1, 26, 26), t, 50);
  CHECK(d.out().dac == 0);
  dstep(d, di(1, 26, 26), t, 300);
  CHECK(!d.phaseClosed()); CHECK(d.state() == drv::State::OFF);
  dstep(d, di(0, 22, 22), t, 500); CHECK(!d.phaseClosed());   // hystereze 5 km/h
  dstep(d, di(0, 19, 19), t, 500); CHECK(d.phaseClosed());
}
TEST(drv_low_battery) {
  drv::Drive d; uint32_t t = 0;
  drv::Inputs in = di(0, 0, 0); in.battPct = 15;
  dstep(d, in, t, 500); CHECK(!d.phaseClosed()); CHECK(d.reasons() & drv::R_BATT_LOW);
  in.battPct = 22; dstep(d, in, t, 500); CHECK(!d.phaseClosed());   // hystereze
  in.battPct = 26; dstep(d, in, t, 500); CHECK(d.phaseClosed());
}
TEST(drv_dac_loss_is_fault) {
  drv::Drive d; uint32_t t = 0;
  dstep(d, di(0, 0, 0), t, 400); dstep(d, di(1, 10, 10), t, 500);
  drv::Inputs in = di(1, 10, 10); in.dacOk = false;
  dstep(d, in, t, 300);
  CHECK(d.state() == drv::State::FAULT); CHECK(d.fault() == drv::FT_DAC); CHECK(!d.phaseClosed());
  dstep(d, di(0, 0, 0), t, 1000); CHECK(d.state() == drv::State::FAULT);
  d.resetFault(); dstep(d, di(0, 0, 0), t, 400); CHECK(d.state() == drv::State::READY);
}
TEST(drv_throttle_signal_lost_while_driving) {
  drv::Drive d; uint32_t t = 0;
  dstep(d, di(0, 0, 0), t, 400); dstep(d, di(1, 10, 10), t, 500);
  dstep(d, di(-1, 10, 10), t, 300);
  CHECK(d.state() == drv::State::FAULT); CHECK(d.fault() == drv::FT_THROTTLE);
}
TEST(drv_throttle_missing_when_idle_no_fault) {
  drv::Drive d; uint32_t t = 0;
  dstep(d, di(0, 0, 0), t, 400);
  dstep(d, di(-1, 0, 0), t, 500);                // primár vypnut
  CHECK(d.state() == drv::State::OFF); CHECK(d.fault() == drv::FT_NONE);
}
TEST(drv_runaway_fault) {
  drv::Drive d; uint32_t t = 0;
  dstep(d, di(0, 0, 0), t, 400);
  dstep(d, di(0, 5, 20), t, 2000);               // přední jede bez plynu
  CHECK(d.fault() == drv::FT_RUNAWAY); CHECK(!d.phaseClosed());
}
TEST(drv_slip_control) {
  drv::Drive d; uint32_t t = 0;
  dstep(d, di(0, 0, 0), t, 400); dstep(d, di(1, 10, 10), t, 1500);
  dstep(d, di(1, 10, 18), t, 400);
  CHECK(d.reasons() & drv::R_SLIP); CHECK(d.out().dac < 0.05f);
  dstep(d, di(1, 10, 10.5f), t, 50); CHECK(!(d.reasons() & drv::R_SLIP));
}
TEST(drv_disable_releases_safely) {
  drv::Drive d; uint32_t t = 0;
  dstep(d, di(0, 0, 0), t, 400); dstep(d, di(1, 10, 10), t, 1000);
  drv::Inputs in = di(1, 10, 10); in.enabled = false;
  t += 10; d.update(in, t);
  CHECK(d.out().dac == 0); CHECK(d.phaseClosed());          // nejdřív klidový plyn
  dstep(d, in, t, 200); CHECK(!d.phaseClosed());
}
TEST(drv_ramp_limits) {
  drv::Drive d; uint32_t t = 0;
  dstep(d, di(0, 0, 0), t, 400);
  dstep(d, di(1, 2, 2), t, 100);
  CHECK(d.out().dac < 0.2f);
}
TEST(drv_dac_mapping) {
  CHECK(fabsf(drv::dacVolts(0, 0.8f, 1.1f, 4.2f) - 0.8f) < 1e-4f);
  CHECK(fabsf(drv::dacVolts(1, 0.8f, 1.1f, 4.2f) - 4.2f) < 1e-4f);
  CHECK(fabsf(drv::dacVolts(0.5f, 0.8f, 1.1f, 4.2f) - 2.65f) < 1e-4f);
}
TEST(drv_throttle_mapping) {
  CHECK(drv::throttleFromMv(300, 300, 2600) == 0);
  CHECK(drv::throttleFromMv(2600, 300, 2600) == 1);
  CHECK(fabsf(drv::throttleFromMv(1450, 300, 2600) - 0.5f) < 1e-3f);
  CHECK(drv::throttleFromMv(0, 300, 2600) < 0);     // přerušený vodič
  CHECK(drv::throttleFromMv(3300, 300, 2600) < 0);  // zkrat na 3V3
  CHECK(drv::throttleFromMv(1000, 300, 400) < 0);   // nekalibrováno
}

// ------------------------------------------------------------ Power
TEST(pwr_idle_off) {
  pwr::AutoOff a; a.idleMin = 1; a.activity(0);
  CHECK(a.update(59000, false, 38, true) == pwr::OffReason::NONE);
  CHECK(a.update(61000, false, 38, true) == pwr::OffReason::IDLE);
}
TEST(pwr_busy_holds) {
  pwr::AutoOff a; a.idleMin = 1;
  for (uint32_t t = 0; t < 300000; t += 1000) CHECK(a.update(t, true, 38, true) == pwr::OffReason::NONE);
}
TEST(pwr_undervolt) {
  pwr::AutoOff a; a.idleMin = 0;
  CHECK(a.update(0, true, 29.5f, true) == pwr::OffReason::NONE);
  CHECK(a.update(20000, true, 31, true) == pwr::OffReason::NONE);     // propad zátěží
  CHECK(a.update(21000, true, 29.5f, true) == pwr::OffReason::NONE);
  CHECK(a.update(52000, true, 29.5f, true) == pwr::OffReason::UNDERVOLT);
}
TEST(pwr_speed_calc) {
  // 15 pulzů/ot, obvod 800 mm, 25 km/h → 6,944 m/s → 8,68 ot/s → 130 Hz
  float k = pwr::kmhFromPeriod(7680, 15, 800);
  CHECK(fabsf(k - 25.0f) < 0.1f);
  CHECK(pwr::kmhFromPeriod(0, 15, 800) == 0);
}

int main() {
  RUN(light_dark_at_start); RUN(light_bright_at_start); RUN(light_needs_delay_to_turn_on);
  RUN(light_hysteresis_and_shadow); RUN(light_sensor_fail_is_on);
  RUN(brake_detects_deceleration); RUN(brake_ignores_pothole_spike); RUN(brake_emergency);
  RUN(brake_slope_compensated); RUN(brake_stuck_recovers); RUN(brake_sensor_lost); RUN(axis_select);
  RUN(chg_starts_at_25_stops_at_95); RUN(chg_needs_generator); RUN(chg_generator_overvolt_cuts_now);
  RUN(chg_front_drive_blocks); RUN(chg_hard_voltage_stop); RUN(chg_overvoltage_latch);
  RUN(chg_no_rise_fault); RUN(chg_rising_no_fault); RUN(chg_relay_min_off_time);
  RUN(chg_mode_off_and_force); RUN(chg_sensor_fault_blocks);
  RUN(drv_arms_and_follows_throttle); RUN(drv_never_exceeds_rider); RUN(drv_sync_reduces_when_front_faster);
  RUN(drv_brake_cuts_immediately); RUN(drv_throttle_held_at_enable); RUN(drv_overspeed_opens_phases);
  RUN(drv_low_battery); RUN(drv_dac_loss_is_fault); RUN(drv_throttle_signal_lost_while_driving);
  RUN(drv_throttle_missing_when_idle_no_fault); RUN(drv_runaway_fault); RUN(drv_slip_control);
  RUN(drv_disable_releases_safely); RUN(drv_ramp_limits); RUN(drv_dac_mapping); RUN(drv_throttle_mapping);
  RUN(pwr_idle_off); RUN(pwr_busy_holds); RUN(pwr_undervolt); RUN(pwr_speed_calc);
  printf("\n%d kontrol OK, %d chyb\n", passes, fails);
  return fails ? 1 : 0;
}
