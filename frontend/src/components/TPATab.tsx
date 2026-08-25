import { useState, useEffect, useRef } from 'react';
import { type AQStatus } from '../App';
import { api } from '../api';
import { FertCard } from './FertsTab';
import { ConfigChecklist } from './HomeTab';
import FertConfigModal from './FertConfigModal';
import { useT } from '../i18n';
import { useConfirm } from '../Confirm';

/** Mirrors CALIBRATION_MIN_DELTA_PCT in Config.h. */
const CALIBRATION_STEP_PCT = 5;

/* ── Manual pump test row: goal + ON/OFF, all 44px tall ───────── */
/**
 * Manual pump row. The goal is entered as a percentage of the aquarium, which is
 * how the tank is actually reasoned about, and shown in litres so the number
 * stays concrete. It is capped at the reservoir volume: the refill pump cannot
 * deliver more water than the reservoir holds, and draining more than that has
 * nowhere to go on the way back.
 */
function PumpTest({
    label, tone, pct, onPct, maxPct, litersPerPct, trackable, flow, onCalibrate, onStart, onStop,
}: {
    label: string;
    tone: 'btn-a' | 'btn-a2';
    pct: string;
    onPct: (v: string) => void;
    maxPct: number;
    litersPerPct: number;
    /** False when neither the level sensor nor a calibrated flow can track the goal. */
    trackable: boolean;
    /** Measured flow (mL/s). 0 means this pump has never been calibrated. */
    flow: number;
    onCalibrate: () => void;
    onStart: () => void;
    onStop: () => void;
}) {
    const { t } = useT();
    const n = parseFloat(pct);
    const hasGoal = Number.isFinite(n) && n > 0;
    const over = hasGoal && maxPct > 0 && n > maxPct;
    const liters = Number.isFinite(n) ? n * litersPerPct : 0;

    return (
        <div className="field">
            <label className="lbl">{label}</label>
            <div className="flex gap-2">
                <div className="relative w-20 flex-none">
                    <input
                        type="number" min="0" max={maxPct > 0 ? maxPct : undefined} step="1"
                        placeholder="%"
                        className={`inp remove-arrow w-full pr-5 text-center ${over ? 'border-danger text-danger' : ''}`}
                        value={pct} onChange={e => onPct(e.target.value)}
                    />
                    <span className="pointer-events-none absolute right-2 top-1/2 -translate-y-1/2 text-[11px] text-muted">%</span>
                </div>
                <button
                    onClick={onStart}
                    className={`btn ${tone} flex-1`}
                    // An empty field means "no goal": run until OFF is pressed. That path
                    // must stay open, otherwise there is no way to run the pumps needed to
                    // calibrate the very flow rates a goal depends on.
                    disabled={over || (hasGoal && !trackable)}
                >▶ ON</button>
                <button onClick={onStop} className="btn btn-d flex-1">⏹ OFF</button>
            </div>
            {/* Always offered. A stored rate can be wrong as easily as missing —
                and a wrong one is worse, because it sizes the TPA timeouts. */}
            <button onClick={onCalibrate} className="btn btn-w mt-2 w-full">
                {flow > 0 ? t('tpa.recalibrateFlow') : t('tpa.calibrateFlow')}
            </button>
            <p className={`hint ${over || (hasGoal && !trackable) ? 'text-danger' : ''}`}>
                {!hasGoal
                    ? t('tpa.goalFree')
                    : !trackable
                        ? t('tpa.goalNotTrackable')
                        : over
                            ? t('tpa.goalOverMax', { max: maxPct.toFixed(1) })
                            : t('tpa.goalEquals', { liters: liters.toFixed(1) })}
            </p>
        </div>
    );
}

