import { useEffect, useState } from 'react';
import HomeTab from './components/HomeTab';
import TPATab from './components/TPATab';
import FertsTab from './components/FertsTab';
import ConfigTab from './components/ConfigTab';
import LogsTab from './components/LogsTab';
import { I18nProvider, useT } from './i18n';
import { setNetErrorMsg } from './api';

declare const __APP_VERSION__: string;

export type AQStatus = {
  wifiConnected: boolean;
  time: string;
  rtcConnected?: boolean;
  rtcLostPower?: boolean;
  waterLevel: number;
  float: boolean;
  emergency: boolean;
  maintenance: boolean;
  tpaState: string;
  canister: boolean;
  tpaInterval: number;
  tpaAutoEnabled: boolean;
  tpaHour: number;
  tpaMinute: number;
  tpaLastRun: number;
  tpaPercent: number;
  tpaConfigReady: boolean;
  primeML: number;
  primeEnabled: boolean;
  aqHeight: number;
  aqMarginMm: number;
  aqLength: number;
  aqWidth: number;
  sensorFullDistanceMm: number;
  aquariumVolume: number;
  litersPerCm: number;
  canisterSafePct: number;
  drainFlowRate: number;
  refillFlowRate: number;
  primeRatio: number;
  reservoirVolume: number;
  reservoirSafetyML: number;
  pumpGoalLiters?: number;
  pumpProgressLiters?: number;
  pumpElapsedMs?: number;
  language: number;
  firmwareVersion?: string;
  resetReason?: string;
  uptimeMs?: number;
  stocks: {
    name: string;
    stock: number;
    doses: number[];
    sH: number[];
    sM: number[];
    fR: number;
    pwm: number;
    en?: boolean;
  }[];
};

/** Compact uptime for the header line: 3d 4h, 4h 12m, or 12m. */
const formatUptime = (ms: number) => {
  const totalMin = Math.floor(ms / 60000);
  const d = Math.floor(totalMin / 1440);
  const h = Math.floor((totalMin % 1440) / 60);
  const m = totalMin % 60;
  if (d > 0) return `${d}d ${h}h`;
  if (h > 0) return `${h}h ${m}m`;
  return `${m}m`;
};

const TABS = [
  { id: 'home', icon: '🏠', key: 'nav.home' },
  { id: 'tpa', icon: '💧', key: 'nav.tpa' },
  { id: 'ferts', icon: '🧪', key: 'nav.ferts' },
  { id: 'logs', icon: '📋', key: 'nav.logs' },
  { id: 'config', icon: '⚙️', key: 'nav.config' },
] as const;

