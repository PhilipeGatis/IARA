import { useState, useEffect } from 'react';
import { api, type AQStatus } from '../App';
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

    return (
        <div
            className="fixed inset-0 z-50 flex items-end sm:items-center justify-center"
            onClick={(e) => e.target === e.currentTarget && onClose()}
        >
            {/* Overlay */}
            <div className="absolute inset-0 bg-black/60 backdrop-blur-sm" />

            {/* Modal */}
            <div className="relative z-10 w-full max-w-lg max-h-[90vh] overflow-y-auto rounded-t-3xl sm:rounded-2xl bg-card shadow-2xl animate-slide-up">
                {/* Header */}
                <div className="sticky top-0 z-10 flex items-center justify-between rounded-t-3xl sm:rounded-t-2xl bg-card border-b border-white/10 px-5 py-4">
                    <div className="flex flex-col flex-1">
                        <span className="text-xs font-bold text-muted uppercase tracking-wider">{t('fert.channel')} {index + 1}</span>
                        <div className="flex items-center gap-3 mt-1">
                            <span className="text-base font-bold text-white">{s.name || `Canal ${index + 1}`}</span>
                            {/* Enable Toggle Switch */}
                            <button
                                onClick={handleToggleEnabled}
                                className={`inline-flex items-center h-5 w-9 rounded-full p-0.5 transition-colors ${enabled ? 'bg-accent' : 'bg-white/20'}`}
                                title={enabled ? t('notify.enabled') : t('notify.disabled')}
                            >
                                <span className={`inline-block h-4 w-4 rounded-full bg-white shadow-md transform transition-transform ${enabled ? 'translate-x-4' : 'translate-x-0'}`} />
                            </button>
                        </div>
                    </div>
                    <button
                        onClick={onClose}
                        className="flex h-8 w-8 items-center justify-center rounded-full bg-white/10 text-muted transition hover:bg-white/20 hover:text-white active:scale-90"
                    >
                        ✕
                    </button>
                </div>

                <div className="p-5 flex flex-col gap-6">
                    {/* SCHEDULE — per day doses + times */}
                    <section>
                        <h3 className="mb-3 flex justify-between items-center text-xs font-bold uppercase tracking-wider text-muted">
                            <span>{t('fert.schedule')}</span>
                            {s.fR > 0 && <span>{(doses.reduce((a, b) => a + Number(b), 0) / s.fR).toFixed(1)}s {t('fert.totalWeek')}</span>}
                        </h3>

                        <div className="mb-4 grid grid-cols-7 gap-1">
                            {shortDays.map((day, i) => {
                                const doseVal = Number(doses[i]);
                                const estimatedSecs = s.fR > 0 ? (doseVal / s.fR).toFixed(1) : '0';
                                const hasActiveDose = doseVal > 0;
                                return (
                                    <div key={i} className="flex flex-col items-center gap-1">
                                        <span className={`text-[10px] font-bold ${hasActiveDose ? 'text-accent' : 'text-muted'}`}>{day}</span>
                                        {/* Dose input */}
                                        <input
                                            type="number" step="0.5" min="0" max="100"
                                            className="w-full min-w-0 rounded-t-sm border-b border-muted bg-white/5 p-1 text-center text-[11px] font-medium text-text outline-none transition-colors focus:border-accent remove-arrow"
                                            style={{ MozAppearance: 'textfield' }}
                                            value={doses[i]}
                                            onChange={(e) => {
                                                const nd = [...doses];
                                                nd[i] = e.target.value;
                                                setDoses(nd);
                                            }}
                                        />
                                        <span className="text-[9px] text-accent font-bold tracking-tighter">
                                            {hasActiveDose ? `${estimatedSecs}s` : '-'}
                                        </span>
                                        {/* Time input — only if dose > 0 */}
                                        {hasActiveDose && (
                                            <div className="flex flex-col items-center gap-0.5 w-full">
                                                <input
                                                    type="number" min="0" max="23"
                                                    className="w-full min-w-0 rounded-sm border-b border-accent/30 bg-accent/5 p-0.5 text-center text-[10px] font-medium text-accent outline-none transition-colors focus:border-accent remove-arrow"
                                                    style={{ MozAppearance: 'textfield' }}
                                                    value={hours[i]}
                                                    onChange={(e) => { const nh = [...hours]; nh[i] = e.target.value; setHours(nh); }}
                                                    placeholder="H"
                                                />
                                                <input
                                                    type="number" min="0" max="59"
                                                    className="w-full min-w-0 rounded-sm border-b border-accent/30 bg-accent/5 p-0.5 text-center text-[10px] font-medium text-accent outline-none transition-colors focus:border-accent remove-arrow"
                                                    style={{ MozAppearance: 'textfield' }}
                                                    value={mins[i]}
                                                    onChange={(e) => { const nm = [...mins]; nm[i] = e.target.value; setMins(nm); }}
                                                    placeholder="M"
                                                />
                                            </div>
                                        )}
                                    </div>
                                );
                            })}
                        </div>

                        <button
                            onClick={handleSave}
                            className="w-full rounded-full bg-accent px-4 py-2.5 text-xs font-bold uppercase tracking-wider text-black shadow-md transition-all hover:bg-blue-300 active:scale-95"
                        >
                            {t('fert.save')}
                        </button>
                    </section>

                    {/* CALIBRATION & PWM */}
                    <section>
                        <h3 className="mb-3 text-xs font-bold uppercase tracking-wider text-muted">
                            {t('fert.calibPower')} ({(s.fR || 0).toFixed(2)} mL/s)
                        </h3>

                        <div className="mb-5 rounded-xl bg-black/20 p-4">
                            <div className="mb-2 flex items-center justify-between text-xs font-bold text-muted">
                                <span className="tracking-wider">{t('fert.power')}</span>
                                <span className="text-accent">{Math.round((pwm / 255) * 100)}%</span>
                            </div>
                            <input
                                type="range" min="0" max="255"
                                className="w-full accent-accent cursor-pointer mb-2"
                                value={pwm}
                                onChange={(e) => setPwm(Number(e.target.value))}
                                onMouseUp={handlePwm} onTouchEnd={handlePwm}
                            />
                            <div className="flex gap-2">
                                <button
                                    onMouseDown={() => handlePump(1)} onMouseUp={() => handlePump(0)} onMouseLeave={() => handlePump(0)}
                                    onTouchStart={() => handlePump(1)} onTouchEnd={() => handlePump(0)} onTouchCancel={() => handlePump(0)}
                                    className="flex-1 rounded-full border border-muted bg-transparent py-2 text-[10px] font-bold uppercase tracking-wider text-muted transition hover:bg-white/5 active:bg-white/10 select-none"
                                >
                                    {t('fert.holdPurge')}
                                </button>
                                <button
                                    onClick={handleRun3s}
                                    className="flex-1 rounded-full border border-muted bg-transparent py-2 text-[10px] font-bold uppercase tracking-wider text-muted transition hover:bg-white/5 active:bg-white/10 select-none"
                                >
                                    {t('fert.run3s')}
                                </button>
                            </div>
                        </div>

                        <div className="flex flex-col gap-1">
                            <label className="text-[10px] font-bold text-muted uppercase tracking-wider">{t('fert.measureResult')}</label>
                            <div className="flex gap-2">
                                <input
                                    type="number" step="0.1" min="0" placeholder={t('tpa.mlMeasured')}
                                    className="w-full min-w-0 flex-1 rounded-t-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm outline-none transition-colors focus:border-accent"
                                    value={calibMl} onChange={(e) => setCalibMl(e.target.value)}
                                />
                                <button
                                    onClick={handleSaveCalib}
                                    className="flex-none rounded-r-md bg-warn/20 px-4 py-2 text-xs font-bold uppercase tracking-wider text-warn transition hover:bg-warn/30 active:scale-95 shadow-sm"
                                >
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
