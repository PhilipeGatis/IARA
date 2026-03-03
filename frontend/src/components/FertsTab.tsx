import { useState, useEffect } from 'react';
import { api, type AQStatus } from '../App';
import { useT } from '../i18n';

export function FertCard({ index, s, hideAgenda }: { index: number; s: AQStatus['stocks'][0]; hideAgenda?: boolean }) {
    const { t } = useT();
    const [name, setName] = useState(s.name || '');
    const [resetVol, setResetVol] = useState('');
    const [calibMl, setCalibMl] = useState('');
    const [pwm, setPwm] = useState(s.pwm !== undefined ? s.pwm : 255);

    // Schedule States
    const [doses, setDoses] = useState<string[]>(Array(7).fill('0'));
    const [sH, setSH] = useState(s.sH?.toString() || '');
    const [sM, setSM] = useState(s.sM?.toString() || '');

    const shortDays = t('fert.shortDays').split(',');

    // Keep local names/masks in sync only if completely unedited, otherwise let user type
    useEffect(() => {
        if (!name && s.name) setName(s.name);
        // Ignore PWM sync while sliding so it doesn't stutter, but set initial
        if (s.pwm !== undefined && pwm === 255) setPwm(s.pwm);
    }, [s.name, s.pwm]);

    // Update doses from server on boot
    useEffect(() => {
        if (s.doses && doses.every(d => d === '0')) {
            setDoses(s.doses.map(d => d.toString()));
        }
    }, [s.doses]);



    const handleAgendar = () => {
        if (sH && sM) {
            api('POST', '/api/fert/schedule', {
                channel: index,
                doses: doses.map(Number),
                hour: +sH,
                minute: +sM,
            });
        }
    };

    const handlePwm = () => {
        api('POST', '/api/fert/pwm', { channel: index, pwm });
    };

    const handlePump = (st: number) => api('POST', '/api/fert/pump', { channel: index, state: st });
    const handleRun3s = () => {
        if (confirm(t('fert.confirmRun3s', { ch: index + 1 }))) {
            api('POST', '/api/fert/run3s', { channel: index });
        }
    };
    const handleSaveCalib = () => {
        if (+calibMl > 0) {
            if (confirm(t('fert.confirmCalib', { ml: calibMl, ch: index + 1 }))) {
                api('POST', '/api/fert/calibrate', { channel: index, ml: +calibMl });
                setCalibMl('');
            }
        } else alert(t('fert.enterMl'));
    };

    const pct = Math.min(100, (s.stock / 500) * 100);

    return (
        <div className="rounded-2xl bg-card p-5 shadow-md">
            <div className="flex items-center justify-between">
                <span className="text-xs font-bold text-muted uppercase tracking-wider">{t('fert.channel')} {index + 1}</span>
                <span className="text-xl font-bold text-white">{s.stock.toFixed(0)} mL</span>
            </div>

            <div className="mt-3 flex gap-2">
                <input
                    type="text"
                    placeholder={t('fert.namePlaceholder')}
                    value={name}
                    onChange={(e) => setName(e.target.value)}
                    onBlur={() => name ? api('POST', '/api/fert/name', { channel: index, name }) : null}
                    className="w-full flex-1 rounded-t-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm text-text outline-none transition-colors focus:border-accent"
                />
            </div>

            <div className="mb-6 h-2 w-full overflow-hidden rounded-full bg-white/5">
                <div
                    className={`h-full transition-all duration-500 ease-out ${pct < 10 ? 'bg-danger' : pct < 20 ? 'bg-warn' : 'bg-accent2'}`}
                    style={{ width: `${pct}%` }}
                />
            </div>

            <div className="flex flex-col gap-1 mb-6">
                <label className="text-xs font-bold text-muted uppercase tracking-wider">{t('fert.refill')}</label>
                <div className="flex gap-2">
                    <input
                        type="number" min="0" max="2000" placeholder={t('fert.newVolume')}
                        className="w-full min-w-0 flex-1 rounded-t-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm text-text outline-none transition-colors focus:border-accent"
                        value={resetVol} onChange={(e) => setResetVol(e.target.value)}
                    />
                    <button
                        onClick={() => resetVol && api('POST', '/api/stock/reset', { channel: index, ml: +resetVol })}
                        className="flex-none rounded-r-md bg-accent2 px-4 py-2 font-bold uppercase tracking-wider text-black transition hover:bg-teal-300 active:scale-95 shadow-md"
                    >
                        OK
                    </button>
                </div>
            </div>



            {/* SCHEDULE */}
            {!hideAgenda && (
                <>
                    <h3 className="mb-3 flex justify-between items-center text-xs font-bold uppercase tracking-wider text-muted">
                        <span>{t('fert.schedule')} ({String(s.sH).padStart(2, '0')}:{String(s.sM).padStart(2, '0')})</span>
                        {s.fR > 0 && <span>{(doses.reduce((a, b) => a + Number(b), 0) / s.fR).toFixed(1)}s {t('fert.totalWeek')}</span>}
                    </h3>

                    <div className="mb-4 grid grid-cols-7 gap-1">
                        {shortDays.map((day, i) => {
                            const estimatedSecs = s.fR > 0 ? (Number(doses[i]) / s.fR).toFixed(1) : '0';
                            return (
                                <div key={i} className="flex flex-col items-center gap-1">
                                    <span className="text-[10px] font-bold text-muted">{day}</span>
                                    <div className="relative w-full">
                                        <input
                                            type="number" step="0.5" min="0" max="100"
                                            className="w-full min-w-0 rounded-t-sm border-b border-muted bg-white/5 p-1 text-center text-[11px] font-medium text-text outline-none transition-colors focus:border-accent remove-arrow"
                                            style={{ MozAppearance: 'textfield' }}
                                            value={doses[i]}
                                            onChange={(e) => {
                                                const newDoses = [...doses];
                                                newDoses[i] = e.target.value;
                                                setDoses(newDoses);
                                            }}
                                        />
                                    </div>
                                    <span className="text-[9px] text-accent font-bold tracking-tighter">
                                        {Number(doses[i]) > 0 ? `${estimatedSecs}s` : '-'}
                                    </span>
                                </div>
                            );
                        })}
                    </div>
                    <div className="flex gap-4 mb-8">
                        <div className="flex-1 flex flex-col gap-1">
                            <label className="text-[10px] font-bold text-muted uppercase tracking-wider">{t('fert.hour')}</label>
                            <input
                                type="number" min="0" max="23" placeholder="HH"
                                className="w-full rounded-t-md border-b-2 border-muted bg-white/5 px-2 py-2 text-center text-sm outline-none transition-colors focus:border-accent"
                                value={sH} onChange={(e) => setSH(e.target.value)}
                            />
                        </div>
                        <div className="flex items-center text-muted font-bold mt-4">:</div>
                        <div className="flex-1 flex flex-col gap-1">
                            <label className="text-[10px] font-bold text-muted uppercase tracking-wider">{t('fert.min')}</label>
                            <input
                                type="number" min="0" max="59" placeholder="MM"
                                className="w-full rounded-t-md border-b-2 border-muted bg-white/5 px-2 py-2 text-center text-sm outline-none transition-colors focus:border-accent"
                                value={sM} onChange={(e) => setSM(e.target.value)}
                            />
                        </div>
                        <button
                            onClick={handleAgendar}
                            className="mt-4 flex-none rounded-full bg-accent px-4 py-2 text-xs font-bold uppercase tracking-wider text-black shadow-md transition-all hover:bg-blue-300 active:scale-95"
                        >
                            {t('fert.save')}
                        </button>
                    </div>
                </>
            )}

            {/* CALIBRATION & PWM */}
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
                        onMouseDown={() => handlePump(1)} onMouseUp={() => handlePump(0)}
                        onTouchStart={() => handlePump(1)} onTouchEnd={() => handlePump(0)}
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
        </div>
    );
}

