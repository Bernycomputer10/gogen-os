import { useEffect, useState } from 'react';
import { CfgKey, Cmd, encode, type GogenConfig } from '../ble/protocol';

type Send = (bytes: number[], fast?: boolean) => Promise<void>;

interface Props {
  cfg: GogenConfig;
  send: Send;
  reload: () => Promise<void>;
  voltage?: number;
}

function Num({ label, value, unit, min, max, step = 1, onCommit }: {
  label: string; value: number; unit?: string; min: number; max: number; step?: number;
  onCommit: (v: number) => void;
}) {
  const [v, setV] = useState(String(value));
  useEffect(() => setV(String(value)), [value]);
  const commit = () => {
    const n = Number(v.replace(',', '.'));
    if (Number.isFinite(n) && n >= min && n <= max) onCommit(n);
    else setV(String(value));
  };
  return (
    <label className="field">
      <span>{label}</span>
      <span className="inp">
        <input inputMode="decimal" value={v} onChange={(e) => setV(e.target.value)} onBlur={commit}
          onKeyDown={(e) => e.key === 'Enter' && (e.target as HTMLInputElement).blur()} step={step} />
        {unit && <em>{unit}</em>}
      </span>
    </label>
  );
}

function Toggle({ label, value, onChange }: { label: string; value: boolean; onChange: (v: boolean) => void }) {
  return (
    <label className="field">
      <span>{label}</span>
      <button className={`switch ${value ? 'on' : ''}`} onClick={() => onChange(!value)} aria-label={label}><span /></button>
    </label>
  );
}

export function Settings({ cfg, send, reload, voltage }: Props) {
  const [dirty, setDirty] = useState(false);
  const [realV, setRealV] = useState('');
  const [pin, setPin] = useState('');
  const [note, setNote] = useState<string | null>(null);

  const set = async (k: CfgKey, v: number) => {
    await send(encode.setCfg(k, v));
    setDirty(true);
    await reload();
  };
  const flash = (t: string) => { setNote(t); window.setTimeout(() => setNote(null), 2500); };

  return (
    <div className="settings">
      <section className="card">
        <h3>Blinkry</h3>
        <Num label="Perioda blikání" unit="ms" value={cfg.blinkPeriodMs} min={300} max={2000}
          onCommit={(v) => set(CfgKey.BlinkPeriod, v)} />
        <Num label="Automatické vypnutí (0 = ne)" unit="s" value={cfg.blinkAutoOffS} min={0} max={600}
          onCommit={(v) => set(CfgKey.BlinkAutoOff, v)} />
      </section>

      <section className="card">
        <h3>Světla</h3>
        <Toggle label="Zadní světlo s předním" value={cfg.tailWithFront} onChange={(v) => set(CfgKey.TailWithFront, +v)} />
        <Toggle label="Brzdové světlo 3× problikne" value={cfg.brakeFlash} onChange={(v) => set(CfgKey.BrakeFlash, +v)} />
        <Toggle label="Světla zapnout po startu" value={cfg.lightsOnBoot} onChange={(v) => set(CfgKey.LightsOnBoot, +v)} />
      </section>

      <section className="card">
        <h3>Baterie</h3>
        <Num label="Varování – slabá" unit="V" value={cfg.battLow} min={28} max={40} step={0.1}
          onCommit={(v) => set(CfgKey.BattLow, v * 100)} />
        <Num label="Kritická (vypne LED)" unit="V" value={cfg.battCrit} min={27} max={40} step={0.1}
          onCommit={(v) => set(CfgKey.BattCrit, v * 100)} />
        <div className="field">
          <span>Kalibrace děliče <small className="muted">nyní {voltage?.toFixed(2) ?? '–'} V · k = {(cfg.battCalPermille / 1000).toFixed(3)}</small></span>
          <span className="inp">
            <input inputMode="decimal" placeholder="multimetr" value={realV} onChange={(e) => setRealV(e.target.value)} />
            <em>V</em>
            <button className="small" onClick={async () => {
              const n = Number(realV.replace(',', '.'));
              if (!(n > 20 && n < 50)) return flash('Zadej napětí 20–50 V');
              await send(encode.calibrate(n));
              await reload();
              setRealV('');
              flash('Kalibrace uložena');
            }}>Nastavit</button>
          </span>
        </div>
      </section>

      <section className="card">
        <h3>Zabezpečení</h3>
        <div className="field">
          <span>Nový PIN pro párování <small className="muted">6 číslic, ESP se restartuje a smaže párování</small></span>
          <span className="inp">
            <input inputMode="numeric" maxLength={6} placeholder="000000" value={pin}
              onChange={(e) => setPin(e.target.value.replace(/\D/g, ''))} />
            <button className="small" onClick={async () => {
              if (pin.length !== 6) return flash('PIN musí mít 6 číslic');
              await send(encode.passkey(Number(pin)));
              flash('PIN změněn – spáruj telefon znovu (v nastavení Bluetooth zapomeň zařízení)');
            }}>Změnit</button>
          </span>
        </div>
      </section>

      <section className="card">
        <h3>Systém</h3>
        <p className="muted">Firmware GoGEN OS {cfg.fw} · vstupy {cfg.inputMode}</p>
        <div className="btnrow">
          <button className="small" onClick={() => send([Cmd.Reboot])}>Restart ESP</button>
          <button className="small danger" onClick={() => {
            if (window.confirm('Obnovit tovární nastavení a smazat párování?')) send([Cmd.Factory]);
          }}>Tovární reset</button>
        </div>
      </section>

      <button className={`save ${dirty ? 'dirty' : ''}`} onClick={async () => {
        await send([Cmd.SaveCfg]);
        setDirty(false);
        flash('Uloženo do paměti ESP');
      }}>{dirty ? 'Uložit změny do ESP' : 'Nastavení uloženo'}</button>

      {note && <div className="toast">{note}</div>}
    </div>
  );
}
