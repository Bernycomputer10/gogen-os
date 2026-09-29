import { useEffect, useRef, useState } from 'react';
import { Haptics, ImpactStyle } from '@capacitor/haptics';
import { Arg, Blink, Cmd, LED_MODE_NAMES, OUT, ST, type GogenState } from '../ble/protocol';

type Send = (bytes: number[], fast?: boolean) => Promise<void>;

const tap = () => Haptics.impact({ style: ImpactStyle.Light }).catch(() => undefined);

function Arrow({ dir }: { dir: 'left' | 'right' }) {
  return (
    <svg viewBox="0 0 48 48" width="44" height="44" aria-hidden>
      <path d={dir === 'left' ? 'M30 8 L12 24 L30 40 Z' : 'M18 8 L36 24 L18 40 Z'} fill="currentColor" />
    </svg>
  );
}

function Battery({ s }: { s: GogenState }) {
  const low = (s.status & ST.BAT_LOW) !== 0;
  const crit = (s.status & ST.BAT_CRIT) !== 0;
  const cls = crit ? 'crit' : low ? 'low' : 'ok';
  return (
    <section className={`card battery ${cls}`}>
      <div className="batt-top">
        <div>
          <div className="volts">{s.voltage.toFixed(1)}<span>V</span></div>
          <div className="muted">Lime-S 10S · {crit ? 'kritické napětí – LED vypnuty' : low ? 'slabá baterie' : 'v pořádku'}</div>
        </div>
        <div className="pct">{s.percent}<span>%</span></div>
      </div>
      <div className="bar"><div style={{ width: `${s.percent}%` }} /></div>
    </section>
  );
}

export function Dashboard({ s, send }: { s: GogenState; send: Send }) {
  const out = s.outputs;
  const lights = (out & OUT.FRONT) !== 0;
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

  return (
    <div className="dash">
      <Battery s={s} />

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

      <section className="grid1">
        <button className={`tile ${lights ? 'on' : ''}`} onClick={() => { tap(); send([Cmd.Lights, Arg.Toggle]); }}>
          <span className="tile-title">Světla</span>
          <span className="chips">
            <i className={out & OUT.FRONT ? 'on' : ''}>přední</i>
            <i className={out & OUT.TAIL ? 'on' : ''}>zadní</i>
            <i className={out & OUT.BRAKE ? 'on brake' : ''}>brzda</i>
          </span>
        </button>
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

      <button className="alloff" onClick={() => { tap(); send([Cmd.AllOff]); }}>Vypnout vše</button>

      <footer className="muted foot">
        {s.status & ST.INPUT_PCF ? 'Ovladač PCF8574' : 'Ovladač GPIO'} {s.status & ST.INPUT_OK ? '✓' : '✗'} ·
        OLED {s.status & ST.DISPLAY_OK ? '✓' : '–'} · běží {Math.floor(s.uptime / 60)} min
      </footer>
    </div>
  );
}
