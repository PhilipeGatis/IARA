import { type AQStatus } from '../App';
import { api } from '../api';
import { useT, tpaStateKey } from '../i18n';
import { useConfirm } from '../Confirm';
import { levelPercent } from '../LevelBadge';

/* ── Shared "what is still missing" checklist ─────────────────── */
export function ConfigChecklist({ status }: { status: AQStatus | null }) {
    const { t } = useT();
    if (!status || status.tpaConfigReady) return null;

    const missing: string[] = [];
    if (!status.aqLength || status.aqLength <= 0) missing.push(t('config.missing.aqLength'));
    if (!status.aqWidth || status.aqWidth <= 0) missing.push(t('config.missing.aqWidth'));
    if (!status.aqHeight || status.aqHeight <= 0) missing.push(t('config.missing.aqHeight'));
    if (!status.sensorFullDistanceMm || status.sensorFullDistanceMm <= 0) missing.push(t('config.missing.sensorFullDistanceMm'));
    if (!status.reservoirVolume || status.reservoirVolume <= 0) missing.push(t('config.missing.reservoirVolume'));
    if (!status.tpaPercent) missing.push(t('tpa.pctMissing'));
    if (!status.drainFlowRate || status.drainFlowRate <= 0) missing.push(t('config.missing.drainFlowRate'));
    if (!status.refillFlowRate || status.refillFlowRate <= 0) missing.push(t('config.missing.refillFlowRate'));
    if (!status.reservoirSafetyML || status.reservoirSafetyML <= 0) missing.push(t('config.missing.reservoirSafetyML'));
    // Mandatory in the firmware since it gates restoreCanisterIfSafe(); at 0 the
    // safe level equals the whole tank height and every level passes. It was
    // missing from this list, so the banner said configuration was incomplete
    // and then listed nothing that was.
    if (!status.canisterSafePct || status.canisterSafePct <= 0) missing.push(t('config.missing.canisterSafePct'));

    return (
        <div className="note note-w">
            <span className="block text-sm font-bold text-warn">{t('home.configIncomplete')}</span>
            <p className="hint mt-1 text-warn/90">{t('tpa.incompleteMsg')}</p>
            <ul className="mt-2 list-disc space-y-1 pl-5 text-xs font-medium text-warn/90">
                {missing.map((m) => <li key={m}>{m}</li>)}
            </ul>
        </div>
    );
}

/* ── Vertical stock bar ───────────────────────────────────────── */
function StockBar({ label, stock, color }: { label: string; stock: number; color: string }) {
    const pct = Math.min(100, Math.max(0, (stock / 500) * 100));
    return (
        <div className="flex min-w-0 flex-1 flex-col items-center gap-1">
            <span className="text-[11px] font-bold tabular-nums" style={{ color }}>{Math.round(pct)}%</span>
            <div className="relative h-24 w-full max-w-[52px] overflow-hidden rounded-md bg-white/5">
                <div
                    className="absolute bottom-0 w-full transition-all duration-700 ease-out"
                    style={{ height: `${pct}%`, backgroundColor: color, opacity: 0.85 }}
                />
                <div className="absolute inset-0 rounded-md border" style={{ borderColor: color, opacity: 0.4 }} />
            </div>
            <span className="w-full truncate text-center text-[10px] font-bold tracking-wide" style={{ color }}>{label}</span>
            <span className="text-[10px] tabular-nums text-muted">{stock.toFixed(0)} mL</span>
        </div>
    );
}

const STOCK_COLORS = ['#00E5FF', '#FF4FD8', '#FFE45C', '#FFA726', '#66E06A'];