function AppContent() {
  const { t } = useT();
  const [tab, setTab] = useState<'home' | 'tpa' | 'ferts' | 'logs' | 'config'>('home');
  const [status, setStatus] = useState<AQStatus | null>(null);
  const [wifiDot, setWifiDot] = useState(false);

  useEffect(() => {
    setNetErrorMsg(t('net.error'));
  }, [t]);

  useEffect(() => {
    // Initial fetch
    fetch('/api/status')
      .then((r) => r.json())
      .then((data) => {
        setStatus(data);
        setWifiDot(true);
        if (data.wifiConnected !== undefined) {
          document.body.className = data.wifiConnected ? '' : 'ap-mode';
          if (!data.wifiConnected) setTab('config');
        }
      })
      .catch(() => setWifiDot(false));

    // SSE Real-time Updates
    const evtSource = new EventSource('/events');
    evtSource.addEventListener('status', (e) => {
      try {
        const d = JSON.parse(e.data);
        setStatus(d);
        setWifiDot(true);
        if (d.wifiConnected !== undefined) {
          document.body.className = d.wifiConnected ? '' : 'ap-mode';
        }
      } catch (err) {
        console.error(err);
      }
    });

    evtSource.onerror = () => setWifiDot(false);

    return () => evtSource.close();
  }, []);

  return (
    <div className="min-h-screen pb-nav">
      {/* Header */}
      <header className="sticky top-0 z-20 border-b border-border/60 bg-card/95 shadow-md backdrop-blur">
        <div className="mx-auto flex w-full max-w-[640px] items-center gap-3 px-4 py-2.5">
          <span
            className={`h-2.5 w-2.5 flex-none rounded-full ${wifiDot ? 'bg-accent2 shadow-[0_0_8px_var(--accent2)]' : 'bg-danger shadow-[0_0_8px_var(--danger)]'
              }`}
            title={wifiDot ? 'online' : 'offline'}
          />
          <div className="min-w-0 flex-1">
            <h1 className="truncate text-[15px] font-medium leading-tight tracking-wide">IARA</h1>
            <p className="truncate font-mono text-[10px] leading-tight text-muted/80">
              FW {status?.firmwareVersion || '?'} · UI {__APP_VERSION__}
              {status?.resetReason ? ` · ${status.resetReason}` : ''}
              {status?.uptimeMs ? ` · ${formatUptime(status.uptimeMs)}` : ''}
            </p>
          </div>
          <span className="flex-none font-mono text-base font-medium tabular-nums tracking-wider text-accent">
            {status?.time || '--:--:--'}
          </span>
        </div>
      </header>

      {/* Global alerts — visible from every tab */}
      {!wifiDot && (
        <div className="mx-auto w-full max-w-[640px] px-4 pt-3">
          <div className="note bg-danger/15 text-danger">{t('net.stale')}</div>
        </div>
      )}

      {(status?.emergency || status?.maintenance) && (
        <div className="mx-auto flex w-full max-w-[640px] flex-col gap-2 px-3 pt-3">
          {status?.emergency && (
            <div className="note note-d animate-pulse text-center text-sm font-bold text-danger">
              {t('emergency.banner')}
            </div>
          )}
          {status?.maintenance && (
            <div className="note note-w text-center text-sm font-bold text-warn">
              {t('maintenance.banner')}
            </div>
          )}
        </div>
      )}

      {/* Tab content — single column, phone first */}
      <main className="mx-auto w-full max-w-[640px] p-3">
        {tab === 'home' && <HomeTab status={status} />}
        {tab === 'tpa' && <TPATab status={status} />}
        {tab === 'ferts' && <FertsTab status={status} />}
        {tab === 'logs' && <LogsTab rtcConnected={status?.rtcConnected} rtcLostPower={status?.rtcLostPower} />}
        {tab === 'config' && <ConfigTab status={status} />}
      </main>

      {/* Bottom Navigation */}
      <nav className="tabs fixed bottom-0 left-0 right-0 z-30 border-t border-border/60 bg-card/95 pb-[var(--safe-b)] shadow-[0_-2px_10px_rgba(0,0,0,0.5)] backdrop-blur">
        <div className="mx-auto flex w-full max-w-[640px] justify-around px-1">
          {TABS.map((tb) => {
            const active = tab === tb.id;
            return (
              <button
                key={tb.id}
                onClick={() => setTab(tb.id)}
                aria-current={active ? 'page' : undefined}
                className={`relative flex min-h-[56px] flex-1 flex-col items-center justify-center gap-0.5 rounded-lg transition-colors active:bg-white/10 ${active ? 'text-accent' : 'text-muted'
                  }`}
              >
                <span className={`absolute inset-x-3 top-0 h-0.5 rounded-full ${active ? 'bg-accent' : 'bg-transparent'}`} />
                <span className="text-xl leading-none">{tb.icon}</span>
                <span className="text-[11px] font-medium leading-none tracking-wide">{t(tb.key)}</span>
              </button>
            );
          })}
        </div>
      </nav>
    </div>
  );
}

function App() {
  const [initialLang, setInitialLang] = useState<number | undefined>(undefined);
  const [ready, setReady] = useState(false);

  useEffect(() => {
    fetch('/api/status')
      .then((r) => r.json())
      .then((d) => {
        setInitialLang(d.language ?? 0);
        setReady(true);
      })
      .catch(() => {
        setInitialLang(0);
        setReady(true);
      });
  }, []);

  if (!ready) return null;

  return (
    <I18nProvider initialLang={initialLang}>
      <AppContent />
    </I18nProvider>
  );
}

export default App;
