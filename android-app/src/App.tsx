import { useState } from 'react';
import { useGogen } from './useGogen';
import { Dashboard } from './components/Dashboard';
import { Settings } from './components/Settings';
import { isNative } from './ble/link';
import { ST } from './ble/protocol';

export default function App() {
  const g = useGogen();
  const [tab, setTab] = useState<'dash' | 'cfg'>('dash');
  const connected = g.status === 'connected' && g.state;

  return (
    <div className="app">
      <header className="top">
        <div className="brand">Go<b>GEN</b></div>
        <div className={`conn ${g.status}`}>
          <i />
          {g.status === 'connected' ? g.name : g.status === 'reconnecting' ? 'Obnovuji spojení…' :
            g.status === 'connecting' ? 'Připojuji…' : 'Odpojeno'}
          {g.state && g.state.status & ST.BONDED ? ' 🔒' : ''}
        </div>
        {g.status !== 'idle' && <button className="ghost" onClick={g.disconnect}>Odpojit</button>}
      </header>

      {g.error && <div className="error" onClick={g.clearError}>{g.error}</div>}

      {!connected && g.status !== 'reconnecting' && (
        <main className="connect">
          <div className="logo">
            <svg viewBox="0 0 64 64" width="96" height="96" aria-hidden>
              <circle cx="32" cy="32" r="28" fill="none" stroke="currentColor" strokeWidth="3" />
              <path d="M36 10 L20 36 H31 L27 54 L44 27 H33 Z" fill="currentColor" />
            </svg>
          </div>
          <h1>GoGEN V2</h1>
          <p className="muted">Sekundární okruh · světla, blinkry, LED</p>
          <button className="primary" disabled={g.status === 'connecting'} onClick={g.scan}>
            {g.status === 'connecting' ? 'Připojuji…' : 'Hledat koloběžku'}
          </button>
          {g.lastDevice && (
            <button className="secondary" disabled={g.status === 'connecting'} onClick={() => g.open(g.lastDevice!)}>
              Připojit {g.lastDevice.name}
            </button>
          )}
          <button className="ghost" onClick={g.demo}>Demo režim</button>
          {!isNative() && <p className="muted small">V prohlížeči funguje BLE jen v Chrome (Web Bluetooth).</p>}
          <p className="muted small">Při prvním připojení zadej PIN (výchozí 123456).</p>
        </main>
      )}

      {g.status === 'reconnecting' && !connected && (
        <main className="connect"><div className="spinner" /><p>Koloběžka je mimo dosah – zkouším znovu…</p></main>
      )}

      {connected && g.state && (
        <>
          <main className="content">
            {tab === 'dash' && <Dashboard s={g.state} send={g.send} />}
            {tab === 'cfg' && g.config && (
              <Settings cfg={g.config} send={g.send} reload={g.reloadConfig} voltage={g.state.voltage} />
            )}
          </main>
          <nav className="tabs">
            <button className={tab === 'dash' ? 'sel' : ''} onClick={() => setTab('dash')}>Ovládání</button>
            <button className={tab === 'cfg' ? 'sel' : ''} onClick={() => setTab('cfg')}>Nastavení</button>
          </nav>
        </>
      )}
    </div>
  );
}
