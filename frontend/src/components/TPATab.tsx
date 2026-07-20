import { useState, useEffect, useRef } from 'react';
import { api, type AQStatus } from '../App';
import { FertCard } from './FertsTab';
import FertConfigModal from './FertConfigModal';
import { useT } from '../i18n';


export default function TPATab({ status }: { status: AQStatus | null }) {
    const { t } = useT();
    // Schedule Builder States
    const [interval, setInterval] = useState('');
    const [autoEnabled, setAutoEnabled] = useState(false);
    const [h, setH] = useState('10');
    const [m, setM] = useState('00');
    const [pct, setPct] = useState('20');

    // Reservoir Safety
    const [safetyML, setSafetyML] = useState('');
    const [primeEnabled, setPrimeEnabled] = useState(true);
    const [drainGoal, setDrainGoal] = useState('');
    const [refillGoal, setRefillGoal] = useState('');

    // Prime config modal
    const [showPrimeConfig, setShowPrimeConfig] = useState(false);

    const initialized = useRef(false);

    useEffect(() => {
        if (status && !initialized.current) {
            initialized.current = true;
            if (status.tpaAutoEnabled !== undefined) setAutoEnabled(status.tpaAutoEnabled);
            if (status.tpaInterval !== undefined) setInterval(status.tpaInterval.toString());
            if (status.tpaHour !== undefined) setH(status.tpaHour.toString().padStart(2, '0'));
            if (status.tpaMinute !== undefined) setM(status.tpaMinute.toString().padStart(2, '0'));
            if (status.tpaPercent) setPct(status.tpaPercent.toString());
            if (status.reservoirSafetyML !== undefined) setSafetyML(status.reservoirSafetyML.toString());
            if (status.primeEnabled !== undefined) setPrimeEnabled(status.primeEnabled);
        }
    }, [status]);

    const handleSaveSchedule = () => {
        api('POST', '/api/schedule', {
            tpaInterval: parseInt(interval) || 0,
            tpaAutoEnabled: autoEnabled ? 1 : 0,
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

    const handlePump = (pump: 'drain' | 'refill' | 'solenoid', state: number, liters?: number) => {
        api('POST', '/api/tpa/pump', { pump, state, liters });
    };

    return (
        <div className="flex flex-col gap-4 pb-4">
            {/* TPA PROGRESS BANNER */}
            {status && status.pumpGoalLiters !== undefined && status.pumpGoalLiters > 0 && (
                <div className="rounded-2xl bg-card p-5 shadow-md border-l-4 border-accent">
                    <div className="flex justify-between items-center mb-2">
                        <h2 className="text-sm font-bold tracking-wide text-accent uppercase">{t('tpa.pumpProgress')}</h2>
                        <span className="text-xs font-mono text-muted">{t('tpa.pumpTime')} {Math.floor((status.pumpElapsedMs || 0) / 60000).toString().padStart(2, '0')}:{Math.floor(((status.pumpElapsedMs || 0) / 1000) % 60).toString().padStart(2, '0')}</span>
                    </div>
                    <div className="w-full bg-white/10 rounded-full h-4 mb-1 overflow-hidden relative">
                        <div 
                            className="bg-accent h-4 rounded-full transition-all duration-1000 ease-linear"
                            style={{ width: `${Math.min(100, Math.max(0, ((status.pumpProgressLiters || 0) / status.pumpGoalLiters) * 100))}%` }}
                        ></div>
                    </div>
                    <div className="flex justify-between text-[10px] text-muted font-bold px-1">
                        <span>{(status.pumpProgressLiters || 0).toFixed(1)} L</span>
                        <span>{status.pumpGoalLiters.toFixed(1)} L</span>
                    </div>
                </div>
            )}

            {/* TPA SCHEDULING CARD */}
            <div className="rounded-2xl bg-card p-5 shadow-md">
                <h2 className="mb-4 text-base font-medium tracking-wide text-text/90 uppercase">{t('tpa.auto')}</h2>

                <div className="flex flex-col gap-4">
                    <div className="flex items-center gap-3 bg-white/5 rounded-md p-3 border border-muted/20">
                        <input
                            type="checkbox"
                            checked={autoEnabled}
                            onChange={(e) => setAutoEnabled(e.target.checked)}
                            className="w-5 h-5 accent-accent"
                        />
                        <div className="flex flex-col">
                            <span className="text-sm font-bold text-text">{t('tpa.autoEnabled')}</span>
                            <span className="text-[10px] text-muted">{t('tpa.autoEnabledHint')}</span>
                        </div>
                    </div>

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
                            {t('tpa.scheduled')} <strong className="text-accent">{autoEnabled ? `${pct}% a cada ${interval} dias às ${h.padStart(2, '0')}:${m.padStart(2, '0')}` : t('tpa.disabledLabel')}</strong>
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
                        {!status.sensorFullDistanceCm ? <li>{t('tpa.sensorMissing')}</li> : null}
                        {!status.drainFlowRate ? <li>{t('tpa.drainPumpMissing')}</li> : null}
                        {!status.refillFlowRate ? <li>{t('tpa.refillPumpMissing')}</li> : null}
                        {!status.reservoirSafetyML ? <li>{t('tpa.safetyMlMissing')}</li> : null}
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

                    <div className="flex flex-col gap-4">
                        <div className="flex flex-col gap-2">
                            <label className="text-xs font-bold text-muted uppercase tracking-wider">{t('tpa.pumpControl')} - {t('tpa.testDrain')}</label>
                            <div className="flex gap-2">
                                <input
                                    type="number" min="0" step="1" placeholder={t('tpa.goalLiters')}
                                    className="w-24 rounded-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm text-text outline-none transition-colors focus:border-accent"
                                    value={drainGoal} onChange={e => setDrainGoal(e.target.value)}
                                />
                                <button
                                    onClick={() => handlePump('drain', 1, parseFloat(drainGoal) || 0)}
                                    className="flex-1 rounded-md bg-accent/20 px-4 py-2 text-[10px] font-bold uppercase tracking-wider text-accent transition hover:bg-accent/30 active:scale-95"
                                >
                                    ▶ ON
                                </button>
                                <button
                                    onClick={() => handlePump('drain', 0)}
                                    className="flex-1 rounded-md bg-danger/20 px-4 py-2 text-[10px] font-bold uppercase tracking-wider text-danger transition hover:bg-danger/30 active:scale-95"
                                >
                                    ⏹ OFF
                                </button>
                            </div>
                        </div>

                        <div className="flex flex-col gap-2">
                            <label className="text-xs font-bold text-muted uppercase tracking-wider">{t('tpa.pumpControl')} - {t('tpa.testRefill')}</label>
                            <div className="flex gap-2">
                                <input
                                    type="number" min="0" step="1" placeholder={t('tpa.goalLiters')}
                                    className="w-24 rounded-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm text-text outline-none transition-colors focus:border-accent"
                                    value={refillGoal} onChange={e => setRefillGoal(e.target.value)}
                                />
                                <button
                                    onClick={() => handlePump('refill', 1, parseFloat(refillGoal) || 0)}
                                    className="flex-1 rounded-md bg-accent2/20 px-4 py-2 text-[10px] font-bold uppercase tracking-wider text-accent2 transition hover:bg-accent2/30 active:scale-95"
                                >
                                    ▶ ON
                                </button>
                                <button
                                    onClick={() => handlePump('refill', 0)}
                                    className="flex-1 rounded-md bg-danger/20 px-4 py-2 text-[10px] font-bold uppercase tracking-wider text-danger transition hover:bg-danger/30 active:scale-95"
                                >
                                    ⏹ OFF
                                </button>
                            </div>
                        </div>
                    </div>

                    <div className="flex flex-col gap-2">
                        <label className="text-xs font-bold text-muted uppercase tracking-wider">{t('tpa.primeDose')}</label>
                        <div className="flex items-center justify-between rounded-md border border-muted/20 bg-white/5 px-4 py-3">
                            <span className="text-sm font-semibold text-white">{status?.primeML ? `${status.primeML.toFixed(1)} mL` : t('tpa.configInConfigTab')}</span>
                            <span className="text-[10px] text-muted italic ml-2">{t('tpa.autoCalc')}</span>
                        </div>
                    </div>

                    <div className="flex items-center gap-3 bg-white/5 rounded-md p-3 border border-muted/20">
                        <input
                            type="checkbox"
                            checked={primeEnabled}
                            onChange={(e) => {
                                setPrimeEnabled(e.target.checked);
                                api('POST', '/api/config/aquarium', { primeEnabled: e.target.checked ? 1 : 0 });
                            }}
                            className="w-5 h-5 accent-accent2"
                        />
                        <div className="flex flex-col">
                            <span className="text-sm font-bold text-text">{t('tpa.primeEnabled')}</span>
                            <span className="text-[10px] text-muted">{t('tpa.primeEnabledHint')}</span>
                        </div>
                    </div>

                    <div className="flex gap-2 mt-2">
                        <button
                            onClick={handleSaveConfig}
                            className="flex-1 rounded-full bg-accent2 px-6 py-2.5 text-sm font-bold uppercase tracking-wider text-black shadow-md transition-all hover:bg-teal-300 active:scale-95"
                        >
                            {t('tpa.saveConfig')}
                        </button>
                        <button
                            onClick={() => handlePump('solenoid', 1)}
                            className="flex-1 rounded-full bg-blue-500/20 px-6 py-2.5 text-sm font-bold uppercase tracking-wider text-blue-400 border border-blue-500/30 shadow-md transition-all hover:bg-blue-500/30 active:scale-95"
                        >
                            {t('tpa.fillReservoir')}
                        </button>
                    </div>
                    <div className="flex gap-2 mt-2">
                        <button
                            onClick={() => api('POST', '/api/canister/toggle')}
                            className={`flex-1 rounded-full px-6 py-2.5 text-sm font-bold uppercase tracking-wider shadow-md transition-all active:scale-95 border ${status?.canister ? 'bg-danger/20 text-danger border-danger/30 hover:bg-danger/30' : 'bg-good/20 text-good border-good/30 hover:bg-good/30'}`}
                        >
                            {status?.canister ? 'Desligar Canister' : 'Ligar Canister'}
                        </button>
                    </div>
                    {status?.tpaState !== 'IDLE' && status?.tpaState !== 'COMPLETE' && (
                        <div className="mt-4 flex flex-col gap-2">
                            <button
                                onClick={() => api('POST', '/api/tpa/abort')}
                                className="rounded-xl bg-danger px-4 py-3 text-sm font-bold uppercase tracking-wider text-white transition hover:bg-red-600 active:scale-95 shadow-md border border-red-500/50"
                            >
                                {t('tpa.abort')}
                            </button>
                        </div>
                    )}

                </div>
            </div>

            {/* FLOW RATES CARD */}
            <div className="rounded-2xl bg-card p-5 shadow-md">
                <h2 className="mb-4 text-base font-medium tracking-wide text-text/90 uppercase">{t('tpa.flowRates')}</h2>
                <div className="flex flex-col gap-4">
                    <div className="flex justify-between items-center bg-white/5 p-3 rounded-lg border border-white/10">
                        <span className="text-sm font-bold text-muted uppercase">{t('tpa.drainPump')}</span>
                        <span className="font-mono text-accent font-bold">
                            {status?.drainFlowRate ? `${status.drainFlowRate.toFixed(2)} mL/s` : '--'}
                        </span>
                    </div>
                    <div className="flex justify-between items-center bg-white/5 p-3 rounded-lg border border-white/10">
                        <span className="text-sm font-bold text-muted uppercase">{t('tpa.refillPump')}</span>
                        <span className="font-mono text-accent2 font-bold">
                            {status?.refillFlowRate ? `${status.refillFlowRate.toFixed(2)} mL/s` : '--'}
                        </span>
                    </div>
                    <span className="text-[10px] text-muted italic text-center mt-2">{t('tpa.autoCalibrated')}</span>
                </div>
            </div>

            {/* PRIME (CH5) CONFIGURATION */}
            {primeEnabled && status?.stocks && status.stocks.length >= 5 && (
                <FertCard index={4} s={status.stocks[4]} onConfig={() => setShowPrimeConfig(true)} />
            )}

            {/* Prime Config Modal */}
            {showPrimeConfig && status?.stocks && status.stocks.length >= 5 && (
                <FertConfigModal
                    index={4}
                    s={status.stocks[4]}
                    onClose={() => setShowPrimeConfig(false)}
                />
            )}

        </div>
    );
}
