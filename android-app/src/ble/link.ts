import { BleClient, numbersToDataView } from '@capacitor-community/bluetooth-le';
import { Capacitor } from '@capacitor/core';
import {
  SVC_UUID, STATE_UUID, CMD_UUID, CFG_UUID, TELEM_UUID, parseState, parseConfig, parseTelemetry,
  type GogenState, type GogenConfig, type GogenTelemetry, Cmd, Arg, Blink, OUT, ST, ST2, HW, CfgKey,
  LedMode, LightMode, ChgState, ChgMode, DrvState, PROTO_VERSION,
} from './protocol';

export interface Link {
  readonly name: string;
  readonly deviceId: string;
  send(bytes: number[], fast?: boolean): Promise<void>;
  readConfig(): Promise<GogenConfig>;
  disconnect(): Promise<void>;
}

export interface LinkCallbacks {
  onState: (s: GogenState) => void;
  onTelemetry: (t: GogenTelemetry) => void;
  onDisconnect: () => void;
}

let bleReady = false;
async function ensureBle() {
  if (bleReady) return;
  await BleClient.initialize({ androidNeverForLocation: true });
  bleReady = true;
}

export const isNative = () => Capacitor.isNativePlatform();

/** Otevře systémový výběr zařízení a vrátí deviceId. */
export async function pickDevice(): Promise<{ deviceId: string; name: string }> {
  await ensureBle();
  if (!(await BleClient.isEnabled())) {
    if (Capacitor.getPlatform() === 'android') await BleClient.requestEnable();
  }
  const d = await BleClient.requestDevice({ services: [SVC_UUID], namePrefix: 'GoGEN' });
  return { deviceId: d.deviceId, name: d.name ?? 'GoGEN' };
}

export async function connectBle(deviceId: string, name: string, cb: LinkCallbacks): Promise<Link> {
  await ensureBle();

  // Párování s PINem (Android zobrazí systémový dialog pro zadání passkey)
  if (Capacitor.getPlatform() === 'android') {
    try {
      if (!(await BleClient.isBonded(deviceId))) await BleClient.createBond(deviceId, { timeout: 60000 });
    } catch (e) {
      console.warn('createBond', e);
    }
  }

  await BleClient.connect(deviceId, () => cb.onDisconnect(), { timeout: 15000 });

  // první čtení šifrované charakteristiky vynutí párování i bez createBond
  const first = await BleClient.read(deviceId, SVC_UUID, STATE_UUID);
  const ver = first.getUint8(0);
  if (ver < PROTO_VERSION) {
    await BleClient.disconnect(deviceId).catch(() => undefined);
    throw new Error(`Firmware používá protokol v${ver}, aplikace v${PROTO_VERSION}. Aktualizuj GoGEN OS.`);
  }
  cb.onState(parseState(first));
  await BleClient.startNotifications(deviceId, SVC_UUID, STATE_UUID, (v) => cb.onState(parseState(v)));
  try {
    cb.onTelemetry(parseTelemetry(await BleClient.read(deviceId, SVC_UUID, TELEM_UUID)));
    await BleClient.startNotifications(deviceId, SVC_UUID, TELEM_UUID, (v) => cb.onTelemetry(parseTelemetry(v)));
  } catch (e) {
    console.warn('telemetry', e);
  }

  let chain: Promise<void> = Promise.resolve();
  return {
    name,
    deviceId,
    send(bytes, fast = false) {
      // zápisy řadíme za sebe – Android neumí paralelní GATT operace
      const dv = numbersToDataView(bytes);
      const op = () =>
        fast
          ? BleClient.writeWithoutResponse(deviceId, SVC_UUID, CMD_UUID, dv)
          : BleClient.write(deviceId, SVC_UUID, CMD_UUID, dv);
      chain = chain.then(op, op);
      return chain;
    },
    async readConfig() {
      await chain;
      return parseConfig(await BleClient.read(deviceId, SVC_UUID, CFG_UUID));
    },
    async disconnect() {
      for (const ch of [STATE_UUID, TELEM_UUID]) {
        try { await BleClient.stopNotifications(deviceId, SVC_UUID, ch); } catch { /* */ }
      }
      await BleClient.disconnect(deviceId);
    },
  };
}

