import { useEffect, useState } from 'react';
import { CfgKey, Cmd, encode, HW, ST2, type GogenConfig, type GogenState } from '../ble/protocol';

type Send = (bytes: number[], fast?: boolean) => Promise<void>;

interface Props {
  cfg: GogenConfig;
  state: GogenState;
  name?: string;
  send: Send;
  reload: () => Promise<GogenConfig | null>;
}

// Popis jednoho klíče: hodnota v ESP je u16, v aplikaci se zobrazuje raw / scale
type Field =
  | { k: CfgKey; label: string; unit?: string; scale?: number; min: number; max: number; hint?: string }
  | { k: CfgKey; label: string; bool: true; hint?: string }
  | { k: CfgKey; label: string; options: string[]; hint?: string };

const SECTIONS: { title: string; hw?: number; fields: Field[] }[] = [
  { title: 'Blinkry', fields: [
    { k: CfgKey.BlinkPeriod, label: 'Perioda blikání', unit: 'ms', min: 300, max: 2000 },
    { k: CfgKey.BlinkAutoOff, label: 'Automatické vypnutí (0 = ne)', unit: 's', min: 0, max: 600 },
  ] },
  { title: 'Světla', fields: [
    { k: CfgKey.LightsOnBoot, label: 'Po zapnutí', options: ['Vypnuto', 'Zapnuto', 'Auto'] },
    { k: CfgKey.TailWithFront, label: 'Zadní světlo s předním', bool: true },
    { k: CfgKey.BrakeFlash, label: 'Brzdové světlo 3× problikne', bool: true },
    { k: CfgKey.LuxOn, label: 'Auto: rozsvítit pod', unit: 'lx', min: 1, max: 2000 },
    { k: CfgKey.LuxOff, label: 'Auto: zhasnout nad', unit: 'lx', min: 2, max: 5000 },
  ] },
  { title: 'Automatické brzdové světlo', fields: [
    { k: CfgKey.AutoBrake, label: 'Brzdit podle akcelerometru', bool: true, hint: 'MPU6050 pevně v krabičce na rámu' },
    { k: CfgKey.BrakeDecel, label: 'Práh zpomalení', unit: 'g', scale: 1000, min: 50, max: 600 },
    { k: CfgKey.MpuAxis, label: 'Osa ve směru jízdy', options: ['+X', '−X', '+Y', '−Y', '+Z', '−Z'] },
  ] },
  { title: 'Baterie a napájení', fields: [
    { k: CfgKey.BattLow, label: 'Varování – slabá', unit: 'V', scale: 100, min: 2800, max: 4000 },
    { k: CfgKey.BattCrit, label: 'Kritická (vypne LED)', unit: 'V', scale: 100, min: 2700, max: 4000 },
    { k: CfgKey.BattOff, label: 'Vypnout GoGEN pod (30 s)', unit: 'V', scale: 100, min: 2500, max: 3600, hint: 'ochrana článků před hlubokým vybitím' },
    { k: CfgKey.AutoOffMin, label: 'Vypnout po nečinnosti (0 = nikdy)', unit: 'min', min: 0, max: 240 },
  ] },
  { title: 'Dobíjení z předního kola', hw: HW.CHARGE, fields: [
    { k: CfgKey.ChgStart, label: 'Začít nabíjet při', unit: '%', min: 5, max: 90 },
    { k: CfgKey.ChgStop, label: 'Přestat nabíjet při', unit: '%', min: 20, max: 100 },
    { k: CfgKey.ChgMode, label: 'Po zapnutí', options: ['Automaticky', 'Vypnuto'] },
  ] },
  { title: 'Kolo a rychloměr', hw: HW.SPEED, fields: [
    { k: CfgKey.WheelMm, label: 'Obvod kola', unit: 'mm', min: 200, max: 3000, hint: '10" kolo ≈ 800 mm' },
    { k: CfgKey.RearPpr, label: 'Pulzů na otáčku – zadní', min: 1, max: 120, hint: 'počet pólových párů motoru' },
    { k: CfgKey.FrontPpr, label: 'Pulzů na otáčku – přední', min: 1, max: 120 },
  ] },
  { title: 'Přední pohon', hw: HW.DRIVE, fields: [
    { k: CfgKey.DrvAllowed, label: 'Povolit přední pohon', bool: true },
    { k: CfgKey.DrvAssist, label: 'Podíl plynu', unit: '%', min: 0, max: 100 },
    { k: CfgKey.DrvMaxKmh, label: 'Odpojit fáze nad', unit: 'km/h', min: 5, max: 60, hint: '48V motor na 36V baterii: max ~75 % jeho rychlosti' },
    { k: CfgKey.DrvMinBatt, label: 'Vypnout pod', unit: '%', min: 5, max: 80 },
    { k: CfgKey.DrvKp, label: 'Regulátor Kp', scale: 100, min: 0, max: 500 },
    { k: CfgKey.DrvKi, label: 'Regulátor Ki', unit: '1/s', scale: 100, min: 0, max: 500 },
    { k: CfgKey.DacIdle, label: 'DAC v klidu', unit: 'V', scale: 100, min: 0, max: 500 },
    { k: CfgKey.DacStart, label: 'DAC začátek plynu', unit: 'V', scale: 100, min: 0, max: 500 },
    { k: CfgKey.DacMax, label: 'DAC plný plyn', unit: 'V', scale: 100, min: 0, max: 500 },
  ] },
];

