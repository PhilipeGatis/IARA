import { useState, useEffect } from 'react';
import { type AQStatus } from '../App';
import { api } from '../api';
import { useT } from '../i18n';
import { useConfirm } from '../Confirm';
import FertConfigModal from './FertConfigModal';

/**
 * The day the bottle cannot cover its next dose.
 *
 * Walks the weekly schedule forward one day at a time instead of dividing the
 * stock by a weekly average: a channel that doses 5 mL on Saturdays alone runs
 * out on a Saturday, and an average would name a Tuesday. Doses already past
 * their hour today are skipped, because the stock reported already reflects
 * them.
 *
 * Null when nothing is scheduled or the channel is off — there is no date to
 * give, and printing one from a stale schedule is worse than printing none.
 */
export function runOutDate(s: AQStatus['stocks'][0], from = new Date()): Date | null {
    if (s.en === false) return null;
    const doses = s.doses ?? [];
    if (!doses.some(d => d > 0)) return null;

    let stock = s.stock;
    const day = new Date(from);
    // Ten years. A channel dosing a tenth of a millilitre a week outlives any
    // answer worth printing, and the loop has to end somewhere.
    for (let i = 0; i < 3660; i++) {
        const dow = day.getDay();
        const dose = doses[dow] ?? 0;
        if (dose > 0) {
            const due = new Date(day);
            due.setHours(s.sH?.[dow] ?? 0, s.sM?.[dow] ?? 0, 0, 0);
            if (due >= from) {
                if (stock < dose) return due;
                stock -= dose;
            }
        }
        day.setDate(day.getDate() + 1);
    }
    return null;
}

/* ── Compact summary card ──────────────────────────────────────── */
function FertCardCompact({
    index,
    s,
    onConfig,
}: {
    index: number;
    s: AQStatus['stocks'][0];
    onConfig: () => void;
}) {
    const { t, lang } = useT();
    const dateLocale = lang === 'ja' ? 'ja-JP' : lang === 'en' ? 'en-US' : 'pt-BR';
    const [name, setName] = useState(s.name || '');
    const [showRefill, setShowRefill] = useState(false);
    const [resetVol, setResetVol] = useState('');
    const shortDays = t('fert.shortDays').split(',');

    useEffect(() => {
        if (!name && s.name) setName(s.name);
    }, [s.name]);

    // Older firmware does not report a bottle size; 500 is what it assumed.
    const cap = s.cap && s.cap > 0 ? s.cap : 500;
    const pct = Math.min(100, (s.stock / cap) * 100);
    const runOut = runOutDate(s);
    const daysLeft = runOut
        ? Math.max(0, Math.round((runOut.getTime() - Date.now()) / 86400000))
        : null;
    const activeDays = s.doses
        ? s.doses.map((dose, i) => ({ dose, i })).filter(d => d.dose > 0)
        : [];
    const totalWeek = activeDays.reduce((a, d) => a + d.dose, 0);

    const commitRefill = () => {
        if (resetVol) {
            api('POST', '/api/stock/reset', { channel: index, ml: +resetVol });
            setResetVol('');
            setShowRefill(false);
        }
    };

    return (
        <section className="overflow-hidden rounded-2xl bg-card shadow-md">
            {/* Header: channel tag + name + stock */}
            <div className="flex items-center gap-2 px-4 pt-4">
                <span className="pill flex-none bg-accent text-[color:var(--on-accent)]">CH{index + 1}</span>
                <input
                    type="text"
                    placeholder={t('fert.namePlaceholder')}
                    value={name}
                    onChange={(e) => setName(e.target.value)}
                    onBlur={() => name ? api('POST', '/api/fert/name', { channel: index, name }) : null}
                    className="h-9 min-w-0 flex-1 truncate border-b border-transparent bg-transparent text-sm font-semibold text-text outline-none transition-colors focus:border-accent"
                />
                <span className="flex-none items-baseline whitespace-nowrap">
                    <span className="text-xl font-bold tabular-nums text-text">{s.stock.toFixed(0)}</span>
                    <span className="ml-0.5 text-[11px] font-medium text-muted">/{cap.toFixed(0)} mL</span>
                </span>
            </div>

            {/* Stock bar */}
            <div className="px-4 pb-3 pt-2">
                <div className="h-1.5 w-full overflow-hidden rounded-full bg-white/10">
                    <div
                        className={`h-full transition-all duration-500 ease-out ${pct < 10 ? 'bg-danger' : pct < 20 ? 'bg-warn' : 'bg-accent2'}`}
                        style={{ width: `${pct}%` }}
                    />
                </div>
                {runOut && (
                    <p className="mt-1.5 text-[11px] text-muted">
                        {t('fert.runOut', {
                            d: runOut.toLocaleDateString(dateLocale, { day: '2-digit', month: '2-digit', year: 'numeric' }),
                            n: String(daysLeft),
                        })}
                    </p>
                )}
            </div>

            {/* Schedule mini-table */}
            {activeDays.length > 0 && (
                <div className="px-4 pb-2">
                    <table className="w-full text-xs">
                        <tbody>
                            {activeDays.map(({ dose, i }) => {
                                const h = Array.isArray(s.sH) ? String(s.sH[i] ?? 0).padStart(2, '0') : '00';
                                const m = Array.isArray(s.sM) ? String(s.sM[i] ?? 0).padStart(2, '0') : '00';
                                return (
                                    <tr key={i} className="border-b border-border/40 last:border-0">
                                        <td className="w-10 py-1.5 pr-2 font-bold text-accent">{shortDays[i]}</td>
                                        <td className="py-1.5 font-mono tabular-nums text-muted">{h}:{m}</td>
                                        <td className="py-1.5 text-right font-bold tabular-nums text-accent2">
                                            {dose}<span className="font-normal text-muted">mL</span>
                                        </td>
                                    </tr>
                                );
                            })}
                        </tbody>
                        <tfoot>
                            <tr className="border-t border-border/60">
                                <td colSpan={2} className="py-1.5 text-[10px] font-bold uppercase tracking-wider text-muted">{t('fert.totalWeek')}</td>
                                <td className="py-1.5 text-right font-bold tabular-nums text-text">
                                    {totalWeek.toFixed(1)}<span className="font-normal text-muted">mL</span>
                                </td>
                            </tr>
                        </tfoot>
                    </table>
                </div>
            )}

            {/* Refill popover */}
            {showRefill && (
                <div className="px-4 pb-3">
                    <div className="flex items-center gap-2 rounded-xl border border-accent2/30 bg-accent2/5 p-2">
                        <input
                            type="number" min="0" max="2000" placeholder={t('fert.newVolume')} autoFocus
                            className="inp remove-arrow h-10 flex-1 text-center"
                            value={resetVol} onChange={(e) => setResetVol(e.target.value)}
                            onKeyDown={(e) => { if (e.key === 'Enter') commitRefill(); }}
                        />
                        <button onClick={commitRefill} className="btn btn-xs btn-a2 flex-none">OK</button>
                        <button
                            onClick={() => setShowRefill(false)}
                            className="btn btn-xs flex-none text-muted hover:text-text"
                            aria-label="close"
                        >
                            ✕
                        </button>
                    </div>
                </div>
            )}

            {/* Footer buttons */}
            <div className="flex border-t border-border/50">
                <button
                    onClick={() => {
                        // A refill is almost always a whole bottle, so offer that
                        // number rather than an empty box.
                        if (!showRefill) setResetVol(String(cap));
                        setShowRefill(!showRefill);
                    }}
                    className="flex min-h-[48px] flex-1 items-center justify-center gap-1.5 border-r border-border/50 text-[11px] font-bold uppercase tracking-wider text-accent2 transition active:bg-white/10"
                >
                    <span>📦</span> {t('fert.refill')}
                </button>
                <button
                    onClick={onConfig}
                    className="flex min-h-[48px] flex-1 items-center justify-center gap-1.5 text-[11px] font-bold uppercase tracking-wider text-muted transition hover:text-accent active:bg-white/10"
                >
                    <span>⚙️</span> {t('fert.configure')}
                </button>
            </div>
        </section>
    );
}

