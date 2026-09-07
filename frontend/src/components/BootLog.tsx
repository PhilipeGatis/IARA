import { useState, useEffect, useCallback } from 'react';
import { useT } from '../i18n';
import { dateFromDeviceEpoch } from '../api';

type BootEntry = {
    seq: number;
    reason: string;
    /** 0 until the clock synced, so the boot has no wall-clock time. */
    epoch: number;
    /** null when the uptime counter did not survive the reset. */
    prevUptimeS: number | null;
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