// ---------------------------------------------------------------------
//  Demo režim – simulace firmwaru pro vývoj v prohlížeči / ukázku
// ---------------------------------------------------------------------
const DEMO_DEFAULTS: Record<number, number> = {
  [CfgKey.BlinkPeriod]: 660, [CfgKey.BlinkAutoOff]: 30, [CfgKey.BattCal]: 1000, [CfgKey.BattLow]: 3300,
  [CfgKey.BattCrit]: 3100, [CfgKey.BrakeFlash]: 1, [CfgKey.TailWithFront]: 1, [CfgKey.LedBright]: 128,
  [CfgKey.LedMode]: 0, [CfgKey.LightsOnBoot]: 0, [CfgKey.LuxOn]: 40, [CfgKey.LuxOff]: 120,
  [CfgKey.AutoBrake]: 1, [CfgKey.BrakeDecel]: 150, [CfgKey.MpuAxis]: 0, [CfgKey.AutoOffMin]: 15,
  [CfgKey.BattOff]: 3000, [CfgKey.ChgStart]: 25, [CfgKey.ChgStop]: 95, [CfgKey.ChgMode]: 0,
  [CfgKey.WheelMm]: 800, [CfgKey.RearPpr]: 15, [CfgKey.FrontPpr]: 15, [CfgKey.DrvAllowed]: 1,
  [CfgKey.DrvAssist]: 60, [CfgKey.DrvMaxKmh]: 25, [CfgKey.DrvMinBatt]: 20, [CfgKey.DrvKp]: 80,
  [CfgKey.DrvKi]: 40, [CfgKey.ThrMinMv]: 320, [CfgKey.ThrMaxMv]: 2580, [CfgKey.DacIdle]: 80,
  [CfgKey.DacStart]: 110, [CfgKey.DacMax]: 420,
};

