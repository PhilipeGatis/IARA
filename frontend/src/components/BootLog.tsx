import { useState, useEffect, useCallback } from 'react';
import { useT } from '../i18n';
import { dateFromDeviceEpoch } from '../api';

/** What the board was doing when the run ended, carried across the reset. */
type BootSnap = {
    /** 0 unknown, 1 connected, 2 down, 3 AP fallback. */
    wifi: number;
    /** null when the station was not associated. */
    rssi: number | null;
    retries: number;
    reason: number;
    /** 65535 = the station never dropped during that run. */
    quietS: number;
    drops: number;
    sse: number;
    outputs: number;
    heapKb: number;
};

type BootEntry = {
    seq: number;
    reason: string;
    /** 0 until the clock synced, so the boot has no wall-clock time. */
    epoch: number;
    /** null when the uptime counter did not survive the reset. */
    prevUptimeS: number | null;
    /** Absent on entries written before the snapshot existed. */
    snap?: BootSnap;
};

type BootLogResponse = {
    count: number;
    boots: BootEntry[];
};

/**
 * Resets nobody needs to explain: the board was switched on, told to reboot,
 * or had its reset button pressed. Everything else is a fault, and the whole
 * point of this card is that the difference is visible at a glance.
 */
const NORMAL_REASONS = new Set(['POWER_ON', 'SOFTWARE', 'EXTERNAL', 'DEEP_SLEEP']);

const WHY_KEYS = {
    BROWNOUT: 'boot.why.BROWNOUT',
    PANIC_EXCEPTION: 'boot.why.PANIC_EXCEPTION',
    INTERRUPT_WATCHDOG: 'boot.why.INTERRUPT_WATCHDOG',
    TASK_WATCHDOG: 'boot.why.TASK_WATCHDOG',
    OTHER_WATCHDOG: 'boot.why.OTHER_WATCHDOG',
} as const;

const WIFI_STATE_KEYS = [
    'boot.snap.wifiUnknown',
    'boot.snap.wifiOk',
    'boot.snap.wifiDown',
    'boot.snap.wifiAp',
] as const;

/** The station never dropped during that run — not an age of 65535 seconds. */
const NEVER_DROPPED = 65535;

/** Seconds to something readable at every scale a reset can happen at. */
function formatUptime(s: number): string {
    if (s < 60) return `${s}s`;
    if (s < 3600) return `${Math.floor(s / 60)}m ${s % 60}s`;
    if (s < 86400) return `${Math.floor(s / 3600)}h ${Math.floor((s % 3600) / 60)}m`;
    return `${Math.floor(s / 86400)}d ${Math.floor((s % 86400) / 3600)}h`;
}

