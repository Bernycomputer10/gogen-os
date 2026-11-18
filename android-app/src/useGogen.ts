import { useCallback, useEffect, useRef, useState } from 'react';
import { Preferences } from '@capacitor/preferences';
import { connectBle, connectDemo, pickDevice, type Link } from './ble/link';
import type { GogenConfig, GogenState, GogenTelemetry } from './ble/protocol';

export type ConnStatus = 'idle' | 'connecting' | 'connected' | 'reconnecting';

const LAST_KEY = 'gogen.lastDevice';

export function useGogen() {
  const [status, setStatus] = useState<ConnStatus>('idle');
  const [state, setState] = useState<GogenState | null>(null);
  const [telem, setTelem] = useState<GogenTelemetry | null>(null);
  const [config, setConfig] = useState<GogenConfig | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [lastDevice, setLastDevice] = useState<{ deviceId: string; name: string } | null>(null);
  const link = useRef<Link | null>(null);
  const manualClose = useRef(false);
  const retryTimer = useRef<number | undefined>(undefined);

  useEffect(() => {
    Preferences.get({ key: LAST_KEY }).then(({ value }) => {
      if (value) setLastDevice(JSON.parse(value));
    });
    return () => window.clearTimeout(retryTimer.current);
  }, []);

  const open = useCallback(async (dev: { deviceId: string; name: string }, isRetry = false) => {
    setStatus(isRetry ? 'reconnecting' : 'connecting');
    setError(null);
    manualClose.current = false;
    try {
      const l = await connectBle(dev.deviceId, dev.name, {
        onState: setState,
        onTelemetry: setTelem,
        onDisconnect: () => {
          link.current = null;
          if (manualClose.current) { setStatus('idle'); return; }
          setStatus('reconnecting');
          retryTimer.current = window.setTimeout(() => open(dev, true), 2500);
        },
      });
      link.current = l;
      setConfig(await l.readConfig());
      setStatus('connected');
      setLastDevice(dev);
      await Preferences.set({ key: LAST_KEY, value: JSON.stringify(dev) });
    } catch (e) {
      link.current = null;
      if (isRetry && !manualClose.current) {
        retryTimer.current = window.setTimeout(() => open(dev, true), 4000);
        return;
      }
      setError(e instanceof Error ? e.message : String(e));
      setStatus('idle');
    }
  }, []);

  const scan = useCallback(async () => {
    try {
      const dev = await pickDevice();
      await open(dev);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
      setStatus('idle');
    }
  }, [open]);

  const demo = useCallback(async () => {
    manualClose.current = false;
    const l = connectDemo({ onState: setState, onTelemetry: setTelem, onDisconnect: () => setStatus('idle') });
    link.current = l;
    setConfig(await l.readConfig());
    setStatus('connected');
  }, []);

  const disconnect = useCallback(async () => {
    manualClose.current = true;
    window.clearTimeout(retryTimer.current);
    const l = link.current;
    link.current = null;
    setStatus('idle');
    setState(null);
    setTelem(null);
    if (l) await l.disconnect().catch(() => undefined);
  }, []);

  const send = useCallback(async (bytes: number[], fast = false) => {
    const l = link.current;
    if (!l) return;
    try { await l.send(bytes, fast); } catch (e) { setError(e instanceof Error ? e.message : String(e)); }
  }, []);

  const reloadConfig = useCallback(async (): Promise<GogenConfig | null> => {
    if (!link.current) return null;
    const c = await link.current.readConfig();
    setConfig(c);
    return c;
  }, []);

  return {
    status, state, telem, config, error, lastDevice, name: link.current?.name,
    scan, open, demo, disconnect, send, reloadConfig, clearError: () => setError(null),
  };
}