export function connectDemo(cb: LinkCallbacks): Link {
  const cfg: GogenConfig = {
    fw: '3.0', inputMode: 'PCF8574', values: { ...DEMO_DEFAULTS }, hw: HW.SPEED | HW.DRIVE | HW.CHARGE | HW.OTA,
  };
  const val = (k: CfgKey) => cfg.values[k];
  const s = {
    lightMode: LightMode.Auto as LightMode, tail: false, led: false, bright: 128, mode: LedMode.Static as LedMode,
    blink: Blink.Off, blinkStart: 0, volts: 36.4, drive: false, chgMode: ChgMode.Auto as ChgMode,
    trip: 0, odo: 1234.5, ota: false,
  };
  const t0 = Date.now();
  let last = t0;
  const tick = () => {
    const now = Date.now();
    const dt = (now - last) / 1000; last = now;
    const tRide = ((now - t0) / 1000) % 40;
    const speed = tRide < 30 ? Math.max(0, Math.min(24, tRide * 3, (30 - tRide) * 4)) : 0;
    const brake = tRide > 25 && tRide < 30;
    const dark = ((now - t0) / 1000) % 60 > 30;
    const lit = s.lightMode === LightMode.On || (s.lightMode === LightMode.Auto && dark);
    const phase = ((now - s.blinkStart) % val(CfgKey.BlinkPeriod)) < val(CfgKey.BlinkPeriod) / 2;
    const pct = Math.round(Math.min(100, Math.max(0, ((s.volts / 10 - 3.3) / 0.9) * 100)));
    const charging = s.chgMode !== ChgMode.Off && pct <= 40 && speed > 5 && !s.drive;
    s.volts = Math.max(30, Math.min(41.5, s.volts + (charging ? 0.002 : -0.0006)));
    s.trip += (speed / 3.6) * dt / 1000;
    s.odo += (speed / 3.6) * dt / 1000;
    const driving = s.drive && speed > 0 && !brake;
    const out = (lit ? OUT.FRONT : 0) | (lit || s.tail ? OUT.TAIL : 0) | (brake ? OUT.BRAKE : 0) |
      (phase && (s.blink === Blink.Left || s.blink === Blink.Hazard) ? OUT.LEFT : 0) |
      (phase && (s.blink === Blink.Right || s.blink === Blink.Hazard) ? OUT.RIGHT : 0) |
      (s.led ? OUT.LED : 0) | (charging ? OUT.CHG : 0) | (s.drive ? OUT.PHASE : 0);
    cb.onState({
      outputs: out, ledBrightness: s.bright, ledMode: s.mode, voltage: s.volts, percent: pct,
      status: ST.BONDED | ST.INPUT_PCF | ST.INPUT_OK | ST.DISPLAY_OK | (brake ? ST.BRAKE_IN : 0),
      uptime: Math.floor((now - t0) / 1000), blink: s.blink, buttons: 0, lightMode: s.lightMode,
      status2: ST2.LUX_OK | ST2.MPU_OK | ST2.DAC_OK | ST2.SPEED_OK | ST2.DRIVE_HW | (s.ota ? ST2.OTA : 0),
      speed, chg: charging ? ChgState.Charging : pct <= 40 ? ChgState.Waiting : ChgState.Idle,
      drv: s.drive ? (driving ? DrvState.Active : DrvState.Ready) : DrvState.Off,
    });
    cb.onTelemetry({
      chgMode: s.chgMode, chgReasons: speed > 5 ? 0 : 1, chgFault: 0,
      drvReasons: s.drive ? (brake ? 32 : 0) : 1, drvFault: 0,
      drvOutput: driving ? 55 : 0, throttle: speed > 0 && !brake ? 90 : 0,
      rearKmh: speed, frontKmh: speed * (s.drive ? 1.01 : 0.99), genV: speed * 2.1,
      lux: dark ? 12 : 860, decelG: brake ? 0.22 : 0.01, odoKm: s.odo, tripKm: s.trip,
      offInS: speed > 0 ? 900 : 900 - (Math.floor((now - t0) / 1000) % 900), chargingMin: charging ? 3 : 0,
    });
  };
  const timer = setInterval(tick, 200);
  const tog = (cur: boolean, a: number) => (a === Arg.Toggle ? !cur : a === Arg.On);

  return {
    name: 'GoGEN-DEMO',
    deviceId: 'demo',
    async send(b) {
      const [c, a = 0] = b;
      switch (c) {
        case Cmd.Front: case Cmd.Lights:
          s.lightMode = tog(s.lightMode !== LightMode.Off, a) ? LightMode.On : LightMode.Off; break;
        case Cmd.LightMode: s.lightMode = a; break;
        case Cmd.Tail: s.tail = tog(s.tail, a); break;
        case Cmd.Blink: if (a !== s.blink) s.blinkStart = Date.now(); s.blink = a; break;
        case Cmd.Led: s.led = tog(s.led, a); break;
        case Cmd.LedBright: s.bright = a; break;
        case Cmd.LedMode: s.mode = a; break;
        case Cmd.AllOff: s.lightMode = LightMode.Off; s.tail = s.led = false; s.blink = Blink.Off; break;
        case Cmd.Drive: s.drive = val(CfgKey.DrvAllowed) ? tog(s.drive, a) : false; break;
        case Cmd.ChgMode: s.chgMode = a; break;
        case Cmd.TripReset: s.trip = 0; break;
        case Cmd.Ota: s.ota = !!a; break;
        case Cmd.SetCfg: cfg.values[a] = b[2] | (b[3] << 8); break;
        case Cmd.Calibrate: s.volts = (b[1] | (b[2] << 8)) / 100; break;
      }
      tick();
    },
    async readConfig() { return { ...cfg, values: { ...cfg.values } }; },
    async disconnect() { clearInterval(timer); cb.onDisconnect(); },
  };
}