export default function BootLog() {
    const { t, lang } = useT();
    const dateLocale = lang === 'ja' ? 'ja-JP' : lang === 'en' ? 'en-US' : 'pt-BR';

    const [data, setData] = useState<BootLogResponse | null>(null);
    const [loading, setLoading] = useState(false);
    const [error, setError] = useState<string | null>(null);
    const [expanded, setExpanded] = useState(false);

    const fetchBoots = useCallback(() => {
        setLoading(true);
        setError(null);
        fetch('/api/boot/log')
            .then((r) => r.json())
            .then((d: BootLogResponse) => {
                setData(d);
                setLoading(false);
            })
            .catch((e) => {
                setError(e.message || 'Error');
                setLoading(false);
            });
    }, []);

    // No polling here, unlike the pump log: a new entry only ever appears by
    // way of a reset, which takes the page down with it.
    useEffect(() => {
        fetchBoots();
    }, [fetchBoots]);

    /**
     * One line saying what the radio and the actuators were doing.
     *
     * Only rendered for faults. On a boot the user caused this is noise; on a
     * brownout it is the entire reason the snapshot exists.
     */
    const describeSnap = (s: BootSnap): string | null => {
        // An all-zero snapshot is a pre-upgrade entry, or a run that ended
        // before the first snapshot landed. Say nothing rather than report a
        // dead radio and no heap.
        if (s.wifi === 0 && s.rssi === null && s.heapKb === 0) return null;

        const parts: string[] = [t(WIFI_STATE_KEYS[s.wifi] ?? WIFI_STATE_KEYS[0])];
        if (s.rssi !== null) parts.push(`${s.rssi} dBm`);
        if (s.drops > 0) parts.push(t('boot.snap.drops', { n: s.drops }));
        if (s.retries > 0) parts.push(t('boot.snap.retries', { n: s.retries }));
        if (s.drops > 0 && s.quietS !== NEVER_DROPPED) {
            parts.push(t('boot.snap.lastDrop', { d: formatUptime(s.quietS) }));
        }
        if (s.sse > 0) parts.push(t('boot.snap.panels', { n: s.sse }));
        parts.push(
            s.outputs === 0
                ? t('boot.snap.noOutputs')
                : t('boot.snap.outputs', { mask: s.outputs.toString(16) })
        );
        return parts.join(' · ');
    };

    const boots = data?.boots ?? [];
    const abnormal = boots.filter((b) => !NORMAL_REASONS.has(b.reason)).length;

    // Collapsed by default: on a healthy board this is 32 lines of POWER_ON
    // sitting above the log people actually open this tab for.
    const visible = expanded ? boots : boots.slice(0, 5);

    return (
        <section className="card">
            <div className="card-h">
                <h2 className="card-t">{t('boot.title')}</h2>
                <div className="flex items-center gap-2">
                    <span className="font-mono text-xs tabular-nums text-muted">
                        {data ? `${data.count} ${t('boot.boots')}` : '...'}
                    </span>
                    <button
                        onClick={fetchBoots}
                        disabled={loading}
                        className="btn btn-xs btn-a"
                    >
                        {loading ? '⟳' : t('logs.refresh')}
                    </button>
                </div>
            </div>

            {error && (
                <div className="note note-d text-center text-xs font-bold text-danger">{error}</div>
            )}

            {data && boots.length === 0 && (
                <p className="py-4 text-center text-sm text-muted">{t('boot.empty')}</p>
            )}

            {boots.length > 0 && (
                <>
                    <div
                        className={`note ${abnormal > 0 ? 'note-d text-danger' : ''} mb-2 text-xs font-bold`}
                    >
                        {abnormal > 0
                            ? t('boot.abnormal', { n: abnormal, total: boots.length })
                            : t('boot.allNormal')}
                    </div>

                    <div className="divide-y divide-border/40">
                        {visible.map((b) => {
                            const fault = !NORMAL_REASONS.has(b.reason);
                            const whyKey = WHY_KEYS[b.reason as keyof typeof WHY_KEYS];
                            return (
                                <div key={b.seq} className={`py-2 ${fault ? 'bg-danger/5' : ''}`}>
                                    <div className="flex items-center gap-2">
                                        <span
                                            className={`h-2 w-2 flex-none rounded-full ${fault ? 'bg-danger shadow-[0_0_6px_var(--danger)]' : 'bg-muted/40'}`}
                                        />
                                        <span
                                            className={`min-w-0 flex-1 truncate text-xs font-bold tracking-wider ${fault ? 'text-danger' : 'text-muted'}`}
                                        >
                                            {b.reason.replace(/_/g, ' ')}
                                        </span>
                                        <span className="flex-none font-mono text-[11px] tabular-nums text-muted">
                                            {b.epoch > 0
                                                ? dateFromDeviceEpoch(b.epoch).toLocaleString(dateLocale, {
                                                      day: '2-digit',
                                                      month: '2-digit',
                                                      hour: '2-digit',
                                                      minute: '2-digit',
                                                  })
                                                : t('boot.noClock')}
                                        </span>
                                    </div>
                                    <div className="mt-0.5 pl-4 text-[11px] font-medium text-muted/85">
                                        #{b.seq}
                                        {' · '}
                                        {b.prevUptimeS === null
                                            ? t('boot.unknownRun')
                                            : t('boot.ranFor', { d: formatUptime(b.prevUptimeS) })}
                                        {whyKey && ` · ${t(whyKey)}`}
                                    </div>
                                    {fault && b.snap && describeSnap(b.snap) && (
                                        <div className="mt-0.5 pl-4 font-mono text-[10px] tabular-nums text-muted/70">
                                            {describeSnap(b.snap)}
                                        </div>
                                    )}
                                </div>
                            );
                        })}
                    </div>

                    {boots.length > 5 && (
                        <button
                            onClick={() => setExpanded((v) => !v)}
                            className="btn btn-xs btn-a mt-2 w-full"
                        >
                            {expanded ? '▲' : `▼ ${boots.length - 5}`}
                        </button>
                    )}
                </>
            )}

            <p className="hint mt-3 leading-relaxed">{t('boot.info')}</p>
        </section>
    );
}