export default function HomeTab({ status }: { status: AQStatus | null }) {
    const { t, lang } = useT();
    const { ask, dialog } = useConfirm();
    const shortDays = t('home.shortDays').split(',');
    const dateLocale = lang === 'ja' ? 'ja-JP' : lang === 'en' ? 'en-US' : 'pt-BR';

    /* ── Water level ─────────────────────────────────────────── */
    // One formula, shared with the header badge. They used to differ by the
    // border margin, so the same tank read two different percentages on the
    // same screen.
    const rawPct = levelPercent(status);
    const levelValid = rawPct !== null;
    const pct = levelValid ? Math.max(0, Math.min(100, Math.round(rawPct))) : 0;
    const levelTone = !levelValid ? 'text-muted' : pct < 25 ? 'text-danger' : pct < 50 ? 'text-warn' : 'text-accent2';
    const levelBar = !levelValid ? 'bg-muted' : pct < 25 ? 'bg-danger' : pct < 50 ? 'bg-warn' : 'bg-accent2';

    // COMPLETE and ERROR are terminal, not running. Gating on `!== 'IDLE'`
    // left the abort button up permanently after the first cycle, with no way
    // back to "start".
    const TERMINAL = ['IDLE', 'COMPLETE', 'ERROR'];
    const running = !!status && !TERMINAL.includes(status.tpaState);
    const stateKey = status ? tpaStateKey(status.tpaState) : null;
    const stateLabel = stateKey ? t(stateKey) : (status?.tpaState ?? '--');

    /* ── Weekly fertilizer table ─────────────────────────────── */
    const renderFertTable = () => {
        if (!status?.stocks) return <div className="hint">{t('home.waiting')}</div>;

        const activeStocks = status.stocks
            .map((s, idx) => ({ ...s, originalIndex: idx }))
            .filter(s => (s.originalIndex !== 4 || !status?.primeEnabled) && s.doses?.some(d => Number(d) > 0));

        if (activeStocks.length === 0) {
            return <div className="hint italic">{t('home.noActiveSchedule')}</div>;
        }

        return (
            <div className="bleed">
                <table className="w-full min-w-[300px] border-collapse text-left">
                    <thead>
                        <tr className="border-b border-border/60 text-[10px] uppercase tracking-wider text-muted">
                            <th className="py-2 pr-2 font-medium">{t('home.channel')}</th>
                            {shortDays.map((d, i) => (
                                <th key={i} className="px-1 py-2 text-center font-medium">{d}</th>
                            ))}
                        </tr>
                    </thead>
                    <tbody className="text-xs">
                        {activeStocks.map((s, i) => (
                            <tr key={i} className="border-b border-border/40 last:border-0">
                                <td className="max-w-[86px] truncate py-3 pr-2 font-medium text-text">
                                    {s.name || `CH ${s.originalIndex + 1}`}
                                </td>
                                {[0, 1, 2, 3, 4, 5, 6].map(di => {
                                    const vol = s.doses?.[di] || 0;
                                    const h = Array.isArray(s.sH) ? String(s.sH[di] ?? 0).padStart(2, '0') : '00';
                                    const m = Array.isArray(s.sM) ? String(s.sM[di] ?? 0).padStart(2, '0') : '00';
                                    return (
                                        <td key={di} className="px-1 py-2.5 text-center align-middle">
                                            {Number(vol) > 0 ? (
                                                <div className="flex flex-col items-center leading-tight">
                                                    <span className="font-bold tabular-nums text-accent2">{Number(vol)}</span>
                                                    <span className="text-[9px] tabular-nums text-muted">{h}:{m}</span>
                                                </div>
                                            ) : (
                                                <span className="text-muted/40">-</span>
                                            )}
                                        </td>
                                    );
                                })}
                            </tr>
                        ))}
                    </tbody>
                </table>
            </div>
        );
    };

    return (
        <div className="flex flex-col gap-3">
            {dialog}
            {/* 1 — Anything blocking the TPA comes first */}
            <ConfigChecklist status={status} />

            {/* 2 — WATER LEVEL (the number you open the app for) */}
            <section className="card">
                <div className="card-h">
                    <h2 className="card-t">{t('home.waterLevel')}</h2>
                    <span className={`pill ${running ? 'bg-accent/20 text-accent' : 'bg-white/5 text-muted'}`}>
                        {stateLabel}
                    </span>
                </div>

                <div className="flex items-end justify-between gap-3">
                    <div className={`font-mono text-5xl font-bold leading-none tabular-nums ${levelTone}`}>
                        {status && levelValid ? pct : '--'}
                        <span className="text-2xl">%</span>
                    </div>
                    {status?.aquariumVolume && levelValid ? (
                        <div className="pb-1 text-right">
                            <div className="font-mono text-lg font-bold tabular-nums text-text">
                                {(status.aquariumVolume * (pct / 100)).toFixed(1)} L
                            </div>
                            <div className="hint">{t('tpa.ofTotal', { v: status.aquariumVolume })}</div>
                        </div>
                    ) : null}
                </div>

                <div className="mt-3 h-3 w-full overflow-hidden rounded-full bg-white/10">
                    <div className={`h-full transition-all duration-500 ease-out ${levelBar}`} style={{ width: `${pct}%` }} />
                </div>

                {/* A dead sensor and a live one produced identical screens: the
                    last median just stopped changing. Say so. */}
                {status && status.sensorsOk === false ? (
                    <p className="mt-3 rounded-lg bg-warn/15 px-3 py-2 text-xs text-warn">
                        {t('home.sensorDown')}
                    </p>
                ) : null}

                {/* Float, canister and maintenance moved to the header, where they
                    are visible from every tab instead of only this one. */}
            </section>

            {/* 3 — TPA: when is the next one, and start/stop it */}
            <section className="card">
                <div className="card-h">
                    <h2 className="card-t">{t('home.tpaSchedule')}</h2>
                </div>

                {status?.tpaInterval ? (() => {
                    const lastRunDate = status.tpaLastRun ? new Date(status.tpaLastRun * 1000) : null;
                    const nextRunDate = lastRunDate
                        ? new Date(lastRunDate.getTime() + status.tpaInterval * 86400000)
                        : null;
                    const now = new Date();
                    const daysUntil = nextRunDate ? Math.ceil((nextRunDate.getTime() - now.getTime()) / 86400000) : null;
                    const soon = daysUntil !== null && daysUntil <= 1;
                    const formatDate = (d: Date) => d.toLocaleDateString(dateLocale, { day: '2-digit', month: '2-digit', year: 'numeric' });

                    return (
                        <>
                            {/* One line for when, one for the details. The schedule is
                                reference material next to the button below it. */}
                            <div className={`flex items-baseline justify-between gap-2 rounded-lg px-3 py-2 ${soon ? 'bg-warn/10' : 'bg-accent2/10'}`}>
                                <span className={`text-base font-bold leading-tight ${soon ? 'text-warn' : 'text-accent2'}`}>
                                    {nextRunDate
                                        ? (daysUntil === 0 ? t('home.today') : daysUntil === 1 ? t('home.tomorrow') : t('home.inDays', { n: daysUntil ?? 0 }))
                                        : '--'}
                                </span>
                                {nextRunDate && (
                                    <span className="font-mono text-[11px] tabular-nums text-muted">
                                        {formatDate(nextRunDate)} · {String(status.tpaHour).padStart(2, '0')}:{String(status.tpaMinute).padStart(2, '0')}
                                    </span>
                                )}
                            </div>

                            <p className="hint mt-1.5">
                                {status.tpaPercent}%
                                {status.aquariumVolume ? ` (${(status.aquariumVolume * status.tpaPercent / 100).toFixed(1)} L)` : ''}
                                {' · '}
                                {t('home.everyDays', { n: status.tpaInterval, s: status.tpaInterval > 1 ? 's' : '' })}
                                {' · '}
                                {t('home.lastRun')}: {lastRunDate ? formatDate(lastRunDate) : t('home.never')}
                            </p>
                        </>
                    );
                })() : (
                    <div className="hint italic">{t('home.noSchedule')}</div>
                )}

                {/* The configured percentage and the litres a cycle can actually
                    move are not the same number once the reservoir caps it. That
                    used to happen silently, at runtime, with only a serial line. */}
                {(() => {
                    const planned = status?.tpaPlannedLiters ?? 0;
                    const wanted = status?.aquariumVolume && status?.tpaPercent
                        ? (status.aquariumVolume * status.tpaPercent) / 100
                        : 0;
                    if (planned <= 0 || wanted <= 0 || planned >= wanted - 0.05) return null;
                    return (
                        <p className="mt-2 rounded-lg bg-warn/15 px-3 py-2 text-xs text-warn">
                            {t('home.tpaCapped', { v: planned.toFixed(1) })}
                        </p>
                    );
                })()}

                {status?.tpaState === 'ERROR' && status?.lastError ? (
                    <p className="mt-2 rounded-lg bg-danger/15 px-3 py-2 text-xs text-danger">
                        {t('home.tpaError', { e: status.lastError })}
                    </p>
                ) : null}

                {status?.tpaBlockedReason ? (
                    <p className="mt-2 rounded-lg bg-danger/15 px-3 py-2 text-xs text-danger">
                        {t('home.tpaBlocked', { r: status.tpaBlockedReason })}
                    </p>
                ) : null}

                {/* Primary action */}
                <div className="mt-4">
                    {!running ? (
                        <button
                            onClick={async () => {
                                if (await ask(t('confirm.tpaStart', { pct: status?.tpaPercent ?? 0 }))) {
                                    api('POST', '/api/tpa/start');
                                }
                            }}
                            className="btn btn-p2 w-full"
                            disabled={!status?.tpaConfigReady}
                        >
                            {t('tpa.start')}
                        </button>
                    ) : (
                        <button
                            onClick={async () => {
                                if (await ask(t('confirm.tpaAbort'))) api('POST', '/api/tpa/abort');
                            }}
                            className="btn btn-dd w-full"
                        >
                            {t('tpa.abort')}
                        </button>
                    )}
                </div>
            </section>

            {/* 4 — Fertilizer stock */}
            {status?.stocks && (
                <section className="card">
                    <div className="card-h">
                        <h2 className="card-t">{t('home.stockBars')}</h2>
                    </div>
                    <div className="flex items-end justify-around gap-2">
                        {status.stocks.map((s, i) => (
                            <StockBar
                                key={i}
                                stock={s.stock}
                                color={STOCK_COLORS[i] || STOCK_COLORS[4]}
                                label={i === 4 && status.primeEnabled ? (s.name || 'Prime') : (s.name || `F${i + 1}`).substring(0, 6)}
                            />
                        ))}
                    </div>
                </section>
            )}

            {/* 5 — Weekly dosing table */}
            <section className="card">
                <div className="card-h">
                    <h2 className="card-t">{t('home.fertTable')}</h2>
                    <span className="pill bg-accent2/15 text-accent2">{t('home.inMl')}</span>
                </div>
                {renderFertTable()}
            </section>
        </div>
    );
}
