// GoGEN BLE protokol v2 (bez klaksonu) – zrcadlo firmware/include/protocol.h
export const SVC_UUID = 'a8f30001-4c1e-4b8e-9a52-6f6f67656e32';
export const STATE_UUID = 'a8f30002-4c1e-4b8e-9a52-6f6f67656e32';
export const CMD_UUID = 'a8f30003-4c1e-4b8e-9a52-6f6f67656e32';
export const CFG_UUID = 'a8f30004-4c1e-4b8e-9a52-6f6f67656e32';

export const OUT = { FRONT: 1, TAIL: 2, BRAKE: 4, LEFT: 8, RIGHT: 16, LED: 32 } as const;
export const ST = {
  BONDED: 1, BAT_LOW: 2, BAT_CRIT: 4, INPUT_PCF: 8, INPUT_OK: 16, DISPLAY_OK: 32, BRAKE_IN: 64,
} as const;

export enum Blink { Off = 0, Left = 1, Right = 2, Hazard = 3 }
export enum LedMode { Static = 0, Breathe = 1, Strobe = 2, Brake = 3 }
export const LED_MODE_NAMES = ['Stálé', 'Dýchání', 'Stroboskop', 'Brzda+'];

export enum Cmd {
  Front = 0x01, Tail = 0x02, Blink = 0x03, Led = 0x05, LedBright = 0x06,
  LedMode = 0x07, Lights = 0x08, AllOff = 0x09, SetCfg = 0x10, SaveCfg = 0x11,
  Calibrate = 0x12, SetPasskey = 0x13, Factory = 0x14, Reboot = 0x20,
}
export enum Arg { Off = 0, On = 1, Toggle = 2 }

export enum CfgKey {
  BlinkPeriod = 1, BlinkAutoOff, BattCal, BattLow, BattCrit, BrakeFlash, TailWithFront,
  LedBright, LedMode, LightsOnBoot,
}

export interface GogenState {
  outputs: number;
  ledBrightness: number;
  ledMode: LedMode;
  voltage: number;   // V
  percent: number;
  status: number;
  uptime: number;    // s
  blink: Blink;
  buttons: number;
}

export interface GogenConfig {
  fw: string;
  inputMode: 'PCF8574' | 'GPIO';
  blinkPeriodMs: number;
  blinkAutoOffS: number;
  battCalPermille: number;
  battLow: number;   // V
  battCrit: number;  // V
  brakeFlash: boolean;
  tailWithFront: boolean;
  ledBrightness: number;
  ledMode: LedMode;
  lightsOnBoot: boolean;
}

export function parseState(v: DataView): GogenState {
  return {
    outputs: v.getUint8(1),
    ledBrightness: v.getUint8(2),
    ledMode: v.getUint8(3),
    voltage: v.getUint16(4, true) / 100,
    percent: v.getUint8(6),
    status: v.getUint8(7),
    uptime: v.getUint32(8, true),
    blink: v.getUint8(12),
    buttons: v.getUint8(13),
  };
}

export function parseConfig(v: DataView): GogenConfig {
  return {
    fw: `${v.getUint8(1)}.${v.getUint8(2)}`,
    inputMode: v.getUint8(3) === 1 ? 'PCF8574' : 'GPIO',
    blinkPeriodMs: v.getUint16(4, true),
    blinkAutoOffS: v.getUint16(6, true),
    battCalPermille: v.getUint16(8, true),
    battLow: v.getUint16(10, true) / 100,
    battCrit: v.getUint16(12, true) / 100,
    brakeFlash: v.getUint8(14) === 1,
    tailWithFront: v.getUint8(15) === 1,
    ledBrightness: v.getUint8(16),
    ledMode: v.getUint8(17),
    lightsOnBoot: v.getUint8(18) === 1,
  };
}

// --- kódování příkazů ---
export const u16 = (n: number) => [n & 0xff, (n >> 8) & 0xff];
export const u32 = (n: number) => [n & 0xff, (n >> 8) & 0xff, (n >> 16) & 0xff, (n >>> 24) & 0xff];

export const encode = {
  simple: (c: Cmd, arg = 0) => [c, arg],
  setCfg: (k: CfgKey, value: number) => [Cmd.SetCfg, k, ...u16(Math.round(value))],
  calibrate: (volts: number) => [Cmd.Calibrate, ...u16(Math.round(volts * 100))],
  passkey: (pin: number) => [Cmd.SetPasskey, ...u32(pin)],
};
