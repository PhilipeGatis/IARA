import type { AQStatus } from '../App';
import { useT } from '../i18n';

function Badge({ label, on, texts }: { label: string; on?: boolean; texts: [string, string] }) {
    if (on === undefined) return null;
    return (
        <div className="flex items-center justify-between border-b border-border/50 py-3 last:border-0">
            <span className="text-sm font-medium tracking-wide text-muted">{label}</span>
            <span className={`rounded-full px-3 py-1 text-xs font-bold tracking-wider ${on ? 'bg-accent/20 text-accent' : 'bg-white/5 text-muted'}`}>
                {on ? texts[0] : texts[1]}
            </span>
        </div>
    );
}

export default function HomeTab({ status }: { status: AQStatus | null }) {
    const { t, lang } = useT();
    const shortDays = t('home.shortDays').split(',');

    const renderFertTable = () => {
        if (!status?.stocks) return <div className="text-xs text-muted">{t('home.waiting')}</div>;

        // Find stocks that have at least one dose > 0, excluding Prime (index 4)
        const activeStocks = status.stocks
            .map((s, idx) => ({ ...s, originalIndex: idx }))
            .filter(s => s.originalIndex !== 4 && s.doses?.some(d => Number(d) > 0));

        if (activeStocks.length === 0) {
            return (
                <div className="text-xs font-semibold text-muted italic mt-1">{t('home.noActiveSchedule')}</div>
            );
        }

        return (
            <div className="overflow-x-auto mt-2 rounded bg-black/20 p-2">
                <table className="w-full text-left border-collapse">
                    <thead>
                        <tr className="border-b border-white/10 text-[10px] text-muted uppercase tracking-wider">
                            <th className="py-2 pr-2 font-medium">{t('home.channel')}</th>
                            <th className="py-2 px-1 font-medium text-center">{t('home.hour')}</th>
                            {shortDays.map((d, i) => (
                                <th key={i} className="py-2 px-1 font-medium text-center">{d}</th>
                            ))}
                        </tr>
                    </thead>
                    <tbody className="text-xs">
                        {activeStocks.map((s, i) => (
                            <tr key={i} className="border-b border-white/5 last:border-0">
                                <td className="py-3 pr-2 font-medium text-text whitespace-nowrap">
                                    {s.name || `CH ${s.originalIndex + 1}`}
                                </td>
                                <td className="py-3 px-1 font-mono text-muted text-center tracking-tighter">
                                    {String(s.sH || 0).padStart(2, '0')}:{String(s.sM || 0).padStart(2, '0')}
                                </td>
                                {[0, 1, 2, 3, 4, 5, 6].map(di => {
                                    const vol = s.doses?.[di] || 0;
                                    return (
                                        <td key={di} className="py-3 px-1 text-center">
                                            {Number(vol) > 0 ? (
                                                <span className="font-bold text-accent2">{Number(vol)}</span>
                                            ) : (
                                                <span className="text-muted/30">-</span>
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

    const dateLocale = lang === 'ja' ? 'ja-JP' : lang === 'en' ? 'en-US' : 'pt-BR';

    return (
        <div className="flex flex-col gap-4">
            {/* Sensors Card */}
            <div className="rounded-2xl bg-card p-5 shadow-md">
                <h2 className="mb-4 text-base font-medium tracking-wide text-text/90 uppercase">{t('home.sensors')}</h2>

                <div className="mb-5">
                    {(() => {
                        const wl = status?.waterLevel || 0;
                        const refCm = status?.aqHeight || 20;
                        const pct = status?.optical ? 100 : Math.max(0, Math.min(100, Math.round((1 - wl / refCm) * 100)));
                        const color = pct < 25 ? 'text-danger' : pct < 50 ? 'text-warn' : 'text-accent2';
                        const barColor = pct < 25 ? 'bg-danger' : pct < 50 ? 'bg-warn' : 'bg-accent2';
                        return (
                            <>
                                <div className="mb-2 flex justify-between text-sm font-medium">
                                    <span className="text-muted tracking-wide">{t('home.waterLevel')}</span>
                                    <span className={`font-mono text-lg ${color}`}>
                                        {status ? `${pct}%` : '--'}
                                    </span>
                                </div>
                                <div className="h-2 w-full overflow-hidden rounded-full bg-white/5">
                                    <div
                                        className={`h-full transition-all duration-500 ease-out ${barColor}`}
                                        style={{ width: `${pct}%` }}
                                    />
                                </div>
                            </>
                        );
                    })()}
                </div>

                <div className="flex flex-col">
                    <Badge label={t('home.optical')} on={status?.optical} texts={[t('home.opticalOn'), t('home.opticalOff')]} />
                    <Badge label={t('home.float')} on={status?.float} texts={[t('home.floatOn'), t('home.floatOff')]} />
                    <Badge label={t('home.canister')} on={status?.canister} texts={[t('home.canisterOn'), t('home.canisterOff')]} />
                </div>
            </div>

            {/* Fertilizer Stock Bars */}
            {status?.stocks && (
                <div className="rounded-2xl bg-card p-5 shadow-md">
                    <h2 className="mb-4 text-base font-medium tracking-wide text-text/90 uppercase">{t('home.stockBars')}</h2>
                    <div className="flex items-end justify-around gap-2">
                        {status.stocks.map((s, i) => {
                            if (i === 4) return null; // Skip Prime in this section
                            const pct = Math.min(100, Math.max(0, (s.stock / 500) * 100));
                            const colors = ['#00FFFF', '#FF00FF', '#FFFF00', '#FFA500'];
                            const color = colors[i] || '#00FF00';
                            const label = s.name || `F${i + 1}`;
                            return (
                                <div key={i} className="flex flex-col items-center gap-1 flex-1 max-w-[60px]">
                                    <span className="text-xs font-bold" style={{ color }}>{Math.round(pct)}%</span>
                                    <div className="relative w-full h-24 rounded bg-white/5 overflow-hidden">
                                        <div
                                            className="absolute bottom-0 w-full rounded transition-all duration-700 ease-out"
                                            style={{ height: `${pct}%`, backgroundColor: color, opacity: 0.85 }}
                                        />
                                        <div
                                            className="absolute inset-0 rounded border"
                                            style={{ borderColor: color, opacity: 0.4 }}
                                        />
                                    </div>
                                    <span className="text-[10px] font-bold tracking-wider" style={{ color }}>{label.substring(0, 4)}</span>
                                    <span className="text-[9px] text-muted">{s.stock.toFixed(0)} mL</span>
                                </div>
                            );
                        })}
                        {/* Prime bar */}
                        {status.stocks.length >= 5 && (() => {
                            const s = status.stocks[4];
                            const pct = Math.min(100, Math.max(0, (s.stock / 500) * 100));
                            const color = '#00FF00';
                            return (
                                <div className="flex flex-col items-center gap-1 flex-1 max-w-[60px]">
                                    <span className="text-xs font-bold" style={{ color }}>{Math.round(pct)}%</span>
                                    <div className="relative w-full h-24 rounded bg-white/5 overflow-hidden">
                                        <div
                                            className="absolute bottom-0 w-full rounded transition-all duration-700 ease-out"
                                            style={{ height: `${pct}%`, backgroundColor: color, opacity: 0.85 }}
                                        />
                                        <div
                                            className="absolute inset-0 rounded border"
                                            style={{ borderColor: color, opacity: 0.4 }}
                                        />
                                    </div>
                                    <span className="text-[10px] font-bold tracking-wider" style={{ color }}>{s.name || 'Prime'}</span>
                                    <span className="text-[9px] text-muted">{s.stock.toFixed(0)} mL</span>
                                </div>
                            );
                        })()}
                    </div>
                </div>
            )}

            {/* Routine State Card */}
            <div className="rounded-2xl bg-card p-5 shadow-md">
                <h2 className="mb-4 text-base font-medium tracking-wide text-text/90 uppercase">{t('home.systemState')}</h2>
                <div className="flex flex-col">
                    <Badge label={t('home.tpaState')} on={status?.tpaState !== 'IDLE'} texts={[status?.tpaState || '', 'IDLE']} />
                    <Badge label={t('home.maintenance')} on={status?.maintenance} texts={[t('home.maintActive'), t('home.maintInactive')]} />
                </div>
            </div>

            {/* TPA SCHEDULE SUMMARY */}
            <div className="rounded-2xl bg-card p-5 shadow-md">
                <h2 className="mb-4 text-base font-medium tracking-wide text-text/90 uppercase">{t('home.tpaSchedule')}</h2>
                {status && !status.tpaConfigReady && (
                    <div className="rounded-lg bg-warn/10 border border-warn/30 px-4 py-3 mb-4">
                        <span className="text-xs font-bold text-warn">{t('home.configIncomplete')}</span>
                    </div>
                )}
                {status?.tpaInterval ? (() => {
                    const lastRunDate = status.tpaLastRun ? new Date(status.tpaLastRun * 1000) : null;
                    const nextRunDate = lastRunDate
                        ? new Date(lastRunDate.getTime() + status.tpaInterval * 86400000)
                        : null;
                    const now = new Date();
                    const daysUntil = nextRunDate ? Math.ceil((nextRunDate.getTime() - now.getTime()) / 86400000) : null;
                    const formatDate = (d: Date) => d.toLocaleDateString(dateLocale, { day: '2-digit', month: '2-digit', year: 'numeric' });

                    return (
                        <div className="flex flex-col gap-3">
                            <div className="flex items-center justify-between border-b border-border/50 py-2">
                                <span className="text-sm text-muted">{t('home.volume')}</span>
                                <span className="font-mono text-sm font-bold text-accent">
                                    {status.tpaPercent}%{status.aquariumVolume ? ` (${(status.aquariumVolume * status.tpaPercent / 100).toFixed(1)} L)` : ''}
                                </span>
                            </div>
                            <div className="flex items-center justify-between border-b border-border/50 py-2">
                                <span className="text-sm text-muted">{t('home.interval')}</span>
                                <span className="font-mono text-sm font-bold text-text">
                                    {t('home.everyDays', { n: status.tpaInterval, s: status.tpaInterval > 1 ? 's' : '' })}
                                </span>
                            </div>
                            <div className="flex items-center justify-between border-b border-border/50 py-2">
                                <span className="text-sm text-muted">{t('home.time')}</span>
                                <span className="font-mono text-sm font-bold text-text">{String(status.tpaHour).padStart(2, '0')}:{String(status.tpaMinute).padStart(2, '0')}</span>
                            </div>
                            <div className="flex items-center justify-between border-b border-border/50 py-2">
                                <span className="text-sm text-muted">{t('home.lastRun')}</span>
                                <span className="font-mono text-xs text-muted">{lastRunDate ? formatDate(lastRunDate) : t('home.never')}</span>
                            </div>
                            <div className="flex items-center justify-between py-2">
                                <span className="text-sm text-muted">{t('home.nextTpa')}</span>
                                <span className={`font-mono text-sm font-bold ${daysUntil !== null && daysUntil <= 1 ? 'text-warn' : 'text-accent2'}`}>
                                    {nextRunDate ? `${formatDate(nextRunDate)} (${daysUntil === 0 ? t('home.today') : daysUntil === 1 ? t('home.tomorrow') : t('home.inDays', { n: daysUntil ?? 0 })})` : '--'}
                                </span>
                            </div>
                        </div>
                    );
                })() : (
                    <div className="text-xs text-muted italic">{t('home.noSchedule')}</div>
                )}
            </div>

            {/* FERTILIZERS WEEKLY VIEW */}
            <div className="rounded-2xl bg-card p-5 shadow-md">
                <div className="mb-4 flex items-center justify-between">
                    <h2 className="text-base font-medium tracking-wide text-text/90 uppercase">{t('home.fertTable')}</h2>
                    <span className="text-[10px] font-bold text-accent2 uppercase tracking-wider bg-accent2/10 px-2 py-1 rounded-full border border-accent2/20">{t('home.inMl')}</span>
                </div>
                {renderFertTable()}
            </div>
        </div>
    );
}
