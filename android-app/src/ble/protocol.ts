// GoGEN BLE protokol v3 – zrcadlo firmware/include/protocol.h
export const PROTO_VERSION = 3;
export const SVC_UUID = 'a8f30001-4c1e-4b8e-9a52-6f6f67656e32';
export const STATE_UUID = 'a8f30002-4c1e-4b8e-9a52-6f6f67656e32';
export const CMD_UUID = 'a8f30003-4c1e-4b8e-9a52-6f6f67656e32';
export const CFG_UUID = 'a8f30004-4c1e-4b8e-9a52-6f6f67656e32';
export const TELEM_UUID = 'a8f30005-4c1e-4b8e-9a52-6f6f67656e32';

export const OUT = { FRONT: 1, TAIL: 2, BRAKE: 4, LEFT: 8, RIGHT: 16, LED: 32, CHG: 64, PHASE: 128 } as const;
export const ST = {
  BONDED: 1, BAT_LOW: 2, BAT_CRIT: 4, INPUT_PCF: 8, INPUT_OK: 16, DISPLAY_OK: 32, BRAKE_IN: 64, AUTO_BRAKE: 128,
} as const;
export const ST2 = {
  LUX_OK: 1, MPU_OK: 2, DAC_OK: 4, SPEED_OK: 8, EMERGENCY: 16, OTA: 32, OFF_SOON: 64, DRIVE_HW: 128,
} as const;
export const HW = { SPEED: 1, DRIVE: 2, CHARGE: 4, OTA: 8 } as const;

export enum Blink { Off = 0, Left = 1, Right = 2, Hazard = 3 }
export enum LedMode { Static = 0, Breathe = 1, Strobe = 2, Brake = 3 }
export const LED_MODE_NAMES = ['Stálé', 'Dýchání', 'Stroboskop', 'Brzda+'];
export enum LightMode { Off = 0, On = 1, Auto = 2 }

export enum ChgState { Idle = 0, Waiting = 1, Charging = 2, Full = 3, Fault = 4 }
export enum ChgMode { Auto = 0, Off = 1, Force = 2 }
export const CHG_STATE_NAMES = ['Čeká na pokles', 'Chce nabíjet', 'Nabíjí', 'Nabito', 'Porucha'];
export const CHG_REASON = { NO_GEN: 1, GEN_OVERVOLT: 2, FRONT_DRIVE: 4, SENSOR: 8, MODE_OFF: 16, MIN_OFF: 32 } as const;
export const CHG_REASON_NAMES: [number, string][] = [
  [1, 'kolo stojí'], [2, 'generátor nad 58 V'], [4, 'přední motor pohání'], [8, 'měření baterie'],
  [16, 'vypnuto'], [32, 'pauza relé'],
];
export const CHG_FAULT_NAMES = ['', 'přepětí baterie', 'nabíjí příliš dlouho', 'napětí neroste'];

export enum DrvState { Off = 0, Arming = 1, Ready = 2, Active = 3, Releasing = 4, Fault = 5 }
export const DRV_STATE_NAMES = ['Vypnuto', 'Připojuji', 'Připraven', 'Pohání', 'Odpojuji', 'Porucha'];
export const DRV_REASON_NAMES: [number, string][] = [
  [1, 'vypnuto'], [2, 'chybí DAC'], [4, 'slabá baterie'], [8, 'nad limitem rychlosti'],
  [16, 'plyn držený / bez signálu'], [32, 'brzda'], [64, 'prokluz'], [128, 'bez otáček – jen podíl plynu'],
];
export const DRV_FAULT_NAMES = ['', 'signál plynu', 'DAC zmizel', 'samovolný rozběh'];

export enum Cmd {
  Front = 0x01, Tail = 0x02, Blink = 0x03, Led = 0x05, LedBright = 0x06,
  LedMode = 0x07, Lights = 0x08, AllOff = 0x09, LightMode = 0x0a,
  SetCfg = 0x10, SaveCfg = 0x11, Calibrate = 0x12, SetPasskey = 0x13, Factory = 0x14,
  PowerOff = 0x15, ChgMode = 0x16, ChgReset = 0x17, Drive = 0x18, DriveReset = 0x19,
  ThrCal = 0x1a, TripReset = 0x1b, Reboot = 0x20, Ota = 0x21,
}
export enum Arg { Off = 0, On = 1, Toggle = 2 }

