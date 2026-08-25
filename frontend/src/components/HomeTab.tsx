import { type AQStatus } from '../App';
import { api } from '../api';
import { useT } from '../i18n';
import { useConfirm } from '../Confirm';

/* ── Small label / status row ─────────────────────────────────── */
function Badge({ label, on, texts }: { label: string; on?: boolean; texts: [string, string] }) {
    if (on === undefined) return null;
    return (
        <div className="row">
            <span className="row-k">{label}</span>
            <span className={`pill ${on ? 'bg-accent/20 text-accent' : 'bg-white/5 text-muted'}`}>
                {on ? texts[0] : texts[1]}
            </span>
        </div>
    );
}

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
    const wl = status?.waterLevel ?? 0;
    const refCm = (status?.aqHeight || 20) - ((status?.aqMarginMm || 0) / 10);
    const sensorFull = (status?.sensorFullDistanceMm || 0) / 10;
    const levelValid = wl >= 0;
    const pct = levelValid ? Math.max(0, Math.min(100, Math.round(100 - ((wl - sensorFull) / refCm) * 100))) : 0;
    const levelTone = !levelValid ? 'text-muted' : pct < 25 ? 'text-danger' : pct < 50 ? 'text-warn' : 'text-accent2';
    const levelBar = !levelValid ? 'bg-muted' : pct < 25 ? 'bg-danger' : pct < 50 ? 'bg-warn' : 'bg-accent2';

    const running = !!status && status.tpaState !== 'IDLE';

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
                        {status ? status.tpaState : '--'}
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

                <div className="mt-2 flex flex-col">
                    <Badge label={t('home.float')} on={status?.float} texts={[t('home.floatOn'), t('home.floatOff')]} />
                    <Badge label={t('home.canister')} on={status?.canister} texts={[t('home.canisterOn'), t('home.canisterOff')]} />
                    <Badge label={t('home.maintenance')} on={status?.maintenance} texts={[t('home.maintActive'), t('home.maintInactive')]} />
                </div>
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
                            <div className={`rounded-xl px-4 py-3 ${soon ? 'bg-warn/10' : 'bg-accent2/10'}`}>
                                <div className="lbl">{t('home.nextTpa')}</div>
                                <div className={`mt-0.5 text-xl font-bold leading-tight ${soon ? 'text-warn' : 'text-accent2'}`}>
                                    {nextRunDate
                                        ? (daysUntil === 0 ? t('home.today') : daysUntil === 1 ? t('home.tomorrow') : t('home.inDays', { n: daysUntil ?? 0 }))
                                        : '--'}
                                </div>
                                {nextRunDate && (
                                    <div className="hint font-mono">
                                        {formatDate(nextRunDate)} · {String(status.tpaHour).padStart(2, '0')}:{String(status.tpaMinute).padStart(2, '0')}
                                    </div>
                                )}
                            </div>

                            <div className="mt-2 flex flex-col">
                                <div className="row">
                                    <span className="row-k">{t('home.volume')}</span>
                                    <span className="row-v text-accent">
                                        {status.tpaPercent}%{status.aquariumVolume ? ` (${(status.aquariumVolume * status.tpaPercent / 100).toFixed(1)} L)` : ''}
                                    </span>
                                </div>
                                <div className="row">
                                    <span className="row-k">{t('home.interval')}</span>
                                    <span className="row-v">
                                        {t('home.everyDays', { n: status.tpaInterval, s: status.tpaInterval > 1 ? 's' : '' })}
                                    </span>
                                </div>
                                <div className="row">
                                    <span className="row-k">{t('home.lastRun')}</span>
                                    <span className="row-v text-muted">{lastRunDate ? formatDate(lastRunDate) : t('home.never')}</span>
                                </div>
                            </div>
                        </>
                    );
                })() : (
                    <div className="hint italic">{t('home.noSchedule')}</div>
                )}

                {/* Primary action */}
                <div className="mt-4">
                    {status?.tpaState === 'IDLE' ? (
                        <button
                            onClick={async () => {
                                if (await ask(t('confirm.tpaStart', { pct: status.tpaPercent }))) {
                                    api('POST', '/api/tpa/start');
                                }
                            }}
                            className="btn btn-p2 w-full"
                            disabled={!status.tpaConfigReady}
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
