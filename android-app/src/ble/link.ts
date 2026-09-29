import { BleClient, numbersToDataView } from '@capacitor-community/bluetooth-le';
import { Capacitor } from '@capacitor/core';
import {
  SVC_UUID, STATE_UUID, CMD_UUID, CFG_UUID, parseState, parseConfig,
  type GogenState, type GogenConfig, Cmd, Arg, Blink, OUT, ST, CfgKey, LedMode,
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
  cb.onState(parseState(first));
  await BleClient.startNotifications(deviceId, SVC_UUID, STATE_UUID, (v) => cb.onState(parseState(v)));

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
      try { await BleClient.stopNotifications(deviceId, SVC_UUID, STATE_UUID); } catch { /* */ }
      await BleClient.disconnect(deviceId);
    },
  };
}

// ---------------------------------------------------------------------
//  Demo režim – simulace firmwaru pro vývoj v prohlížeči / ukázku
// ---------------------------------------------------------------------
export function connectDemo(cb: LinkCallbacks): Link {
  const cfg: GogenConfig = {
    fw: '1.0', inputMode: 'PCF8574', blinkPeriodMs: 660, blinkAutoOffS: 30, battCalPermille: 1000,
    battLow: 33, battCrit: 31, brakeFlash: true, tailWithFront: true,
    ledBrightness: 128, ledMode: LedMode.Static, lightsOnBoot: false,
  };
  const s = { front: false, tail: false, led: false, bright: 128, mode: LedMode.Static as LedMode,
    blink: Blink.Off, blinkStart: 0, brake: false, volts: 38.6 };
  const t0 = Date.now();
  const tick = () => {
    const now = Date.now();
    const phase = ((now - s.blinkStart) % cfg.blinkPeriodMs) < cfg.blinkPeriodMs / 2;
    s.brake = (now / 1000) % 9 < 1.2;
    s.volts = Math.max(30, s.volts - 0.0004);
    const cell = s.volts / 10;
    const pct = Math.round(Math.min(100, Math.max(0, ((cell - 3.3) / 0.9) * 100)));
    const out = (s.front ? OUT.FRONT : 0) | (s.tail ? OUT.TAIL : 0) | (s.brake ? OUT.BRAKE : 0) |
      (phase && (s.blink === Blink.Left || s.blink === Blink.Hazard) ? OUT.LEFT : 0) |
      (phase && (s.blink === Blink.Right || s.blink === Blink.Hazard) ? OUT.RIGHT : 0) |
      (s.led ? OUT.LED : 0);
    cb.onState({
      outputs: out, ledBrightness: s.bright, ledMode: s.mode, voltage: s.volts, percent: pct,
      status: ST.BONDED | ST.INPUT_PCF | ST.INPUT_OK | ST.DISPLAY_OK | (s.brake ? ST.BRAKE_IN : 0),
      uptime: Math.floor((now - t0) / 1000), blink: s.blink, buttons: 0,
    });
  };
  const timer = setInterval(tick, 100);
  const tog = (cur: boolean, a: number) => (a === Arg.Toggle ? !cur : a === Arg.On);

  return {
    name: 'GoGEN-DEMO',
    deviceId: 'demo',
    async send(b) {
      const [c, a = 0] = b;
      switch (c) {
        case Cmd.Front: s.front = tog(s.front, a); if (cfg.tailWithFront) s.tail = s.front; break;
        case Cmd.Tail: s.tail = tog(s.tail, a); break;
        case Cmd.Lights: s.front = tog(s.front, a); s.tail = s.front; break;
        case Cmd.Blink: if (a !== s.blink) s.blinkStart = Date.now(); s.blink = a; break;
        case Cmd.Led: s.led = tog(s.led, a); break;
        case Cmd.LedBright: s.bright = a; break;
        case Cmd.LedMode: s.mode = a; break;
        case Cmd.AllOff: s.front = s.tail = s.led = false; s.blink = Blink.Off; break;
        case Cmd.SetCfg: {
          const v = b[2] | (b[3] << 8);
          const map: Partial<Record<CfgKey, () => void>> = {
            [CfgKey.BlinkPeriod]: () => (cfg.blinkPeriodMs = v),
            [CfgKey.BlinkAutoOff]: () => (cfg.blinkAutoOffS = v),
            [CfgKey.BattLow]: () => (cfg.battLow = v / 100),
            [CfgKey.BattCrit]: () => (cfg.battCrit = v / 100),
            [CfgKey.BrakeFlash]: () => (cfg.brakeFlash = !!v),
            [CfgKey.TailWithFront]: () => (cfg.tailWithFront = !!v),
            [CfgKey.LedBright]: () => (cfg.ledBrightness = v),
            [CfgKey.LedMode]: () => (cfg.ledMode = v),
            [CfgKey.LightsOnBoot]: () => (cfg.lightsOnBoot = !!v),
          };
          map[a as CfgKey]?.();
          break;
        }
        case Cmd.Calibrate: s.volts = (b[1] | (b[2] << 8)) / 100; break;
      }
      tick();
    },
    async readConfig() { return { ...cfg }; },
    async disconnect() { clearInterval(timer); cb.onDisconnect(); },
  };
}
