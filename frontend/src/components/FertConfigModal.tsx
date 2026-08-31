import { useState, useEffect } from 'react';
import { type AQStatus } from '../App';
import { api } from '../api';
import { useT } from '../i18n';
import { useConfirm } from '../Confirm';

type Props = {
    index: number;
    s: AQStatus['stocks'][0];
    /** The controller's own clock, "YYYY/MM/DD HH:MM:SS". Its day is what marks
     *  the row, not the phone's: the two can disagree, and the row that matters
     *  is the one the controller will read. */
    time?: string;
    onClose: () => void;
};

/** Day of week of the controller's clock, 0=Sun..6=Sat, -1 if unreadable.
 *  Slashes rather than dashes, so it parses as local time instead of UTC and
 *  the day does not shift for a browser in another timezone. */
const dowOf = (time?: string): number => {
    if (!time) return -1;
    const d = new Date(time);
    return isNaN(d.getTime()) ? -1 : d.getDay();
};

export default function FertConfigModal({ index, s, time, onClose }: Props) {
    const today = dowOf(time);
    const { t } = useT();
    const { ask, dialog } = useConfirm();

    // Schedule States — per day
    const [doses, setDoses] = useState<string[]>(Array(7).fill('0'));
    const [hours, setHours] = useState<string[]>(Array(7).fill('9'));
    const [mins, setMins] = useState<string[]>(Array(7).fill('0'));

    // Calibration States
    const [calibMl, setCalibMl] = useState('');
    const [pwm, setPwm] = useState(s.pwm !== undefined ? s.pwm : 255);
    const [cap, setCap] = useState(s.cap ? String(s.cap) : '');
    const [enabled, setEnabled] = useState(s.en !== undefined ? s.en : true);
    const [resetDone, setResetDone] = useState(false);

    const shortDays = t('fert.shortDays').split(',');

    // Init doses from server (only once)
    const [doseInit, setDoseInit] = useState(false);
    useEffect(() => {
        if (!doseInit && s.doses) {
            setDoses(s.doses.map(d => d.toString()));
            setDoseInit(true);
        }
    }, [s.doses, doseInit]);

    // Init per-day times from server (only once)
    const [timeInit, setTimeInit] = useState(false);
    useEffect(() => {
        if (!timeInit && s.sH && Array.isArray(s.sH) && s.sM && Array.isArray(s.sM)) {
            setHours(s.sH.map(h => h.toString()));
            setMins(s.sM.map(m => m.toString()));
            setTimeInit(true);
        }
    }, [s.sH, s.sM, timeInit]);

    useEffect(() => {
        if (s.pwm !== undefined) setPwm(s.pwm);
        if (s.en !== undefined) setEnabled(s.en);
        if (s.cap) setCap(String(s.cap));
    }, [s.pwm, s.en, s.cap]);

    const handleSave = () => {
        api('POST', '/api/fert/schedule', {
            channel: index,
            doses: doses.map(Number),
            hours: hours.map(Number),
            minutes: mins.map(Number),
            // Zero is refused by the firmware, so an emptied box leaves the
            // stored size alone instead of wiping it.
            capacityML: parseFloat(cap) || 0,
        });
    };

    const handlePwm = () => api('POST', '/api/fert/pwm', { channel: index, pwm });
    const handlePump = (st: number) => api('POST', '/api/fert/pump', { channel: index, state: st });

    const handleRun3s = async () => {
        if (await ask(t('fert.confirmRun3s', { ch: index + 1 }))) {
            api('POST', '/api/fert/run3s', { channel: index });
        }
    };

    const handleToggleEnabled = () => {
        const nextState = !enabled;
        setEnabled(nextState);
        api('POST', '/api/fert/enable', { channel: index, enabled: nextState ? 1 : 0 });
    };

    const handleReset = async () => {
        if (!(await ask(t('fert.confirmReset', { ch: index + 1 })))) return;
        const r = await api('POST', '/api/fert/reset', { channel: index });
        if (!r?.ok) return;
        // The init effects below run once and then latch, so the form has to be
        // put back by hand — otherwise the inputs go on showing the values that
        // were just erased on the controller.
        setDoses(Array(7).fill('0'));
        setHours(Array(7).fill('9'));
        setMins(Array(7).fill('0'));
        setPwm(255);
        setEnabled(true);
        setCalibMl('');
        setResetDone(true);
    };

    const handleSaveCalib = async () => {
        if (+calibMl > 0) {
            if (await ask(t('fert.confirmCalib', { ml: calibMl, ch: index + 1 }))) {
                api('POST', '/api/fert/calibrate', { channel: index, ml: +calibMl });
                setCalibMl('');
            }
        } else alert(t('fert.enterMl'));
    };

    const weekTotal = doses.reduce((a, b) => a + Number(b), 0);

    return (
        <div
            className="fixed inset-0 z-50 flex items-end justify-center sm:items-center"
            onClick={(e) => e.target === e.currentTarget && onClose()}
        >
            {dialog}
            {/* Overlay */}
            <div className="absolute inset-0 bg-black/70 backdrop-blur-sm" />

            {/* Modal */}
            <div className="animate-slide-up sheet relative z-10 w-full max-w-lg overflow-y-auto overscroll-contain rounded-t-3xl bg-card shadow-2xl sm:rounded-2xl">
                {/* Header */}
                <div className="sticky top-0 z-10 flex items-center gap-3 rounded-t-3xl border-b border-border/60 bg-card px-4 py-3 sm:rounded-t-2xl">
                    <div className="min-w-0 flex-1">
                        <span className="lbl">{t('fert.channel')} {index + 1}</span>
                        <div className="mt-0.5 flex items-center gap-3">
                            <span className="truncate text-base font-bold text-text">{s.name || `${t('fert.channel')} ${index + 1}`}</span>
                            {/* Enable Toggle Switch */}
                            <button
                                onClick={handleToggleEnabled}
                                aria-pressed={enabled}
                                className={`inline-flex h-6 w-11 flex-none items-center rounded-full p-0.5 transition-colors ${enabled ? 'bg-accent' : 'bg-white/20'}`}
                                title={enabled ? t('notify.enabled') : t('notify.disabled')}
                            >
                                <span className={`inline-block h-5 w-5 rounded-full bg-white shadow-md transition-transform ${enabled ? 'translate-x-5' : 'translate-x-0'}`} />
                            </button>
                        </div>
                    </div>
                    <button
                        onClick={onClose}
                        aria-label="close"
                        className="flex h-11 w-11 flex-none items-center justify-center rounded-full bg-white/10 text-muted transition hover:bg-white/20 hover:text-text active:scale-90"
                    >
                        ✕
                    </button>
                </div>

                <div className="safe-b flex flex-col gap-6 p-4">
                    {/* SCHEDULE — one row per day, thumb sized */}
                    <section>
                        <div className="card-h">
                            <h3 className="card-t">{t('fert.schedule')}</h3>
                            <span className="pill bg-white/5 text-muted">
                                {weekTotal.toFixed(1)} mL{s.fR > 0 ? ` · ${(weekTotal / s.fR).toFixed(1)}s` : ''} / {t('fert.totalWeek')}
                            </span>
                        </div>

                        {/* Four fixed columns. The old layout was a flex row of
                            fixed-width boxes that added up to more than a 320 px
                            phone has, so the last column fell off the edge. The
                            time inputs take the free column and shrink instead. */}
                        <div className="mb-4 flex flex-col">
                            <div className="grid grid-cols-[2.25rem_3.5rem_minmax(0,1fr)_2.5rem] items-center gap-2 pb-1 text-[10px] font-bold uppercase tracking-wider text-muted">
                                <span />
                                <span className="text-center">mL</span>
                                <span className="text-center">{t('fert.hour')}:{t('fert.min')}</span>
                                <span className="text-right">s</span>
                            </div>

                            {shortDays.map((day, i) => {
                                const doseVal = Number(doses[i]);
                                const estimatedSecs = s.fR > 0 ? (doseVal / s.fR).toFixed(1) : '0';
                                const hasActiveDose = doseVal > 0;
                                const isToday = i === today;
                                return (
                                    <div
                                        key={i}
                                        className={`grid grid-cols-[2.25rem_3.5rem_minmax(0,1fr)_2.5rem] items-center gap-2 border-b border-border/40 py-1.5 last:border-0 ${isToday ? 'rounded-md bg-accent/10' : ''}`}
                                    >
                                        <span className={`text-xs font-bold ${hasActiveDose ? 'text-accent' : 'text-muted'}`}>
                                            {day}
                                            {isToday && <span className="block text-[9px] font-bold uppercase leading-none text-accent2">{t('fert.today')}</span>}
                                        </span>

                                        <input
                                            type="number" step="0.5" min="0" max="100"
                                            inputMode="decimal"
                                            aria-label={`${day} mL`}
                                            className="inp inp-num remove-arrow h-10 w-full px-1 text-center"
                                            value={doses[i]}
                                            onChange={(e) => {
                                                const nd = [...doses];
                                                nd[i] = e.target.value;
                                                setDoses(nd);
                                            }}
                                        />

                                        {hasActiveDose ? (
                                            <div className="flex min-w-0 items-center gap-1">
                                                <input
                                                    type="number" min="0" max="23" placeholder="H"
                                                    inputMode="numeric"
                                                    aria-label={`${day} ${t('fert.hour')}`}
                                                    className="inp inp-num remove-arrow h-10 w-full min-w-0 border-accent/40 bg-accent/5 px-1 text-center text-accent"
                                                    value={hours[i]}
                                                    onChange={(e) => { const nh = [...hours]; nh[i] = e.target.value; setHours(nh); }}
                                                />
                                                <span className="flex-none text-xs font-bold text-muted">:</span>
                                                <input
                                                    type="number" min="0" max="59" placeholder="M"
                                                    inputMode="numeric"
                                                    aria-label={`${day} ${t('fert.min')}`}
                                                    className="inp inp-num remove-arrow h-10 w-full min-w-0 border-accent/40 bg-accent/5 px-1 text-center text-accent"
                                                    value={mins[i]}
                                                    onChange={(e) => { const nm = [...mins]; nm[i] = e.target.value; setMins(nm); }}
                                                />
                                            </div>
                                        ) : (
                                            <span />
                                        )}

                                        <span className="text-right text-[11px] font-bold tabular-nums text-accent">
                                            {hasActiveDose ? `${estimatedSecs}s` : '–'}
                                        </span>
                                    </div>
                                );
                            })}
                        </div>

                        <div className="field mt-3">
                            <label className="lbl">{t('fert.bottleSize')}</label>
                            <input
                                type="number" min="1" max="5000" step="10" placeholder="450"
                                className="inp remove-arrow"
                                value={cap} onChange={e => setCap(e.target.value)}
                            />
                            <span className="hint">{t('fert.bottleSizeHint')}</span>
                        </div>

                        <button onClick={handleSave} className="btn btn-p w-full">
                            {t('fert.save')}
                        </button>
                    </section>

                    {/* CALIBRATION & PWM */}
                    <section className="sub">
                        <div className="card-h">
                            <h3 className="card-t">{t('fert.calibPower')}</h3>
                            <span className="pill bg-white/5 text-muted">{(s.fR || 0).toFixed(2)} mL/s</span>
                        </div>

                        <div className="mb-4 rounded-xl bg-black/25 p-4">
                            <div className="mb-2 flex items-center justify-between">
                                <span className="lbl">{t('fert.power')}</span>
                                <span className="font-mono text-sm font-bold tabular-nums text-accent">{Math.round((pwm / 255) * 100)}%</span>
                            </div>
                            <input
                                type="range" min="0" max="255"
                                aria-label={t('fert.power')}
                                className="mb-3 h-6 w-full cursor-pointer accent-accent"
                                value={pwm}
                                onChange={(e) => setPwm(Number(e.target.value))}
                                onMouseUp={handlePwm} onTouchEnd={handlePwm}
                            />
                            <div className="grid grid-cols-2 gap-2">
                                <button
                                    onMouseDown={() => handlePump(1)} onMouseUp={() => handlePump(0)} onMouseLeave={() => handlePump(0)}
                                    onTouchStart={() => handlePump(1)} onTouchEnd={() => handlePump(0)} onTouchCancel={() => handlePump(0)}
                                    className="btn btn-n text-xs"
                                >
                                    {t('fert.holdPurge')}
                                </button>
                                <button onClick={handleRun3s} className="btn btn-n text-xs">
                                    {t('fert.run3s')}
                                </button>
                            </div>
                        </div>

                        <div className="field">
                            <label className="lbl">{t('fert.measureResult')}</label>
                            <div className="flex gap-2">
                                <input
                                    type="number" step="0.1" min="0" placeholder={t('fert.mlMeasured')}
                                    inputMode="decimal"
                                    className="inp inp-num remove-arrow min-w-0 flex-1"
                                    value={calibMl} onChange={(e) => setCalibMl(e.target.value)}
                                />
                                <button onClick={handleSaveCalib} className="btn btn-w flex-none">
                                    {t('fert.calculate')}
                                </button>
                            </div>
                        </div>
                    </section>

                    {/* DANGER ZONE — wipe this channel's stored configuration */}
                    <section className="sub">
                        <div className="card-h">
                            <h3 className="card-t text-danger">{t('fert.dangerZone')}</h3>
                        </div>
                        <p className="hint mb-3">{t('fert.resetHint')}</p>
                        <button onClick={handleReset} className="btn btn-d w-full">
                            {t('fert.reset')}
                        </button>
                        {resetDone && (
                            <p className="mt-2 text-[11px] font-bold leading-snug text-accent">
                                {t('fert.resetDone', { ch: index + 1 })}
                            </p>
                        )}
                    </section>
                </div>
            </div>
        </div>
    );
}
