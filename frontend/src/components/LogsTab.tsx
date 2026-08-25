import { useState, useEffect, useCallback } from 'react';
import { useT } from '../i18n';

type PumpLogEntry = {
    t: string;
    pin: string;
    state: 'ON' | 'OFF';
    reason: string;
};

type PumpLogResponse = {
    count: number;
    log: PumpLogEntry[];
};

const PIN_COLORS: Record<string, string> = {
    DRAIN: '#ff6b6b',
    REFILL: '#5c9dff',
    SOLENOID: '#4ec8ff',
    CANISTER: '#ffa040',
    FERT1: '#00E5FF',
    FERT2: '#FF4FD8',
    FERT3: '#FFE45C',
    FERT4: '#FFA726',
    PRIME: '#66E06A',
};

export default function LogsTab({ rtcConnected, rtcLostPower }: { rtcConnected?: boolean, rtcLostPower?: boolean }) {
    const { t } = useT();
    const [logs, setLogs] = useState<PumpLogResponse | null>(null);
    const [loading, setLoading] = useState(false);
    const [error, setError] = useState<string | null>(null);
    const [filter, setFilter] = useState<string>('ALL');

    const fetchLogs = useCallback(() => {
        setLoading(true);
        setError(null);
        fetch('/api/pump/log')
            .then((r) => r.json())
            .then((d: PumpLogResponse) => {
                setLogs(d);
                setLoading(false);
            })
            .catch((e) => {
                setError(e.message || 'Error');
                setLoading(false);
            });
    }, []);

    useEffect(() => {
        fetchLogs();
        // Auto-refresh every 10s
        const iv = setInterval(fetchLogs, 10000);
        return () => clearInterval(iv);
    }, [fetchLogs]);

    const filteredLogs = logs?.log?.filter((e) =>
        filter === 'ALL' ? true : e.pin === filter
    ) ?? [];

    // Reversed: newest first
    const displayLogs = [...filteredLogs].reverse();

    // Unique pins for filter
    const uniquePins = Array.from(new Set(logs?.log?.map((e) => e.pin) ?? []));

    return (
        <div className="flex flex-col gap-3">
            {/* Header + filters */}
            <section className="card">
                <div className="card-h">
                    <h2 className="card-t">{t('logs.title')}</h2>
                    <div className="flex items-center gap-2">
                        <span className="font-mono text-xs tabular-nums text-muted">
                            {logs ? `${logs.count} ${t('logs.events')}` : '...'}
                        </span>
                        <button
                            onClick={fetchLogs}
                            disabled={loading}
                            className="btn btn-xs btn-a"
                        >
                            {loading ? '⟳' : t('logs.refresh')}
                        </button>
                    </div>
                </div>

                {/* Filter pills */}
                <div className="flex flex-wrap gap-1.5">
                    <button
                        onClick={() => setFilter('ALL')}
                        className={`pill min-h-[32px] transition-colors ${filter === 'ALL' ? 'bg-accent/20 text-accent' : 'bg-white/5 text-muted'}`}
                    >
                        {t('logs.all')}
                    </button>
                    {uniquePins.map((pin) => (
                        <button
                            key={pin}
                            onClick={() => setFilter(pin)}
                            className={`pill min-h-[32px] transition-colors ${filter === pin ? 'bg-accent/20' : 'bg-white/5 text-muted'}`}
                            style={filter === pin ? { color: PIN_COLORS[pin] || undefined } : {}}
                        >
                            {pin}
                        </button>
                    ))}
                </div>
            </section>

            {/* Error State */}
            {error && (
                <div className="note note-d text-center text-xs font-bold text-danger">{error}</div>
            )}

            {/* Empty State */}
            {logs && displayLogs.length === 0 && (
                <div className="card py-10 text-center">
                    <span className="mb-3 block text-4xl">📋</span>
                    <span className="text-sm text-muted">{t('logs.empty')}</span>
                </div>
            )}

            {/* Log Entries */}
            {displayLogs.length > 0 && (
                <section className="overflow-hidden rounded-2xl bg-card shadow-md">
                    <div className="divide-y divide-border/40">
                        {displayLogs.map((entry, i) => (
                            <div
                                key={i}
                                className={`px-4 py-2.5 ${entry.state === 'ON' ? 'bg-accent2/5' : ''}`}
                            >
                                <div className="flex items-center gap-2">
                                    <span
                                        className={`h-2 w-2 flex-none rounded-full ${entry.state === 'ON'
                                            ? 'bg-accent2 shadow-[0_0_6px_var(--accent2)]'
                                            : 'bg-muted/40'
                                            }`}
                                    />
                                    <span
                                        className="min-w-0 flex-1 truncate text-xs font-bold tracking-wider"
                                        style={{ color: PIN_COLORS[entry.pin] || 'var(--text)' }}
                                    >
                                        {entry.pin}
                                    </span>
                                    <span className={`pill flex-none ${entry.state === 'ON' ? 'bg-accent2/20 text-accent2' : 'bg-white/5 text-muted'}`}>
                                        {entry.state}
                                    </span>
                                    <span className="flex-none font-mono text-[11px] tabular-nums text-muted">
                                        {entry.t}
                                    </span>
                                </div>
                                <div className="mt-0.5 pl-4 text-[11px] font-medium text-muted/85">
                                    {entry.reason.replace(/_/g, ' ')}
                                </div>
                            </div>
                        ))}
                    </div>
                </section>
            )}

            {/* Info Card */}
            <section className="card bg-card/60">
                <p className="hint leading-relaxed">{t('logs.info')}</p>

                {rtcConnected !== undefined && (
                    <div className="mt-3 flex items-center gap-2 border-t border-border/50 pt-3">
                        <div className={`h-2 w-2 flex-none rounded-full ${rtcConnected ? (rtcLostPower ? 'bg-danger' : 'bg-accent2') : 'bg-muted'}`} />
                        <span className="text-[11px] font-medium text-muted">
                            RTC:
                            <span className={rtcConnected ? (rtcLostPower ? 'ml-1 text-danger' : 'ml-1 text-accent2') : 'ml-1 text-muted'}>
                                {rtcConnected ? (rtcLostPower ? 'Battery Dead / Lost Power' : 'OK') : 'Not Found'}
                            </span>
                        </span>
                    </div>
                )}
            </section>
        </div>
    );
}