/* ── Exported FertCard (for reuse in other tabs if needed) ────── */
export { FertCardCompact as FertCard };

/* ── Main FertsTab ─────────────────────────────────────────────── */
export default function FertsTab({ status }: { status: AQStatus | null }) {
    const { t } = useT();
    const { ask, dialog } = useConfirm();
    const [configChannel, setConfigChannel] = useState<number | null>(null);
    const [doseMsg, setDoseMsg] = useState<string | null>(null);

    // Fires today's schedule ahead of its hour. The firmware decides what is
    // owing — a channel that already dosed today is not in it — so the answer
    // worth showing is how many channels it actually took.
    const doseNow = async () => {
        if (!(await ask(t('confirm.fertDoseNow')))) return;
        const r = await api('POST', '/api/fert/dose-now');
        if (!r) return; // api() already reported the network failure
        setDoseMsg(
            r.queued > 0
                ? t('fert.doseNowQueued', { n: r.queued })
                : t('fert.doseNowNone'),
        );
    };

    if (!status?.stocks) return <div className="card text-center text-sm text-muted">{t('fert.loading')}</div>;

    // Filter out channel 4 (Prime) ONLY if primeEnabled is true
    const channels = status.stocks
        .map((s, i) => ({ s, i }))
        .filter(c => c.i !== 4 || !status?.primeEnabled);

    return (
        <>
            {dialog}
            <div className="flex flex-col gap-3">
                <section className="card">
                    <button onClick={doseNow} className="btn btn-p w-full">
                        {t('fert.doseNow')}
                    </button>
                    <span className="hint mt-2 block">{t('fert.doseNowHint')}</span>
                    {doseMsg && (
                        <p className="mt-2 rounded-lg bg-accent2/10 px-3 py-2 text-xs text-accent2">
                            {doseMsg}
                        </p>
                    )}
                </section>

                {channels.map(({ s, i }) => (
                    <FertCardCompact
                        key={i}
                        index={i}
                        s={s}
                        onConfig={() => setConfigChannel(i)}
                    />
                ))}
            </div>

            {/* Config Modal */}
            {configChannel !== null && (
                <FertConfigModal
                    index={configChannel}
                    s={status.stocks[configChannel]}
                    time={status.time}
                    onClose={() => setConfigChannel(null)}
                />
            )}
        </>
    );
}
