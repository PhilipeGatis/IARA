import { useState, useEffect } from 'react';
import { api, type AQStatus } from '../App';
import { useT } from '../i18n';
import FertConfigModal from './FertConfigModal';

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
    const { t } = useT();
    const [name, setName] = useState(s.name || '');
    const [showRefill, setShowRefill] = useState(false);
    const [resetVol, setResetVol] = useState('');
    const shortDays = t('fert.shortDays').split(',');

    useEffect(() => {
        if (!name && s.name) setName(s.name);
    }, [s.name]);

    const pct = Math.min(100, (s.stock / 500) * 100);
    const activeDays = s.doses
        ? s.doses.map((dose, i) => ({ dose, i })).filter(d => d.dose > 0)
        : [];
    const totalWeek = activeDays.reduce((a, d) => a + d.dose, 0);

    return (
        <div className="rounded-2xl bg-card shadow-md overflow-hidden">
            {/* Header: channel tag + name + stock */}
            <div className="px-4 pt-4 pb-1 flex items-center justify-between">
                <div className="flex items-center gap-2 min-w-0">
                    <span className="text-[10px] font-bold text-black bg-accent/80 rounded px-1.5 py-0.5 uppercase tracking-wider flex-none">CH{index + 1}</span>
                    <input
                        type="text"
                        placeholder={t('fert.namePlaceholder')}
                        value={name}
                        onChange={(e) => setName(e.target.value)}
                        onBlur={() => name ? api('POST', '/api/fert/name', { channel: index, name }) : null}
                        className="min-w-0 flex-1 bg-transparent text-sm font-semibold text-white outline-none border-b border-transparent focus:border-accent transition-colors truncate"
                    />
                </div>
                <div className="flex items-baseline gap-0.5 flex-none ml-2">
                    <span className="text-xl font-bold text-white tabular-nums">{s.stock.toFixed(0)}</span>
                    <span className="text-[10px] text-muted font-medium">mL</span>
                </div>
            </div>

            {/* Stock bar */}
            <div className="px-4 pb-3">
                <div className="h-1 w-full overflow-hidden rounded-full bg-white/5">
                    <div
                        className={`h-full transition-all duration-500 ease-out ${pct < 10 ? 'bg-danger' : pct < 20 ? 'bg-warn' : 'bg-accent2'}`}
                        style={{ width: `${pct}%` }}
                    />
                </div>
            </div>

            {/* Schedule mini-table */}
            {activeDays.length > 0 && (
                <div className="px-4 pb-2">
                    <table className="w-full text-[10px]">
                        <tbody>
                            {activeDays.map(({ dose, i }) => {
                                const h = Array.isArray(s.sH) ? String(s.sH[i] ?? 0).padStart(2, '0') : '00';
                                const m = Array.isArray(s.sM) ? String(s.sM[i] ?? 0).padStart(2, '0') : '00';
                                return (
                                    <tr key={i} className="border-b border-white/5 last:border-0">
                                        <td className="py-1 pr-2 font-bold text-accent w-6">{shortDays[i]}</td>
                                        <td className="py-1 font-mono text-muted">{h}:{m}</td>
                                        <td className="py-1 text-right font-bold text-accent2">{dose}<span className="text-muted font-normal">mL</span></td>
                                    </tr>
                                );
                            })}
                        </tbody>
                        <tfoot>
                            <tr className="border-t border-white/10">
                                <td colSpan={2} className="py-1 text-muted font-bold uppercase tracking-wider">{t('fert.totalWeek')}</td>
                                <td className="py-1 text-right font-bold text-white">{totalWeek.toFixed(1)}<span className="text-muted font-normal">mL</span></td>
                            </tr>
                        </tfoot>
                    </table>
                </div>
            )}

            {/* Refill popover */}
            {showRefill && (
                <div className="px-4 pb-2">
                    <div className="flex items-center gap-2 rounded-lg bg-accent2/5 border border-accent2/20 p-2">
                        <input
                            type="number" min="0" max="2000" placeholder="mL" autoFocus
                            className="w-20 min-w-0 rounded-md border border-muted/30 bg-white/5 px-2 py-1.5 text-xs text-text outline-none transition-colors focus:border-accent2 text-center"
                            value={resetVol} onChange={(e) => setResetVol(e.target.value)}
                            onKeyDown={(e) => {
                                if (e.key === 'Enter' && resetVol) {
                                    api('POST', '/api/stock/reset', { channel: index, ml: +resetVol });
                                    setResetVol('');
                                    setShowRefill(false);
                                }
                            }}
                        />
                        <button
                            onClick={() => {
                                if (resetVol) {
                                    api('POST', '/api/stock/reset', { channel: index, ml: +resetVol });
                                    setResetVol('');
                                    setShowRefill(false);
                                }
                            }}
                            className="flex-none rounded-md bg-accent2/20 px-3 py-1.5 text-[10px] font-bold uppercase tracking-wider text-accent2 transition hover:bg-accent2/30 active:scale-95"
                        >
                            OK
                        </button>
                        <button
                            onClick={() => setShowRefill(false)}
                            className="flex-none text-muted hover:text-white text-xs transition"
                        >
                            ✕
                        </button>
                    </div>
                </div>
            )}

            {/* Footer buttons */}
            <div className="flex border-t border-white/5">
                <button
                    onClick={() => setShowRefill(!showRefill)}
                    className="flex-1 flex items-center justify-center gap-1.5 py-2.5 text-[10px] font-bold uppercase tracking-wider text-accent2 transition hover:bg-white/5 active:bg-white/10 border-r border-white/5"
                >
                    <span>📦</span> {t('fert.refill')}
                </button>
                <button
                    onClick={onConfig}
                    className="flex-1 flex items-center justify-center gap-1.5 py-2.5 text-[10px] font-bold uppercase tracking-wider text-muted transition hover:bg-white/5 hover:text-accent active:bg-white/10"
                >
                    <span>⚙️</span> {t('fert.configure')}
                </button>
            </div>
        </div>
    );
}

/* ── Exported FertCard (for reuse in other tabs if needed) ────── */
export { FertCardCompact as FertCard };

/* ── Main FertsTab ─────────────────────────────────────────────── */
export default function FertsTab({ status }: { status: AQStatus | null }) {
    const { t } = useT();
    const [configChannel, setConfigChannel] = useState<number | null>(null);

    if (!status?.stocks) return <div className="text-muted text-center p-4">{t('fert.loading')}</div>;

    // Filter out channel 4 (Prime) ONLY if primeEnabled is true
    const channels = status.stocks
        .map((s, i) => ({ s, i }))
        .filter(c => c.i !== 4 || !status?.primeEnabled);

    return (
        <>
            <div className="flex flex-col gap-3">
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
                    onClose={() => setConfigChannel(null)}
                />
            )}
        </>
    );
}
