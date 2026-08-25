import { useState, useEffect } from 'react';
import { type AQStatus } from '../App';
import { api } from '../api';
import { useT } from '../i18n';

type Props = {
    index: number;
    s: AQStatus['stocks'][0];
    onClose: () => void;
};

export default function FertConfigModal({ index, s, onClose }: Props) {
    const { t } = useT();

    // Schedule States — per day
    const [doses, setDoses] = useState<string[]>(Array(7).fill('0'));
    const [hours, setHours] = useState<string[]>(Array(7).fill('8'));
    const [mins, setMins] = useState<string[]>(Array(7).fill('0'));

    // Calibration States
    const [calibMl, setCalibMl] = useState('');
    const [pwm, setPwm] = useState(s.pwm !== undefined ? s.pwm : 255);
    const [enabled, setEnabled] = useState(s.en !== undefined ? s.en : true);

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
    }, [s.pwm, s.en]);

    const handleSave = () => {
        api('POST', '/api/fert/schedule', {
            channel: index,
            doses: doses.map(Number),
            hours: hours.map(Number),
            minutes: mins.map(Number),
        });
    };

    const handlePwm = () => api('POST', '/api/fert/pwm', { channel: index, pwm });
    const handlePump = (st: number) => api('POST', '/api/fert/pump', { channel: index, state: st });

    const handleRun3s = () => {
        if (confirm(t('fert.confirmRun3s', { ch: index + 1 }))) {
            api('POST', '/api/fert/run3s', { channel: index });
        }
    };

    const handleToggleEnabled = () => {
        const nextState = !enabled;
        setEnabled(nextState);
        api('POST', '/api/fert/enable', { channel: index, enabled: nextState ? 1 : 0 });
    };

    const handleSaveCalib = () => {
        if (+calibMl > 0) {
            if (confirm(t('fert.confirmCalib', { ml: calibMl, ch: index + 1 }))) {
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
            {/* Overlay */}
            <div className="absolute inset-0 bg-black/70 backdrop-blur-sm" />

            {/* Modal */}
            <div className="animate-slide-up relative z-10 max-h-[92vh] w-full max-w-lg overflow-y-auto rounded-t-3xl bg-card shadow-2xl sm:rounded-2xl">
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

                <div className="flex flex-col gap-6 p-4">
                    {/* SCHEDULE — one row per day, thumb sized */}
                    <section>
                        <div className="card-h">
                            <h3 className="card-t">{t('fert.schedule')}</h3>
                            <span className="pill bg-white/5 text-muted">
                                {weekTotal.toFixed(1)} mL{s.fR > 0 ? ` · ${(weekTotal / s.fR).toFixed(1)}s` : ''} / {t('fert.totalWeek')}
                            </span>
                        </div>

                        <div className="mb-4 flex flex-col">
                            {shortDays.map((day, i) => {
                                const doseVal = Number(doses[i]);
                                const estimatedSecs = s.fR > 0 ? (doseVal / s.fR).toFixed(1) : '0';
                                const hasActiveDose = doseVal > 0;
                                return (
                                    <div key={i} className="flex items-center gap-2 border-b border-border/40 py-1.5 last:border-0">
                                        <span className={`w-7 flex-none text-xs font-bold ${hasActiveDose ? 'text-accent' : 'text-muted'}`}>{day}</span>

                                        <input
                                            type="number" step="0.5" min="0" max="100"
                                            aria-label={`${day} mL`}
                                            className="inp remove-arrow h-10 w-16 flex-none px-1 text-center"
                                            value={doses[i]}
                                            onChange={(e) => {
                                                const nd = [...doses];
                                                nd[i] = e.target.value;
                                                setDoses(nd);
                                            }}
                                        />
                                        <span className="flex-none text-[10px] text-muted">mL</span>

                                        {hasActiveDose ? (
                                            <div className="flex flex-none items-center gap-0.5">
                                                <input
                                                    type="number" min="0" max="23" placeholder="H"
                                                    aria-label={`${day} ${t('fert.hour')}`}
                                                    className="inp remove-arrow h-10 w-12 border-accent/40 bg-accent/5 px-1 text-center text-accent"
                                                    value={hours[i]}
                                                    onChange={(e) => { const nh = [...hours]; nh[i] = e.target.value; setHours(nh); }}
                                                />
                                                <span className="text-xs font-bold text-muted">:</span>
                                                <input
                                                    type="number" min="0" max="59" placeholder="M"
                                                    aria-label={`${day} ${t('fert.min')}`}
                                                    className="inp remove-arrow h-10 w-12 border-accent/40 bg-accent/5 px-1 text-center text-accent"
                                                    value={mins[i]}
                                                    onChange={(e) => { const nm = [...mins]; nm[i] = e.target.value; setMins(nm); }}
                                                />
                                            </div>
                                        ) : (
                                            <span className="flex-1" />
                                        )}

                                        <span className="ml-auto w-10 flex-none text-right text-[11px] font-bold tabular-nums text-accent">
                                            {hasActiveDose ? `${estimatedSecs}s` : '–'}
                                        </span>
                                    </div>
                                );
                            })}
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
                                    className="inp remove-arrow flex-1"
                                    value={calibMl} onChange={(e) => setCalibMl(e.target.value)}
                                />
                                <button onClick={handleSaveCalib} className="btn btn-w flex-none">
                                    {t('fert.calculate')}
                                </button>
                            </div>
                        </div>
                    </section>
                </div>
            </div>
        </div>
    );
}
