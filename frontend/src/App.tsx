import { useEffect, useState } from 'react';
import HomeTab from './components/HomeTab';
import TPATab from './components/TPATab';
import FertsTab from './components/FertsTab';
import ConfigTab from './components/ConfigTab';
import { I18nProvider, useT } from './i18n';

export type AQStatus = {
  wifiConnected: boolean;
  time: string;
  waterLevel: number;
  optical: boolean;
  float: boolean;
  emergency: boolean;
  maintenance: boolean;
  tpaState: string;
  canister: boolean;
  tpaInterval: number;
  tpaHour: number;
  tpaMinute: number;
  tpaLastRun: number;
  tpaPercent: number;
  tpaConfigReady: boolean;
  primeMl: number;
  aqHeight: number;
  aqLength: number;
  aqWidth: number;
  aqMarginCm: number;
  aquariumVolume: number;
  litersPerCm: number;
  drainFlowRate: number;
  refillFlowRate: number;
  primeRatio: number;
  reservoirVolume: number;
  reservoirSafetyML: number;
  language: number;
  stocks: Array<{
    name: string;
    stock: number;
    doses: number[];
    sH: number;
    sM: number;
    fR: number;
    pwm: number;
  }>;
};

export const api = (method: string, url: string, body?: any) => {
  fetch(url, {
    method,
    headers: { 'Content-Type': 'application/json' },
    body: body ? JSON.stringify(body) : undefined,
  })
    .then((r) => r.json())
    .then((d) => {
      if (d.error) alert(d.error);
    })
    .catch((e) => console.error(e));
};

function AppContent() {
  const { t } = useT();
  const [tab, setTab] = useState<'home' | 'tpa' | 'ferts' | 'config'>('home');
  const [status, setStatus] = useState<AQStatus | null>(null);
  const [wifiDot, setWifiDot] = useState(false);

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
    <div className="min-h-screen pb-20">
      {/* Header */}
      <div className="sticky top-0 z-10 flex flex-wrap items-center justify-between gap-2 bg-card shadow-md px-4 py-4">
        <div className="flex min-w-0 flex-1 items-center gap-3">
          <span
            className={`h-2.5 w-2.5 flex-none rounded-full ${wifiDot ? 'bg-accent2 shadow-[0_0_8px_var(--accent2)]' : 'bg-danger shadow-[0_0_8px_var(--danger)]'
              }`}
          />
          <h1 className="truncate text-base font-medium tracking-wide sm:text-lg">Aquarium Controller</h1>
        </div>
        <span className="flex-none font-mono text-sm tracking-widest text-accent sm:text-base">
          {status?.time || '--:--:--'}
        </span>
      </div>

      {/* Emergency Banner */}
      {status?.emergency && (
        <div className="mx-auto my-3 w-[calc(100%-24px)] max-w-[800px] animate-pulse rounded-xl border-2 border-danger bg-red-500/10 p-4 text-center">
          <strong>{t('emergency.banner')}</strong>
        </div>
      )}

      {/* Tabs Content */}
      <div className="mx-auto grid max-w-[800px] grid-cols-1 gap-3 p-3 sm:grid-cols-2">
        {tab === 'home' && <HomeTab status={status} />}
        {tab === 'tpa' && <TPATab status={status} />}
        {tab === 'ferts' && <FertsTab status={status} />}
        {tab === 'config' && <ConfigTab status={status} />}
      </div>

      {/* Bottom Navigation */}
      <nav className="tabs fixed bottom-0 left-0 right-0 z-20 flex justify-around bg-card p-2 pb-6 shadow-[0_-2px_10px_rgba(0,0,0,0.5)]">
        {[
          { id: 'home', icon: '🏠', label: t('nav.home') },
          { id: 'tpa', icon: '💧', label: t('nav.tpa') },
          { id: 'ferts', icon: '🧪', label: t('nav.ferts') },
          { id: 'config', icon: '⚙️', label: t('nav.config') },
        ].map((tb) => (
          <button
            key={tb.id}
            onClick={() => setTab(tb.id as any)}
            className={`flex flex-1 flex-col items-center gap-1 rounded-xl p-2 transition-all active:scale-95 ${tab === tb.id ? 'text-accent bg-accent/10' : 'text-muted hover:bg-white/5 hover:text-text'
              }`}
          >
            <span className="text-2xl drop-shadow-sm">{tb.icon}</span>
            <span className="text-xs font-medium tracking-wide">{tb.label}</span>
          </button>
        ))}
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