export default function FertsTab({ status }: { status: AQStatus | null }) {
    const { t } = useT();
    const [openChannelIndex, setOpenChannelIndex] = useState(0);

    if (!status?.stocks) return <div className="text-muted text-center p-4">{t('fert.loading')}</div>;

    const availableChannels = status.stocks
        .map((s, i) => ({
            idx: i,
            label: `CH${i + 1} - ${s.name || `Canal ${i + 1}`}`,
        }))
        .filter(c => c.idx !== 4);

    return (
        <div className="flex flex-col gap-4">
            {/* Channel Selector */}
            <div className="rounded-2xl bg-card p-4 shadow-md">
                <label className="mb-2 block text-xs font-bold text-muted uppercase tracking-wider">{t('fert.selectChannel')}</label>
                <select
                    className="w-full rounded-t-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm font-medium text-white outline-none transition-colors focus:border-accent"
                    value={openChannelIndex}
                    onChange={(e) => setOpenChannelIndex(Number(e.target.value))}
                >
                    {availableChannels.map((c) => (
                        <option key={c.idx} value={c.idx} className="bg-card">
                            {c.label}
                        </option>
                    ))}
                </select>
            </div>

            {/* Active Channel Details */}
            <FertCard index={openChannelIndex} s={status.stocks[openChannelIndex]} />
        </div>
    );
}
