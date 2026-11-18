#include "sensors.h"
#include <Wire.h>
#include "config.h"
#include "protocol.h"
#include "settings.h"
#include "autologic.h"

#define DAC_VDD 5.0f   // MCP4725 napájený z 5 V (výstup až 4,2 V pro kontrolér)

namespace sensors {

static autologic::AutoLight autoLight;
static autologic::AutoBrake autoBrake;

static bool luxPresent = false, mpuPresent = false, dacPresent = false;
static float luxVal = -1;
static uint32_t tLux = 0, tRetry = 0;
static uint8_t mpuErr = 0, luxErr = 0;
static bool lit = true;
static uint16_t lastDacCode = 0xFFFF;
static uint32_t lastDacWrite = 0;

static bool probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static bool writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

// ---------------- BH1750 ----------------
static bool luxInit() {
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(0x01);                       // power on
  if (Wire.endTransmission() != 0) return false;
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(0x10);                       // continuous H-res, 1 lx, 120 ms
  return Wire.endTransmission() == 0;
}

static bool luxRead(float &out) {
  if (Wire.requestFrom((uint8_t)BH1750_ADDR, (uint8_t)2) != 2) return false;
  uint16_t raw = (Wire.read() << 8) | Wire.read();
  out = raw / 1.2f;
  return true;
}

// ---------------- MPU6050 ----------------
static bool mpuInit() {
  if (!writeReg(MPU6050_ADDR, 0x6B, 0x01)) return false;   // probudit, PLL z gyra X
  writeReg(MPU6050_ADDR, 0x1A, 0x03);                      // DLPF 44 Hz – tlumí vibrace
  writeReg(MPU6050_ADDR, 0x1C, 0x00);                      // ±2 g
  return true;
}

static bool mpuRead(float &x, float &y, float &z) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)6) != 6) return false;
  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();
  x = ax / 16384.0f; y = ay / 16384.0f; z = az / 16384.0f;
  return true;
}

// ---------------- MCP4725 ----------------
static bool dacWriteCode(uint16_t code) {
  Wire.beginTransmission(MCP4725_ADDR);
  Wire.write((code >> 8) & 0x0F);         // fast write, normální režim
  Wire.write(code & 0xFF);
  return Wire.endTransmission() == 0;
}

bool dacWrite(float volts) {
  if (!dacPresent) return false;
  if (volts < 0) volts = 0;
  if (volts > DAC_VDD) volts = DAC_VDD;
  uint16_t code = (uint16_t)(volts / DAC_VDD * 4095.0f + 0.5f);
  // stejnou hodnotu stačí obnovit 2× za s (kdyby se DAC resetoval na hodnotu z EEPROM)
  if (code == lastDacCode && millis() - lastDacWrite < 500) return true;
  if (!dacWriteCode(code)) { dacPresent = false; lastDacCode = 0xFFFF; return false; }
  lastDacCode = code;
  lastDacWrite = millis();
  return true;
}

bool dacSetPowerOnZero() {
  if (!dacPresent) return false;
  Wire.beginTransmission(MCP4725_ADDR);
  Wire.write(0x60);                       // zápis DAC + EEPROM
  Wire.write(0x00);
  Wire.write(0x00);
  bool ok = Wire.endTransmission() == 0;
  delay(60);                              // EEPROM zápis trvá až 50 ms
  lastDacCode = 0;
  return ok;
}

// ---------------------------------------------------------------------
void begin() {
  // DAC jako první: po resetu ESP drží MCP4725 poslední hodnotu → hned klidový plyn
  dacPresent = probe(MCP4725_ADDR);
  if (dacPresent) dacWriteCode(0);
  luxPresent = luxInit();
  mpuPresent = mpuInit();
}

void poll(uint32_t now) {
  // znovu hledat chybějící senzory (zmizelý DAC už předtím shodil pohon do poruchy)
  if (now - tRetry > 3000) {
    tRetry = now;
    if (!luxPresent) luxPresent = luxInit();
    if (!mpuPresent) mpuPresent = mpuInit();
    if (!dacPresent && probe(MCP4725_ADDR)) { dacPresent = true; dacWriteCode(0); lastDacCode = 0; }
  }

  // MPU6050 každých SENSOR_POLL_MS
  float a = 0;
  bool aOk = false;
  if (mpuPresent) {
    float x, y, z;
    if (mpuRead(x, y, z)) {
      mpuErr = 0; aOk = true;
      a = autologic::axisValue(settings::get(CFG_MPU_AXIS), x, y, z);
    } else if (++mpuErr > 5) mpuPresent = false;
  }
  autoBrake.onG = settings::get(CFG_BRAKE_DECEL) / 1000.0f;
  autoBrake.offG = autoBrake.onG * 0.55f;
  bool brakeEnabled = settings::get(CFG_AUTO_BRAKE);
  autoBrake.update(a, aOk && brakeEnabled, now);

  // BH1750 každých LUX_POLL_MS
  if (now - tLux >= LUX_POLL_MS) {
    tLux = now;
    if (luxPresent) {
      float l;
      if (luxRead(l)) { luxVal = l; luxErr = 0; }
      else if (++luxErr > 3) { luxPresent = false; luxVal = -1; }
    } else luxVal = -1;
    autoLight.luxOn = settings::get(CFG_LUX_ON);
    autoLight.luxOff = settings::get(CFG_LUX_OFF);
    lit = autoLight.update(luxVal, now);
  }
}

bool luxOk() { return luxPresent; }
float lux() { return luxVal; }
bool autoLightLit() { return lit; }
bool mpuOk() { return mpuPresent; }
float decelG() { return autoBrake.primed ? autoBrake.decel() : 0; }
bool autoBrakeActive() { return autoBrake.active; }
bool emergency() { return autoBrake.emergency; }
bool dacOk() { return dacPresent; }

}  // namespace sensors
