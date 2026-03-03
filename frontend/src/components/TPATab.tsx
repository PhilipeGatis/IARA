import { useState, useEffect } from 'react';
import { api, type AQStatus } from '../App';
import { FertCard } from './FertsTab';
import { useT } from '../i18n';


export default function TPATab({ status }: { status: AQStatus | null }) {
    const { t } = useT();
    // Schedule Builder States
    const [interval, setInterval] = useState('');
    const [h, setH] = useState('10');
    const [m, setM] = useState('00');
    const [pct, setPct] = useState('20');

    // Reservoir Safety
    const [safetyML, setSafetyML] = useState('');
    const [drainMl, setDrainMl] = useState('');
    const [refillMl, setRefillMl] = useState('');
    const [running3s, setRunning3s] = useState<string | null>(null);

    useEffect(() => {
        if (status) {
            if (!interval && status.tpaInterval !== undefined) setInterval(status.tpaInterval.toString());
            if (status.tpaHour !== undefined) setH(status.tpaHour.toString().padStart(2, '0'));
            if (status.tpaMinute !== undefined) setM(status.tpaMinute.toString().padStart(2, '0'));
            if (!pct && status.tpaPercent) setPct(status.tpaPercent.toString());
            if (!safetyML && status.reservoirSafetyML !== undefined) setSafetyML(status.reservoirSafetyML.toString());
        }
    }, [status]);

    const handleSaveSchedule = () => {
        api('POST', '/api/schedule', {
            tpaInterval: parseInt(interval) || 0,
            tpaHour: parseInt(h) || 0,
            tpaMinute: parseInt(m) || 0,
            tpaPercent: parseInt(pct) || 20
        });
    };

    const handleSaveConfig = () => {
        api('POST', '/api/tpa/config', {
            reservoirSafetyML: parseFloat(safetyML) || 0
        });
    };

    const handlePump = (pump: 'drain' | 'refill', state: number) => {
        api('POST', '/api/tpa/pump', { pump, state });
    };

    return (
        <div className="flex flex-col gap-4 pb-4">
            {/* TPA SCHEDULING CARD */}
            <div className="rounded-2xl bg-card p-5 shadow-md">
                <h2 className="mb-4 text-base font-medium tracking-wide text-text/90 uppercase">{t('tpa.auto')}</h2>

                <div className="flex flex-col gap-4">
                    <div className="flex flex-col gap-1">
                        <label className="text-xs font-bold text-muted uppercase tracking-wider">{t('tpa.frequency')}</label>
                        <input
                            type="number" min="0" max="90" placeholder={t('tpa.disabled')}
                            className="w-full rounded-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm text-text outline-none transition-colors focus:border-accent"
                            value={interval} onChange={e => setInterval(e.target.value)}
                        />
                        <span className="text-[10px] text-muted italic mt-1">{t('tpa.freqHint')}</span>
                    </div>

                    <div className="flex flex-col gap-1">
                        <label className="text-xs font-bold text-muted uppercase tracking-wider">{t('tpa.volumePct')}</label>
                        <input
                            type="number" min="1" max="100" step="1" placeholder="Ex: 20"
                            className="w-full rounded-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm text-text outline-none transition-colors focus:border-accent"
                            value={pct} onChange={e => setPct(e.target.value)}
                        />
                        {status?.aquariumVolume ? (
                            <span className="text-[10px] text-muted italic mt-1">
                                = <strong className="text-accent">{(status.aquariumVolume * (parseInt(pct) || 0) / 100).toFixed(1)} L</strong> {t('tpa.ofTotal', { v: status.aquariumVolume })}
                            </span>
                        ) : (
                            <span className="text-[10px] text-muted italic mt-1">{t('tpa.configDimHint')}</span>
                        )}
                    </div>

                    <div className="flex gap-4 mt-2">
                        <div className="flex-1 flex flex-col gap-1">
                            <label className="text-xs font-bold text-muted uppercase tracking-wider">{t('tpa.hour')}</label>
                            <input
                                type="number" min="0" max="23" placeholder="HH"
                                className="w-full rounded-md border-b-2 border-muted bg-white/5 px-3 py-2 text-center text-sm text-text outline-none transition-colors focus:border-accent"
                                value={h} onChange={e => setH(e.target.value)}
                            />
                        </div>
                        <div className="flex items-center text-muted font-bold text-xl mt-5">:</div>
                        <div className="flex-1 flex flex-col gap-1">
                            <label className="text-xs font-bold text-muted uppercase tracking-wider">{t('tpa.minute')}</label>
                            <input
                                type="number" min="0" max="59" placeholder="MM"
                                className="w-full rounded-md border-b-2 border-muted bg-white/5 px-3 py-2 text-center text-sm text-text outline-none transition-colors focus:border-accent"
                                value={m} onChange={e => setM(e.target.value)}
                            />
                        </div>
                    </div>

                    <div className="mt-5 flex items-center justify-between">
                        <span className="text-xs text-muted italic">
                            {t('tpa.scheduled')} <strong className="text-accent">{parseInt(interval) > 0 ? `${pct}% a cada ${interval} dias às ${h.padStart(2, '0')}:${m.padStart(2, '0')}` : t('tpa.disabledLabel')}</strong>
                        </span>
                        <button
                            onClick={handleSaveSchedule}
                            className="rounded-full bg-accent px-5 py-2 text-[10px] font-bold uppercase tracking-wider text-black shadow-md transition-all hover:bg-blue-300 active:scale-95"
                        >
                            {t('tpa.saveSchedule')}
                        </button>
                    </div>
                </div>
            </div>

            {/* CONFIG STATUS */}
            {status && !status.tpaConfigReady && (
                <div className="rounded-2xl bg-warn/10 border border-warn/30 p-4 shadow-md">
                    <h2 className="mb-2 text-sm font-bold text-warn uppercase tracking-wide">{t('tpa.incompleteConfig')}</h2>
                    <p className="text-xs text-muted">{t('tpa.incompleteMsg')} <strong className="text-text">Config</strong>:</p>
                    <ul className="mt-2 text-xs text-muted list-disc list-inside">
                        {!status.aqHeight || !status.aqLength || !status.aqWidth ? <li>{t('tpa.dimMissing')}</li> : null}
                        {!status.reservoirVolume ? <li>{t('tpa.reservoirMissing')}</li> : null}
                        {!status.tpaPercent ? <li>{t('tpa.pctMissing')}</li> : null}
                    </ul>
                </div>
            )}

            {/* RESERVOIR SAFETY + PUMP TEST */}
            <div className="rounded-2xl bg-card p-5 shadow-md">
                <h2 className="mb-4 text-base font-medium tracking-wide text-text/90 uppercase">{t('tpa.reservoir')}</h2>
                <div className="flex flex-col gap-5">

                    <div className="flex flex-col gap-2">
                        <label className="text-xs font-bold text-muted uppercase tracking-wider">{t('tpa.safetyMargin')}</label>
                        <input
                            type="number" min="0" max="99999" step="100" placeholder="Ex: 500"
                            className="w-full rounded-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm text-text outline-none transition-colors focus:border-accent"
                            value={safetyML} onChange={e => setSafetyML(e.target.value)}
                        />
                        <span className="text-[10px] text-muted italic mt-1">{t('tpa.safetyHint')}</span>
                    </div>

                    <div className="flex gap-3">
                        <button
                            onMouseDown={() => handlePump('drain', 1)} onMouseUp={() => handlePump('drain', 0)}
                            onTouchStart={() => handlePump('drain', 1)} onTouchEnd={() => handlePump('drain', 0)}
                            className="flex-1 rounded-md border border-muted bg-transparent px-4 py-3 text-[10px] font-bold uppercase tracking-wider text-muted transition hover:bg-white/5 active:bg-white/10 select-none"
                        >
                            {t('tpa.testDrain')}
                        </button>
                        <button
                            onMouseDown={() => handlePump('refill', 1)} onMouseUp={() => handlePump('refill', 0)}
                            onTouchStart={() => handlePump('refill', 1)} onTouchEnd={() => handlePump('refill', 0)}
                            className="flex-1 rounded-md border border-muted bg-transparent px-4 py-3 text-[10px] font-bold uppercase tracking-wider text-muted transition hover:bg-white/5 active:bg-white/10 select-none"
                        >
                            {t('tpa.testRefill')}
                        </button>
                    </div>

                    <div className="flex flex-col gap-2">
                        <label className="text-xs font-bold text-muted uppercase tracking-wider">{t('tpa.primeDose')}</label>
                        <div className="rounded-lg bg-accent/10 px-4 py-3">
                            <strong className="text-accent text-sm">{status?.primeMl ? `${status.primeMl.toFixed(1)} mL` : t('tpa.configInConfigTab')}</strong>
                            <span className="text-[10px] text-muted italic ml-2">{t('tpa.autoCalc')}</span>
                        </div>
                    </div>

                    <button
                        onClick={handleSaveConfig}
                        className="mt-2 rounded-full bg-accent2 px-6 py-2.5 text-sm font-bold uppercase tracking-wider text-black shadow-md transition-all hover:bg-teal-300 active:scale-95"
                    >
                        {t('tpa.saveConfig')}
                    </button>

                </div>
            </div>

            {/* PUMP CALIBRATION CARD */}
            <div className="rounded-2xl bg-card p-5 shadow-md">
                <h2 className="mb-4 text-base font-medium tracking-wide text-text/90 uppercase">{t('tpa.pumpCalib')}</h2>

                {/* Drain (Diafragma) */}
                <div className="flex flex-col gap-3 mb-5">
                    <label className="text-xs font-bold text-muted uppercase tracking-wider">{t('tpa.drainPump')}</label>
                    <div className="flex items-center gap-2">
                        <button
                            onClick={async () => {
                                setRunning3s('drain');
                                await fetch('/api/tpa/run3s', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ pump: 'drain' }) });
                                setRunning3s(null);
                            }}
                            disabled={running3s !== null}
                            className="flex-none rounded-md bg-accent/20 px-4 py-2 text-[10px] font-bold uppercase tracking-wider text-accent transition hover:bg-accent/30 active:scale-95 disabled:opacity-50"
                        >
                            {running3s === 'drain' ? t('tpa.running') : t('tpa.run3s')}
                        </button>
                        <input
                            type="number" step="0.1" min="0" placeholder={t('tpa.mlMeasured')}
                            className="flex-1 rounded-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm text-text outline-none transition-colors focus:border-accent"
                            value={drainMl} onChange={e => setDrainMl(e.target.value)}
                        />
                        <button
                            onClick={() => { api('POST', '/api/tpa/calibrate', { pump: 'drain', ml: parseFloat(drainMl) }); setDrainMl(''); }}
                            disabled={!drainMl}
                            className="flex-none rounded-md bg-accent2/20 px-4 py-2 text-[10px] font-bold uppercase tracking-wider text-accent2 transition hover:bg-accent2/30 active:scale-95 disabled:opacity-50"
                        >
                            {t('tpa.calibrate')}
                        </button>
                    </div>
                    <span className="text-[10px] text-muted italic">{t('tpa.flowRate')} <strong className="text-accent">{status?.drainFlowRate ? `${status.drainFlowRate.toFixed(2)} mL/s` : t('tpa.notCalibrated')}</strong></span>
                    <button
                        onMouseDown={() => handlePump('drain', 1)} onMouseUp={() => handlePump('drain', 0)}
                        onTouchStart={() => handlePump('drain', 1)} onTouchEnd={() => handlePump('drain', 0)}
                        className="mt-1 w-full rounded-md border border-muted bg-transparent px-4 py-2 text-[10px] font-bold uppercase tracking-wider text-muted transition hover:bg-white/5 active:bg-white/10 select-none"
                    >
                        {t('tpa.purgeDrain')}
                    </button>
                </div>

                {/* Refill (Recalque) */}
                <div className="flex flex-col gap-3">
                    <label className="text-xs font-bold text-muted uppercase tracking-wider">{t('tpa.refillPump')}</label>
                    <div className="flex items-center gap-2">
                        <button
                            onClick={async () => {
                                setRunning3s('refill');
                                await fetch('/api/tpa/run3s', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ pump: 'refill' }) });
                                setRunning3s(null);
                            }}
                            disabled={running3s !== null}
                            className="flex-none rounded-md bg-accent/20 px-4 py-2 text-[10px] font-bold uppercase tracking-wider text-accent transition hover:bg-accent/30 active:scale-95 disabled:opacity-50"
                        >
                            {running3s === 'refill' ? t('tpa.running') : t('tpa.run3s')}
                        </button>
                        <input
                            type="number" step="0.1" min="0" placeholder={t('tpa.mlMeasured')}
                            className="flex-1 rounded-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm text-text outline-none transition-colors focus:border-accent"
                            value={refillMl} onChange={e => setRefillMl(e.target.value)}
                        />
                        <button
                            onClick={() => { api('POST', '/api/tpa/calibrate', { pump: 'refill', ml: parseFloat(refillMl) }); setRefillMl(''); }}
                            disabled={!refillMl}
                            className="flex-none rounded-md bg-accent2/20 px-4 py-2 text-[10px] font-bold uppercase tracking-wider text-accent2 transition hover:bg-accent2/30 active:scale-95 disabled:opacity-50"
                        >
                            {t('tpa.calibrate')}
                        </button>
                    </div>
                    <span className="text-[10px] text-muted italic">{t('tpa.flowRate')} <strong className="text-accent">{status?.refillFlowRate ? `${status.refillFlowRate.toFixed(2)} mL/s` : t('tpa.notCalibrated')}</strong></span>
                    <button
                        onMouseDown={() => handlePump('refill', 1)} onMouseUp={() => handlePump('refill', 0)}
                        onTouchStart={() => handlePump('refill', 1)} onTouchEnd={() => handlePump('refill', 0)}
                        className="mt-1 w-full rounded-md border border-muted bg-transparent px-4 py-2 text-[10px] font-bold uppercase tracking-wider text-muted transition hover:bg-white/5 active:bg-white/10 select-none"
                    >
                        {t('tpa.purgeRefill')}
                    </button>
                </div>
            </div>

            {/* PRIME (CH5) CONFIGURATION */}
            {status?.stocks && status.stocks.length >= 5 && (
                <FertCard index={4} s={status.stocks[4]} hideAgenda={true} />
            )}

        </div>
    );
}