function Num({ label, hint, value, rev, unit, scale = 1, min, max, onCommit }: {
  label: string; hint?: string; value: number; rev: number; unit?: string; scale?: number; min: number; max: number;
  onCommit: (raw: number) => void;
}) {
  const shown = (raw: number) => String(+(raw / scale).toFixed(scale >= 1000 ? 3 : scale >= 100 ? 2 : 0));
  const [v, setV] = useState(shown(value));
  useEffect(() => setV(shown(value)), [value, rev]); // eslint-disable-line react-hooks/exhaustive-deps
  const commit = () => {
    const raw = Math.round(Number(v.replace(',', '.')) * scale);
    if (Number.isFinite(raw) && raw >= min && raw <= max) onCommit(raw);
    else setV(shown(value));
  };
  return (
    <label className="field">
      <span>{label}{hint && <small className="muted">{hint}</small>}</span>
      <span className="inp">
        <input inputMode="decimal" value={v} onChange={(e) => setV(e.target.value)} onBlur={commit}
          onKeyDown={(e) => e.key === 'Enter' && (e.target as HTMLInputElement).blur()} />
        {unit && <em>{unit}</em>}
      </span>
    </label>
  );
}

function Toggle({ label, hint, value, onChange }: { label: string; hint?: string; value: boolean; onChange: (v: boolean) => void }) {
  return (
    <label className="field">
      <span>{label}{hint && <small className="muted">{hint}</small>}</span>
      <button className={`switch ${value ? 'on' : ''}`} onClick={() => onChange(!value)} aria-label={label}><span /></button>
    </label>
  );
}

function Select({ label, hint, value, options, onChange }: {
  label: string; hint?: string; value: number; options: string[]; onChange: (v: number) => void;
}) {
  return (
    <label className="field">
      <span>{label}{hint && <small className="muted">{hint}</small>}</span>
      <select className="sel" value={value} onChange={(e) => onChange(+e.target.value)}>
        {options.map((o, i) => <option key={o} value={i}>{o}</option>)}
      </select>
    </label>
  );
}

export function Settings({ cfg, state, name, send, reload }: Props) {
  const [dirty, setDirty] = useState(false);
  const [realV, setRealV] = useState('');
  const [pin, setPin] = useState('');
  const [note, setNote] = useState<string | null>(null);

  const flash = (t: string) => { setNote(t); window.setTimeout(() => setNote(null), 3000); };
  const [rev, setRev] = useState(0);
  const set = async (k: CfgKey, raw: number) => {
    await send(encode.setCfg(k, raw));
    setDirty(true);
    const c = await reload();
    setRev((r) => r + 1);
    // ESP hodnotu odmítne, pokud je mimo rozsah nebo nesedí s jinou (např. start ≥ stop)
    if (c && c.values[k] !== raw) flash('ESP hodnotu odmítl – mimo rozsah nebo v rozporu s jiným nastavením');
  };

  const field = (f: Field) => {
    const v = cfg.values[f.k] ?? 0;
    if ('bool' in f) return <Toggle key={f.k} label={f.label} hint={f.hint} value={v === 1} onChange={(b) => set(f.k, +b)} />;
    if ('options' in f) return <Select key={f.k} label={f.label} hint={f.hint} value={v} options={f.options} onChange={(x) => set(f.k, x)} />;
    return <Num key={f.k} {...f} value={v} rev={rev} onCommit={(raw) => set(f.k, raw)} />;
  };

  const otaOn = (state.status2 & ST2.OTA) !== 0;

  return (
    <div className="settings">
      {SECTIONS.filter((sec) => !sec.hw || cfg.hw & sec.hw).map((sec) => (
        <section className="card" key={sec.title}>
          <h3>{sec.title}</h3>
          {sec.fields.map(field)}

          {sec.title === 'Baterie a napájení' && (
            <div className="field">
              <span>Kalibrace děliče <small className="muted">nyní {state.voltage.toFixed(2)} V · k = {((cfg.values[CfgKey.BattCal] ?? 1000) / 1000).toFixed(3)}</small></span>
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
          )}

          {sec.title === 'Přední pohon' && (
            <div className="field">
              <span>Kalibrace plynu
                <small className="muted">puštěný {cfg.values[CfgKey.ThrMinMv]} mV · plný {cfg.values[CfgKey.ThrMaxMv]} mV · pohon musí být vypnutý</small>
              </span>
              <span className="inp">
                <button className="small" onClick={async () => { await send([Cmd.ThrCal, 0]); setDirty(true); await reload(); flash('Puštěný plyn uložen'); }}>Puštěný</button>
                <button className="small" onClick={async () => { await send([Cmd.ThrCal, 1]); setDirty(true); await reload(); flash('Plný plyn uložen'); }}>Plný</button>
              </span>
            </div>
          )}
        </section>
      ))}

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
        <p className="muted">GoGEN OS {cfg.fw} · vstupy {cfg.inputMode}</p>
        {cfg.hw & HW.OTA ? (
          <div className="field">
            <span>Aktualizace přes Wi-Fi
              <small className="muted">{otaOn
                ? `Připoj se k síti ${name ?? 'GoGEN'}-OTA (heslo „gg“ + PIN) a otevři http://192.168.4.1`
                : 'Koloběžka musí stát, přední pohon se vypne'}</small>
            </span>
            <button className="small" onClick={() => send([Cmd.Ota, otaOn ? 0 : 1])}>{otaOn ? 'Zastavit' : 'Spustit'}</button>
          </div>
        ) : null}
        <div className="btnrow">
          <button className="small" onClick={() => send([Cmd.Reboot])}>Restart ESP</button>
          <button className="small danger" onClick={() => {
            if (window.confirm('Obnovit tovární nastavení a smazat párování? Ujeté km zůstanou.')) send([Cmd.Factory]);
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
