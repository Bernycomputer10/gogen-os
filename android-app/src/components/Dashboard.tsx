import { useEffect, useRef, useState } from 'react';
import { Haptics, ImpactStyle } from '@capacitor/haptics';
import {
  Arg, Blink, Cmd, LED_MODE_NAMES, OUT, ST, ST2, HW, LightMode, ChgMode, ChgState, DrvState,
  CHG_STATE_NAMES, CHG_REASON_NAMES, CHG_FAULT_NAMES, DRV_STATE_NAMES, DRV_REASON_NAMES, DRV_FAULT_NAMES,
  reasonsText, type GogenState, type GogenTelemetry, type GogenConfig, CfgKey,
} from '../ble/protocol';

type Send = (bytes: number[], fast?: boolean) => Promise<void>;

const tap = () => Haptics.impact({ style: ImpactStyle.Light }).catch(() => undefined);

function Arrow({ dir }: { dir: 'left' | 'right' }) {
  return (
    <svg viewBox="0 0 48 48" width="44" height="44" aria-hidden>
      <path d={dir === 'left' ? 'M30 8 L12 24 L30 40 Z' : 'M18 8 L36 24 L18 40 Z'} fill="currentColor" />
    </svg>
  );
}

function Seg<T extends number>({ value, options, onPick }: {
  value: T; options: [T, string][]; onPick: (v: T) => void;
}) {
  return (
    <div className="modes" style={{ gridTemplateColumns: `repeat(${options.length}, 1fr)` }}>
      {options.map(([v, label]) => (
        <button key={v} className={value === v ? 'sel' : ''} onClick={() => { tap(); onPick(v); }}>{label}</button>
      ))}
    </div>
  );
}

function Ride({ s, t, send }: { s: GogenState; t: GogenTelemetry | null; send: Send }) {
  const known = (s.status2 & ST2.SPEED_OK) !== 0;
  return (
    <section className="card ride">
      <div className="ride-top">
        <div className="speed">{known ? Math.round(s.speed) : '–'}<span>km/h</span></div>
        <div className="trip">
          <div>{t ? t.tripKm.toFixed(2) : '–'} <span>km výlet</span></div>
          <div className="muted">{t ? t.odoKm.toFixed(1) : '–'} km celkem</div>
          <button className="ghost small" onClick={() => send([Cmd.TripReset])}>Vynulovat výlet</button>
        </div>
      </div>
      {!known && <p className="muted small">Rychloměr čeká na první pulz z hallu zadního motoru.</p>}
    </section>
  );
}

function Battery({ s, t }: { s: GogenState; t: GogenTelemetry | null }) {
  const low = (s.status & ST.BAT_LOW) !== 0;
  const crit = (s.status & ST.BAT_CRIT) !== 0;
  const cls = crit ? 'crit' : low ? 'low' : 'ok';
  const charging = s.chg === ChgState.Charging;
  return (
    <section className={`card battery ${cls}`}>
      <div className="batt-top">
        <div>
          <div className="volts">{s.voltage.toFixed(1)}<span>V</span></div>
          <div className="muted">
            Lime-S 10S · {crit ? 'kritické napětí – LED vypnuty' : low ? 'slabá baterie' : 'v pořádku'}
            {charging && ` · nabíjí ${t?.chargingMin ?? 0} min`}
          </div>
        </div>
        <div className="pct">{s.percent}<span>%</span></div>
      </div>
      <div className={`bar ${charging ? 'charging' : ''}`}><div style={{ width: `${s.percent}%` }} /></div>
    </section>
  );
}

function Charge({ s, t, cfg, send }: { s: GogenState; t: GogenTelemetry | null; cfg: GogenConfig; send: Send }) {
  const why = t ? reasonsText(t.chgReasons, CHG_REASON_NAMES) : '';
  const fault = s.chg === ChgState.Fault;
  return (
    <section className={`card ${s.chg === ChgState.Charging ? 'on-green' : ''} ${fault ? 'fault' : ''}`}>
      <div className="row">
        <span className="tile-title">Dobíjení z předního kola</span>
        <span className={`badge ${s.chg === ChgState.Charging ? 'ok' : fault ? 'bad' : ''}`}>{CHG_STATE_NAMES[s.chg]}</span>
      </div>
      <p className="muted small">
        Zapne při {cfg.values[CfgKey.ChgStart]} %, vypne při {cfg.values[CfgKey.ChgStop]} % · generátor {t ? t.genV.toFixed(1) : '–'} V
        {why && ` · ${why}`}
        {fault && t && ` · porucha: ${CHG_FAULT_NAMES[t.chgFault]}`}
      </p>
      <Seg value={(t?.chgMode ?? ChgMode.Auto) as ChgMode}
        options={[[ChgMode.Auto, 'Auto'], [ChgMode.Off, 'Vypnuto'], [ChgMode.Force, 'Nabít teď']]}
        onPick={(m) => send([Cmd.ChgMode, m])} />
      {fault && <button className="small danger wide" onClick={() => send([Cmd.ChgReset])}>Zrušit poruchu</button>}
    </section>
  );
}