export enum CfgKey {
  BlinkPeriod = 1, BlinkAutoOff, BattCal, BattLow, BattCrit, BrakeFlash, TailWithFront,
  LedBright, LedMode, LightsOnBoot, LuxOn, LuxOff, AutoBrake, BrakeDecel, MpuAxis,
  AutoOffMin, BattOff, ChgStart, ChgStop, ChgMode, WheelMm, RearPpr, FrontPpr,
  DrvAllowed, DrvAssist, DrvMaxKmh, DrvMinBatt, DrvKp, DrvKi, ThrMinMv, ThrMaxMv,
  DacIdle, DacStart, DacMax,
}
export const CFG_KEY_COUNT = CfgKey.DacMax;

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
  lightMode: LightMode;
  status2: number;
  speed: number;     // km/h
  chg: ChgState;
  drv: DrvState;
}

export interface GogenTelemetry {
  chgMode: ChgMode;
  chgReasons: number;
  chgFault: number;
  drvReasons: number;
  drvFault: number;
  drvOutput: number;   // %
  throttle: number | null; // %, null = bez signálu
  rearKmh: number;
  frontKmh: number;
  genV: number;
  lux: number | null;
  decelG: number;
  odoKm: number;
  tripKm: number;
  offInS: number | null;
  chargingMin: number;
}

export interface GogenConfig {
  fw: string;
  inputMode: 'PCF8574' | 'GPIO';
  values: Record<number, number>;   // CfgKey → hodnota
  hw: number;
}

export function parseState(v: DataView): GogenState {
  const long = v.byteLength >= 20;
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
    lightMode: long ? v.getUint8(14) : LightMode.Off,
    status2: long ? v.getUint8(15) : 0,
    speed: long ? v.getUint16(16, true) / 10 : 0,
    chg: long ? v.getUint8(18) : ChgState.Idle,
    drv: long ? v.getUint8(19) : DrvState.Off,
  };
}

export function parseTelemetry(v: DataView): GogenTelemetry {
  const lux = v.getUint16(14, true);
  const off = v.getUint16(26, true);
  const thr = v.getUint8(7);
  return {
    chgMode: v.getUint8(1),
    chgReasons: v.getUint8(2),
    chgFault: v.getUint8(3),
    drvReasons: v.getUint8(4),
    drvFault: v.getUint8(5),
    drvOutput: v.getUint8(6),
    throttle: thr === 0xff ? null : thr,
    rearKmh: v.getUint16(8, true) / 10,
    frontKmh: v.getUint16(10, true) / 10,
    genV: v.getUint16(12, true) / 10,
    lux: lux === 0xffff ? null : lux,
    decelG: v.getInt16(16, true) / 1000,
    odoKm: v.getUint32(18, true) / 1000,
    tripKm: v.getUint32(22, true) / 1000,
    offInS: off === 0xffff ? null : off,
    chargingMin: v.getUint16(28, true),
  };
}

export function parseConfig(v: DataView): GogenConfig {
  const values: Record<number, number> = {};
  const n = Math.min(CFG_KEY_COUNT, Math.floor((v.byteLength - 4) / 2));
  for (let k = 1; k <= n; k++) values[k] = v.getUint16(4 + 2 * (k - 1), true);
  const hwOff = 4 + 2 * CFG_KEY_COUNT;
  return {
    fw: `${v.getUint8(1)}.${v.getUint8(2)}`,
    inputMode: v.getUint8(3) === 1 ? 'PCF8574' : 'GPIO',
    values,
    hw: v.byteLength > hwOff ? v.getUint8(hwOff) : 0,
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

export const reasonsText = (bits: number, table: [number, string][]) =>
  table.filter(([b]) => bits & b).map(([, t]) => t).join(', ');