export default function TPATab({ status }: { status: AQStatus | null }) {
    const { t } = useT();
    const { ask, dialog } = useConfirm();
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

    // Manual pump goals are entered as % of the aquarium and sent to the
    // firmware in litres, which is the unit /api/tpa/pump expects.
    const aqVolume = status?.aquariumVolume ?? 0;
    const resVolume = status?.reservoirVolume ?? 0;
    const litersPerPct = aqVolume / 100;
    // The reservoir is the hard ceiling in both directions: the refill pump has
    // nothing else to draw from, and water drained beyond it cannot be returned.
    const maxPct = aqVolume > 0 && resVolume > 0
        ? Math.min(100, (resVolume / aqVolume) * 100)
        : 0;
    // The firmware refuses a goal it cannot measure: it needs either a live
    // level sensor (with litersPerCm known) or a calibrated flow for that pump.
    const sensorTrackable = aqVolume > 0 && (status?.litersPerCm ?? 0) > 0 && (status?.waterLevel ?? 0) > 0;
    const drainTrackable = sensorTrackable || (status?.drainFlowRate ?? 0) > 0;
    const refillTrackable = sensorTrackable || (status?.refillFlowRate ?? 0) > 0;

    const handleCalibrate = async (pump: 'drain' | 'refill') => {
        const label = t(pump === 'drain' ? 'tpa.testDrain' : 'tpa.testRefill');
        if (await ask(t('confirm.calibrateFlow', { pump: label }))) {
            api('POST', '/api/tpa/calibrate-pump', { pump });
        }
    };

    const handleCalibrateBoth = async () => {
        if (await ask(t('confirm.calibrateBoth', { pct: CALIBRATION_STEP_PCT }))) {
            api('POST', '/api/tpa/calibrate-pumps');
        }
    };

    const pctToLiters = (v: string) => {
        const n = parseFloat(v);
        if (!Number.isFinite(n) || n <= 0) return 0;
        return Math.min(n, maxPct) * litersPerPct;
    };

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

    const handlePump = async (pump: 'drain' | 'refill' | 'solenoid', state: number, liters?: number) => {
        // Only turning something ON needs a confirmation: stopping is always safe.
        if (state === 1) {
            const key =
                pump === 'drain' ? 'confirm.pumpDrain'
                    : pump === 'refill' ? 'confirm.pumpRefill'
                        : 'confirm.solenoid';
            if (!(await ask(t(key)))) return;
        }
        api('POST', '/api/tpa/pump', { pump, state, liters });
    };

    const handleCanisterToggle = async () => {
        // status.canister is true while the filter is running.
        if (status?.canister && !(await ask(t('confirm.canisterOff')))) return;
        api('POST', '/api/canister/toggle');
    };

    const busy = !!status && status.tpaState !== 'IDLE' && status.tpaState !== 'COMPLETE';

    return (
        <div className="flex flex-col gap-3">
            {dialog}
            {/* LIVE PUMP PROGRESS */}
            {status && status.pumpGoalLiters !== undefined && status.pumpGoalLiters > 0 && (
                <section className="card border-l-4 border-accent">
                    <div className="card-h">
                        <h2 className="card-t text-accent">{t('tpa.pumpProgress')}</h2>
                        <span className="font-mono text-xs tabular-nums text-muted">
                            {t('tpa.pumpTime')} {Math.floor((status.pumpElapsedMs || 0) / 60000).toString().padStart(2, '0')}:{Math.floor(((status.pumpElapsedMs || 0) / 1000) % 60).toString().padStart(2, '0')}
                        </span>
                    </div>
                    <div className="h-4 w-full overflow-hidden rounded-full bg-white/10">
                        <div
                            className="h-full rounded-full bg-accent transition-all duration-1000 ease-linear"
                            style={{ width: `${Math.min(100, Math.max(0, ((status.pumpProgressLiters || 0) / status.pumpGoalLiters) * 100))}%` }}
                        />
                    </div>
                    <div className="mt-1 flex justify-between px-1 text-[11px] font-bold tabular-nums text-muted">
                        {/* Percentage first: the goal is entered as a percentage of the
                            aquarium, so that is the number to read it back against. */}
                        <span className="text-text">
                            {Math.min(100, Math.round(((status.pumpProgressLiters || 0) / status.pumpGoalLiters) * 100))}%
                        </span>
                        <span>
                            {(status.pumpProgressLiters || 0).toFixed(1)} / {status.pumpGoalLiters.toFixed(1)} L
                        </span>
                    </div>
                </section>
            )}

            {/* WHAT IS MISSING BEFORE A TPA CAN RUN */}
            <ConfigChecklist status={status} />

            {/* MANUAL CONTROLS — the things you press while standing at the tank */}
            <section className="card">
                <div className="card-h">
                    <h2 className="card-t">{t('tpa.manual')}</h2>
                    <span className={`pill ${busy ? 'bg-accent/20 text-accent' : 'bg-white/5 text-muted'}`}>
                        {status ? status.tpaState : '--'}
                    </span>
                </div>

                <div className="flex flex-col gap-3">
                    <div className="grid grid-cols-2 gap-2">
                        {status?.tpaState === 'MANUAL_RESERVOIR_FILL' ? (
                            <button onClick={() => handlePump('solenoid', 0)} className="btn btn-d">
                                {t('tpa.stopFill')}
                            </button>
                        ) : (
                            <button onClick={() => handlePump('solenoid', 1)} className="btn btn-a">
                                {t('tpa.fillReservoir')}
                            </button>
                        )}
                        <button
                            onClick={handleCanisterToggle}
                            className={`btn ${status?.canister ? 'btn-d' : 'btn-g'}`}
                        >
                            {status?.canister ? t('tpa.canisterOff') : t('tpa.canisterOn')}
                        </button>
                    </div>

                    {busy && (
                        <button
                            onClick={async () => { if (await ask(t('confirm.tpaAbort'))) api('POST', '/api/tpa/abort'); }}
                            className="btn btn-dd w-full"
                        >
                            {t('tpa.abort')}
                        </button>
                    )}

                    <div className="sub flex flex-col gap-4">
                        <h3 className="sub-t -mb-1">{t('tpa.pumpControl')}</h3>
                        <PumpTest
                            label={t('tpa.testDrain')} tone="btn-a"
                            pct={drainGoal} onPct={setDrainGoal}
                            maxPct={maxPct} litersPerPct={litersPerPct} trackable={drainTrackable}
                            flow={status?.drainFlowRate ?? 0} onCalibrate={() => handleCalibrate('drain')}
                            onStart={() => handlePump('drain', 1, pctToLiters(drainGoal))}
                            onStop={() => handlePump('drain', 0)}
                        />
                        <PumpTest
                            label={t('tpa.testRefill')} tone="btn-a2"
                            pct={refillGoal} onPct={setRefillGoal}
                            maxPct={maxPct} litersPerPct={litersPerPct} trackable={refillTrackable}
                            flow={status?.refillFlowRate ?? 0} onCalibrate={() => handleCalibrate('refill')}
                            onStart={() => handlePump('refill', 1, pctToLiters(refillGoal))}
                            onStop={() => handlePump('refill', 0)}
                        />
                        <p className="hint">
                            {maxPct > 0
                                ? t('tpa.goalHint', { max: maxPct.toFixed(1), res: resVolume.toFixed(0), aq: aqVolume.toFixed(0) })
                                : t('tpa.goalHintNoConfig')}
                        </p>
                    </div>
                </div>
            </section>

            {/* AUTOMATIC SCHEDULE */}
            <section className="card">
                <div className="card-h">
                    <h2 className="card-t">{t('tpa.auto')}</h2>
                    <span className={`pill ${autoEnabled ? 'bg-accent2/20 text-accent2' : 'bg-white/5 text-muted'}`}>
                        {autoEnabled ? t('notify.enabled') : t('notify.disabled')}
                    </span>
                </div>

                <div className="flex flex-col gap-4">
                    <label className="flex min-h-[56px] cursor-pointer items-center gap-3 rounded-xl border border-border bg-white/5 px-3 py-2">
                        <input
                            type="checkbox"
                            checked={autoEnabled}
                            onChange={(e) => setAutoEnabled(e.target.checked)}
                            className="h-5 w-5 flex-none accent-accent"
                        />
                        <span className="min-w-0">
                            <span className="block text-sm font-bold text-text">{t('tpa.autoEnabled')}</span>
                            <span className="hint block">{t('tpa.autoEnabledHint')}</span>
                        </span>
                    </label>

                    <div className="field">
                        <label className="lbl">{t('tpa.frequency')}</label>
                        <input
                            type="number" min="0" max="90" placeholder={t('tpa.disabled')}
                            className="inp remove-arrow"
                            value={interval} onChange={e => setInterval(e.target.value)}
                        />
                        <span className="hint">{t('tpa.freqHint')}</span>
                    </div>

                    <div className="field">
                        <label className="lbl">{t('tpa.volumePct')}</label>
                        <input
                            type="number" min="1" max="100" step="1" placeholder="20"
                            className="inp remove-arrow"
                            value={pct} onChange={e => setPct(e.target.value)}
                        />
                        {status?.aquariumVolume ? (
                            <span className="hint">
                                = <strong className="text-accent">{(status.aquariumVolume * (parseInt(pct) || 0) / 100).toFixed(1)} L</strong> {t('tpa.ofTotal', { v: status.aquariumVolume })}
                            </span>
                        ) : (
                            <span className="hint">{t('tpa.configDimHint')}</span>
                        )}
                    </div>

                    <div className="grid grid-cols-2 gap-3">
                        <div className="field">
                            <label className="lbl">{t('tpa.hour')}</label>
                            <input
                                type="number" min="0" max="23" placeholder="HH"
                                className="inp remove-arrow text-center"
                                value={h} onChange={e => setH(e.target.value)}
                            />
                        </div>
                        <div className="field">
                            <label className="lbl">{t('tpa.minute')}</label>
                            <input
                                type="number" min="0" max="59" placeholder="MM"
                                className="inp remove-arrow text-center"
                                value={m} onChange={e => setM(e.target.value)}
                            />
                        </div>
                    </div>

                    <div className="rounded-xl bg-white/5 px-4 py-3">
                        <span className="lbl">{t('tpa.scheduled')}</span>
                        <p className="mt-0.5 text-sm font-bold text-accent">
                            {autoEnabled
                                ? t('tpa.summary', { p: pct || '0', n: interval || '0', t: `${h.padStart(2, '0')}:${m.padStart(2, '0')}` })
                                : t('tpa.disabledLabel')}
                        </p>
                    </div>

                    <button onClick={handleSaveSchedule} className="btn btn-p w-full">
                        {t('tpa.saveSchedule')}
                    </button>
                </div>
            </section>

            {/* RESERVOIR & PRIME */}
            <section className="card">
                <div className="card-h">
                    <h2 className="card-t">{t('tpa.reservoir')}</h2>
                </div>

                <div className="flex flex-col gap-4">
                    <div className="field">
                        <label className="lbl">{t('tpa.safetyMargin')}</label>
                        <input
                            type="number" min="0" max="99999" step="100" placeholder="500"
                            className="inp remove-arrow"
                            value={safetyML} onChange={e => setSafetyML(e.target.value)}
                        />
                        <span className="hint">{t('tpa.safetyHint')}</span>
                    </div>

                    <div className="field">
                        <label className="lbl">{t('tpa.primeDose')}</label>
                        <div className="flex min-h-[44px] flex-wrap items-center justify-between gap-2 rounded-lg border border-border bg-white/5 px-3 py-2">
                            <span className="text-sm font-bold text-text">
                                {status?.primeML ? `${status.primeML.toFixed(1)} mL` : t('tpa.configInConfigTab')}
                            </span>
                            <span className="hint">{t('tpa.autoCalc')}</span>
                        </div>
                    </div>

                    <label className="flex min-h-[56px] cursor-pointer items-center gap-3 rounded-xl border border-border bg-white/5 px-3 py-2">
                        <input
                            type="checkbox"
                            checked={primeEnabled}
                            onChange={(e) => {
                                setPrimeEnabled(e.target.checked);
                                api('POST', '/api/config/aquarium', { primeEnabled: e.target.checked ? 1 : 0 });
                            }}
                            className="h-5 w-5 flex-none accent-accent2"
                        />
                        <span className="min-w-0">
                            <span className="block text-sm font-bold text-text">{t('tpa.primeEnabled')}</span>
                            <span className="hint block">{t('tpa.primeEnabledHint')}</span>
                        </span>
                    </label>

                    <button onClick={handleSaveConfig} className="btn btn-p2 w-full">
                        {t('tpa.saveConfig')}
                    </button>
                </div>
            </section>

            {/* FLOW RATES */}
            <section className="card">
                <div className="card-h">
                    <h2 className="card-t">{t('tpa.flowRates')}</h2>
                </div>
                <div className="flex flex-col">
                    <div className="row">
                        <span className="row-k">{t('tpa.drainPump')}</span>
                        <span className="row-v text-accent">
                            {status?.drainFlowRate ? `${status.drainFlowRate.toFixed(2)} mL/s` : '--'}
                        </span>
                    </div>
                    <div className="row">
                        <span className="row-k">{t('tpa.refillPump')}</span>
                        <span className="row-v text-accent2">
                            {status?.refillFlowRate ? `${status.refillFlowRate.toFixed(2)} mL/s` : '--'}
                        </span>
                    </div>
                </div>
                <button onClick={handleCalibrateBoth} className="btn btn-p2 mt-3 w-full">
                    {t('tpa.calibrateBoth')}
                </button>
                <p className="hint">{t('tpa.calibrateBothHint')}</p>

                <p className="hint mt-3">{t('tpa.autoCalibrated')}</p>
            </section>

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