function Drive({ s, t, cfg, send }: { s: GogenState; t: GogenTelemetry | null; cfg: GogenConfig; send: Send }) {
  const allowed = cfg.values[CfgKey.DrvAllowed] === 1;
  const on = s.drv !== DrvState.Off && s.drv !== DrvState.Fault;
  const fault = s.drv === DrvState.Fault;
  const why = t ? reasonsText(t.drvReasons & ~1, DRV_REASON_NAMES) : '';
  return (
    <section className={`card ${s.drv === DrvState.Active ? 'on' : ''} ${fault ? 'fault' : ''}`}>
      <div className="row">
        <span className="tile-title">Přední pohon</span>
        <button className={`switch ${on ? 'on' : ''}`} disabled={!allowed}
          onClick={() => { tap(); send([Cmd.Drive, Arg.Toggle]); }} aria-label="Přední pohon zap/vyp"><span /></button>
      </div>
      {!allowed ? (
        <p className="muted small">Zakázáno v Nastavení → Přední pohon. Nejdřív zkalibruj plyn a DAC.</p>
      ) : (
        <>
          <div className="stats">
            <div><b>{DRV_STATE_NAMES[s.drv]}</b><span>stav</span></div>
            <div><b>{t?.throttle ?? '–'}{t?.throttle != null && ' %'}</b><span>plyn</span></div>
            <div><b>{t ? `${t.drvOutput} %` : '–'}</b><span>přední</span></div>
            <div><b>{t ? `${t.rearKmh.toFixed(0)}/${t.frontKmh.toFixed(0)}` : '–'}</b><span>zadní/přední</span></div>
          </div>
          {why && <p className="muted small">{why}</p>}
          {fault && t && <p className="small" style={{ color: 'var(--red)' }}>Porucha: {DRV_FAULT_NAMES[t.drvFault]}</p>}
          {fault && <button className="small danger wide" onClick={() => send([Cmd.DriveReset])}>Zrušit poruchu</button>}
        </>
      )}
    </section>
  );
}

