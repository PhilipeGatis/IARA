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
    DRAIN: '#ff4444',
    REFILL: '#4488ff',
    SOLENOID: '#44bbff',
    CANISTER: '#ff8800',
    FERT1: '#00FFFF',
    FERT2: '#FF00FF',
    FERT3: '#FFFF00',
    FERT4: '#FFA500',
    PRIME: '#00FF00',
};

const STATE_BADGE = {
    ON: 'bg-accent2/20 text-accent2',
    OFF: 'bg-white/5 text-muted',
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
        <div className="flex flex-col gap-4">
            {/* Header Card */}
            <div className="rounded-2xl bg-card p-5 shadow-md">
                <div className="flex items-center justify-between mb-4">
                    <h2 className="text-base font-medium tracking-wide text-text/90 uppercase">
                        {t('logs.title')}
                    </h2>
                    <div className="flex items-center gap-2">
                        <span className="text-xs text-muted font-mono">
                            {logs ? `${logs.count} ${t('logs.events')}` : '...'}
                        </span>
                        <button
                            onClick={fetchLogs}
                            disabled={loading}
                            className="rounded-lg bg-accent/10 px-3 py-1.5 text-xs font-bold text-accent transition-all hover:bg-accent/20 active:scale-95 disabled:opacity-50"
                        >
                            {loading ? '⟳' : t('logs.refresh')}
                        </button>
                    </div>
                </div>

                {/* Filter Pills */}
                <div className="flex flex-wrap gap-1.5 mb-1">
                    <button
                        onClick={() => setFilter('ALL')}
                        className={`rounded-full px-3 py-1 text-[10px] font-bold tracking-wider transition-all ${
                            filter === 'ALL'
                                ? 'bg-accent/20 text-accent'
                                : 'bg-white/5 text-muted hover:bg-white/10'
                        }`}
                    >
                        {t('logs.all')}
                    </button>
                    {uniquePins.map((pin) => (
                        <button
                            key={pin}
                            onClick={() => setFilter(pin)}
                            className={`rounded-full px-3 py-1 text-[10px] font-bold tracking-wider transition-all ${
                                filter === pin
                                    ? 'bg-accent/20 text-accent'
                                    : 'bg-white/5 text-muted hover:bg-white/10'
                            }`}
                            style={filter === pin ? { color: PIN_COLORS[pin] || undefined } : {}}
                        >
                            {pin}
                        </button>
                    ))}
                </div>
            </div>

            {/* Error State */}
            {error && (
                <div className="rounded-2xl bg-danger/10 border border-danger/30 p-4 text-center">
                    <span className="text-xs font-bold text-danger">{error}</span>
                </div>
            )}

            {/* Empty State */}
            {logs && displayLogs.length === 0 && (
                <div className="rounded-2xl bg-card p-8 shadow-md text-center">
                    <span className="text-4xl block mb-3">📋</span>
                    <span className="text-sm text-muted">{t('logs.empty')}</span>
                </div>
            )}

            {/* Log Entries */}
            {displayLogs.length > 0 && (
                <div className="rounded-2xl bg-card shadow-md overflow-hidden">
                    <div className="divide-y divide-border/30">
                        {displayLogs.map((entry, i) => (
                            <div
                                key={i}
                                className={`flex items-center gap-3 px-4 py-3 transition-colors ${
                                    entry.state === 'ON' ? 'bg-accent2/5' : ''
                                }`}
                            >
                                {/* State indicator dot */}
                                <div
                                    className={`h-2 w-2 flex-none rounded-full ${
                                        entry.state === 'ON'
                                            ? 'bg-accent2 shadow-[0_0_6px_var(--accent2)]'
                                            : 'bg-muted/30'
                                    }`}
                                />

                                {/* Pin name */}
                                <span
                                    className="text-xs font-bold tracking-wider w-[70px] flex-none"
                                    style={{ color: PIN_COLORS[entry.pin] || 'var(--text)' }}
                                >
                                    {entry.pin}
                                </span>

                                {/* State badge */}
                                <span
                                    className={`rounded-full px-2 py-0.5 text-[10px] font-bold tracking-wider flex-none ${
                                        STATE_BADGE[entry.state]
                                    }`}
                                >
                                    {entry.state}
                                </span>

                                {/* Reason */}
                                <span className="text-[10px] text-muted font-medium truncate flex-1 min-w-0">
                                    {entry.reason.replace(/_/g, ' ')}
                                </span>

                                {/* Timestamp */}
                                <span className="text-[10px] font-mono text-muted/70 flex-none">
                                    {entry.t}
                                </span>
                            </div>
                        ))}
                    </div>
                </div>
            )}

            {/* Info Card */}
            <div className="rounded-2xl bg-card/50 p-4 shadow-md">
                <p className="text-[10px] text-muted/60 leading-relaxed mb-3">
                    {t('logs.info')}
                </p>
                
                {rtcConnected !== undefined && (
                    <div className="flex items-center gap-2 border-t border-border/30 pt-3">
                        <div className={`w-2 h-2 rounded-full ${rtcConnected ? (rtcLostPower ? 'bg-danger' : 'bg-accent2') : 'bg-muted'}`} />
                        <span className="text-[10px] font-medium text-muted">
                            RTC Status: 
                            <span className={rtcConnected ? (rtcLostPower ? 'text-danger ml-1' : 'text-accent2 ml-1') : 'text-muted ml-1'}>
                                {rtcConnected ? (rtcLostPower ? 'Battery Dead / Lost Power' : 'OK') : 'Not Found'}
                            </span>
                        </span>
                    </div>
                )}
            </div>
        </div>
    );
}