export function Dashboard({ s, t, cfg, send }: { s: GogenState; t: GogenTelemetry | null; cfg: GogenConfig; send: Send }) {
  const out = s.outputs;
  const led = (out & OUT.LED) !== 0;
  const [bright, setBright] = useState(s.ledBrightness);
  const dragging = useRef(false);
  const sendTimer = useRef<number | undefined>(undefined);

  useEffect(() => { if (!dragging.current) setBright(s.ledBrightness); }, [s.ledBrightness]);

  const blink = (b: Blink) => { tap(); send([Cmd.Blink, s.blink === b ? Blink.Off : b]); };
  const onBright = (v: number) => {
    setBright(v);
    window.clearTimeout(sendTimer.current);
    sendTimer.current = window.setTimeout(() => send([Cmd.LedBright, v], true), 40);
  };
  const luxOk = (s.status2 & ST2.LUX_OK) !== 0;

  return (
    <div className="dash">
      {cfg.hw & HW.SPEED ? <Ride s={s} t={t} send={send} /> : null}
      <Battery s={s} t={t} />

      <section className="blinkers">
        <button
          className={`blink left ${s.blink === Blink.Left || s.blink === Blink.Hazard ? 'armed' : ''} ${out & OUT.LEFT ? 'lit' : ''}`}
          onClick={() => blink(Blink.Left)} aria-label="Levý blinkr">
          <Arrow dir="left" />
        </button>
        <button
          className={`hazard ${s.blink === Blink.Hazard ? 'armed' : ''} ${s.blink === Blink.Hazard && out & (OUT.LEFT | OUT.RIGHT) ? 'lit' : ''}`}
          onClick={() => blink(Blink.Hazard)} aria-label="Výstražná světla">
          <svg viewBox="0 0 48 48" width="34" height="34" aria-hidden>
            <path d="M24 6 L44 40 H4 Z" fill="none" stroke="currentColor" strokeWidth="4" strokeLinejoin="round" />
            <path d="M24 18 L33 34 H15 Z" fill="none" stroke="currentColor" strokeWidth="3" strokeLinejoin="round" />
          </svg>
        </button>
        <button
          className={`blink right ${s.blink === Blink.Right || s.blink === Blink.Hazard ? 'armed' : ''} ${out & OUT.RIGHT ? 'lit' : ''}`}
          onClick={() => blink(Blink.Right)} aria-label="Pravý blinkr">
          <Arrow dir="right" />
        </button>
      </section>

      <section className={`card ${out & OUT.FRONT ? 'on' : ''}`}>
        <div className="row">
          <span className="tile-title">Světla</span>
          <span className="chips">
            <i className={out & OUT.FRONT ? 'on' : ''}>přední</i>
            <i className={out & OUT.TAIL ? 'on' : ''}>zadní</i>
            <i className={out & OUT.BRAKE ? 'on brake' : ''}>brzda{s.status & ST.AUTO_BRAKE ? ' (auto)' : ''}</i>
          </span>
        </div>
        <Seg value={s.lightMode}
          options={luxOk ? [[LightMode.Off, 'Vyp'], [LightMode.On, 'Zap'], [LightMode.Auto, 'Auto']]
            : [[LightMode.Off, 'Vyp'], [LightMode.On, 'Zap']]}
          onPick={(m) => send([Cmd.LightMode, m])} />
        <p className="muted small" style={{ marginBottom: 0 }}>
          {luxOk ? `Okolí ${t?.lux ?? '–'} lx · rozsvítí pod ${cfg.values[CfgKey.LuxOn]} lx` : 'Senzor světla BH1750 nenalezen'}
          {s.status2 & ST2.EMERGENCY ? ' · PRUDKÉ BRZDĚNÍ' : ''}
        </p>
      </section>

      <section className={`card led ${led ? 'on' : ''}`}>
        <div className="row">
          <span className="tile-title">LED pásky</span>
          <button className={`switch ${led ? 'on' : ''}`} onClick={() => { tap(); send([Cmd.Led, Arg.Toggle]); }}
            aria-label="LED pásky zap/vyp"><span /></button>
        </div>
        <div className="row">
          <input type="range" min={0} max={255} value={bright}
            onPointerDown={() => (dragging.current = true)}
            onPointerUp={() => (dragging.current = false)}
            onChange={(e) => onBright(+e.target.value)} />
          <span className="num">{Math.round((bright / 255) * 100)} %</span>
        </div>
        <div className="modes">
          {LED_MODE_NAMES.map((n, i) => (
            <button key={n} className={s.ledMode === i ? 'sel' : ''} onClick={() => { tap(); send([Cmd.LedMode, i]); }}>{n}</button>
          ))}
        </div>
      </section>

      {cfg.hw & HW.CHARGE ? <Charge s={s} t={t} cfg={cfg} send={send} /> : null}
      {cfg.hw & HW.DRIVE ? <Drive s={s} t={t} cfg={cfg} send={send} /> : null}

      <div className="btnrow" style={{ marginBottom: 10 }}>
        <button className="alloff" style={{ flex: 1 }} onClick={() => { tap(); send([Cmd.AllOff]); }}>Zhasnout vše</button>
        <button className="alloff danger" style={{ flex: 1 }} onClick={() => {
          if (window.confirm('Vypnout GoGEN? Zapne se tlačítkem napájení (nebo stiskem brzdy).')) send([Cmd.PowerOff]);
        }}>Vypnout GoGEN</button>
      </div>

      <footer className="muted foot">
        {s.status & ST.INPUT_PCF ? 'Ovladač PCF8574' : 'Ovladač GPIO'} {s.status & ST.INPUT_OK ? '✓' : '✗'} ·
        OLED {s.status & ST.DISPLAY_OK ? '✓' : '–'} · světlo {luxOk ? '✓' : '–'} ·
        MPU {s.status2 & ST2.MPU_OK ? '✓' : '–'} · DAC {s.status2 & ST2.DAC_OK ? '✓' : '–'} ·
        běží {Math.floor(s.uptime / 60)} min
        {t?.offInS != null && s.status2 & ST2.OFF_SOON ? ` · vypnutí za ${t.offInS} s` : ''}
      </footer>
    </div>
  );
}
