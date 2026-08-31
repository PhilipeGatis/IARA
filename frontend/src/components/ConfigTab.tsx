import { useState, useEffect, useRef } from 'react';
import { type AQStatus } from '../App';
import { api, REQUEST_HEADER } from '../api';
import Sheet from './Sheet';
import { useT, type Lang } from '../i18n';
import { useConfirm } from '../Confirm';

type NotifyStatus = {
    enabled: boolean;
    topic: string;
    dailyCount: number;
    reportHour: number;
    reportMinute: number;
    types: boolean[];
};

const NOTIFY_TYPE_KEYS = [
    'notify.tpaComplete',
    'notify.tpaError',
    'notify.fertLowStock',
    'notify.emergency',
    'notify.fertComplete',
    'notify.dailyLevel',
] as const;

const NOTIFY_TYPE_API_KEYS = [
    'tpaComplete',
    'tpaError',
    'fertLowStock',
    'emergency',
    'fertComplete',
    'dailyLevel',
];

const LANGS: { code: Lang; flag: string; label: string }[] = [
    { code: 'pt', flag: '🇧🇷', label: 'PT' },
    { code: 'en', flag: '🇺🇸', label: 'EN' },
    { code: 'ja', flag: '🇯🇵', label: '日本語' },
];

export default function ConfigTab({ status }: { status: AQStatus | null }) {
    const { t, lang, setLang } = useT();
    const { ask, dialog } = useConfirm();
    const [height, setHeight] = useState('');
    const [margin, setMargin] = useState('');
    const [length, setLength] = useState('');
    const [width, setWidth] = useState('');
    const [sensorFull, setSensorFull] = useState('');
    const [blindZone, setBlindZone] = useState('');
    const [primeRatio, setPrimeRatio] = useState('');
    const [reservoirVol, setReservoirVol] = useState('');
    const [canisterSafePct, setCanisterSafePct] = useState('');
    const [sheet, setSheet] = useState<null | 'aquarium' | 'notify'>(null);
    const [feedPauseMin, setFeedPauseMin] = useState('');
    const [ssid, setSsid] = useState('');
    const [pass, setPass] = useState('');
    const [networks, setNetworks] = useState<string[]>([]);
    const [scanning, setScanning] = useState(false);

    // OTA States
    const [otaFile, setOtaFile] = useState<File | null>(null);
    const [otaProgress, setOtaProgress] = useState(-1);
    const [otaStatus, setOtaStatus] = useState<string | null>(null);

    // Notification state
    const [notifyStatus, setNotifyStatus] = useState<NotifyStatus | null>(null);
    const [topicInput, setTopicInput] = useState('');
    const [reportH, setReportH] = useState('');
    const [reportM, setReportM] = useState('');
    const [typeToggles, setTypeToggles] = useState<boolean[]>([]);

    const initialized = useRef(false);

    useEffect(() => {
        if (status && !initialized.current) {
            initialized.current = true;
            if (status.aqHeight) setHeight(status.aqHeight.toString());
            if (status.aqMarginMm !== undefined) setMargin(status.aqMarginMm.toString());
            if (status.aqLength) setLength(status.aqLength.toString());
            if (status.aqWidth) setWidth(status.aqWidth.toString());
            if (status.sensorFullDistanceMm !== undefined) setSensorFull(status.sensorFullDistanceMm.toString());
            if (status.ultrasonicMinMm !== undefined) setBlindZone(status.ultrasonicMinMm.toString());
            if (status.primeRatio) setPrimeRatio(status.primeRatio.toString());
            if (status.reservoirVolume) setReservoirVol(status.reservoirVolume.toString());
            if (status.canisterSafePct) setCanisterSafePct(status.canisterSafePct.toString());
            if (status.feedPauseMin) setFeedPauseMin(status.feedPauseMin.toString());
        }
    }, [status]);

    // Fetch notification status
    useEffect(() => {
        fetch('/api/notify/status')
            .then((r) => r.json())
            .then((data: NotifyStatus) => {
                setNotifyStatus(data);
                setReportH(data.reportHour.toString());
                setReportM(data.reportMinute.toString());
                setTypeToggles(data.types || []);
            })
            .catch(() => { });
    }, []);

    const handleSaveConfig = () => {
        api('POST', '/api/config/aquarium', {
            aqHeight: parseInt(height) || 0,
            aqMarginMm: parseInt(margin) || 0,
            aqLength: parseInt(length) || 0,
            aqWidth: parseInt(width) || 0,
            sensorFullDistanceMm: parseInt(sensorFull) || 0,
            ultrasonicMinMm: parseInt(blindZone) || 0,
            primeRatio: parseFloat(primeRatio) || 0,
            reservoirVolume: parseInt(reservoirVol) || 0,
            canisterSafePct: parseInt(canisterSafePct) || 0,
            feedPauseMin: parseInt(feedPauseMin) || 0,
        });
    };

    const handleScanWifi = async () => {
        setScanning(true);
        try {
            const res = await fetch('/api/wifi/scan');
            const data = await res.json();
            if (data.networks) {
                setNetworks(data.networks.map((n: any) => n.ssid || n));
            } else if (data.status === 'scanning') {
                alert(t('config.scanning'));
            }
        } catch {
            alert(t('config.scanError'));
        }
        setScanning(false);
    };

    const handleSaveWifi = async () => {
        if (!ssid) return;
        try {
            const res = await fetch('/api/wifi', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                    [REQUEST_HEADER]: '1',
                },
                body: new URLSearchParams({ ssid, pass }).toString(),
            });
            const data = await res.json();
            if (data.ok) alert(t('config.wifiOk'));
            else alert(t('config.wifiError'));
        } catch {
            alert(t('config.commError'));
        }
    };

    const calcVolume = () => {
        const h = parseInt(height) || 0;
        const m = (parseInt(margin) || 0) / 10.0;
        const l = parseInt(length) || 0;
        const w = parseInt(width) || 0;
        const effH = Math.max(0, h - m);
        return (effH * l * w) / 1000;
    };

    const litersPerCm = () => {
        const l = parseInt(length) || 0;
        const w = parseInt(width) || 0;
        return (l * w) / 1000;
    };

    const calcPrimeDose = () => {
        const rv = parseInt(reservoirVol) || 0;
        const pr = parseFloat(primeRatio) || 0;
        return rv * pr;
    };

    const handleCalibrateSensor = async () => {
        try {
            const res = await fetch('/api/config/calibrate-sensor-full', {
                method: 'POST',
                headers: { [REQUEST_HEADER]: '1' },
            });
            if (res.ok) {
                alert(t('config.success'));
                window.location.reload();
            }
            else alert(t('config.commError'));
        } catch {
            alert(t('config.commError'));
        }
    };

    // Notification handlers
    const handleSaveTopic = async () => {
        if (!topicInput.trim()) {
            alert(t('notify.noKey'));
            return;
        }
        try {
            await fetch('/api/notify/key', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json', [REQUEST_HEADER]: '1' },
                body: JSON.stringify({ topic: topicInput.trim() }),
            });
            alert(t('notify.keySaved'));
            // Refresh status
            const res = await fetch('/api/notify/status');
            const data = await res.json();
            setNotifyStatus(data);
            setTopicInput('');
        } catch {
            alert(t('config.commError'));
        }
    };

    const handleTestNotify = async () => {
        try {
            await fetch('/api/notify/test', {
                method: 'POST',
                headers: { [REQUEST_HEADER]: '1' },
            });
            alert(t('notify.testSent'));
        } catch {
            alert(t('config.commError'));
        }
    };

    const handleSaveNotifyConfig = async () => {
        const body: Record<string, any> = {
            reportHour: parseInt(reportH) || 0,
            reportMinute: parseInt(reportM) || 0,
        };
        typeToggles.forEach((on, i) => {
            body[NOTIFY_TYPE_API_KEYS[i]] = on ? 1 : 0;
        });
        try {
            await fetch('/api/notify/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json', [REQUEST_HEADER]: '1' },
                body: JSON.stringify(body),
            });
            // Refresh status
            const res = await fetch('/api/notify/status');
            const data = await res.json();
            setNotifyStatus(data);
            setTypeToggles(data.types || []);
        } catch {
            alert(t('config.commError'));
        }
    };

    const handleToggleType = (idx: number) => {
        setTypeToggles((prev) => {
            const next = [...prev];
            next[idx] = !next[idx];
            return next;
        });
    };

    const handleOtaUpload = () => {
        if (!otaFile) return;
        setOtaProgress(0);
        setOtaStatus(null);
        const xhr = new XMLHttpRequest();
        xhr.upload.addEventListener('progress', (event) => {
            if (event.lengthComputable) {
                const percent = Math.round((event.loaded / event.total) * 100);
                setOtaProgress(percent);
            }
        });
        xhr.addEventListener('load', () => {
            if (xhr.status === 200) {
                setOtaStatus(t('config.otaSuccess'));
                setTimeout(() => window.location.reload(), 3000);
            } else {
                setOtaStatus(t('config.otaError'));
                setOtaProgress(-1);
            }
        });
        xhr.addEventListener('error', () => {
            setOtaStatus(t('config.otaError'));
            setOtaProgress(-1);
        });
        const formData = new FormData();
        formData.append('update', otaFile, otaFile.name);
        xhr.open('POST', '/api/ota', true);
        // A multipart form POST is exactly what a hostile page can forge without
        // a preflight, and this endpoint writes flash.
        xhr.setRequestHeader(REQUEST_HEADER, '1');
        xhr.send(formData);
    };

    return (
        <div className="flex flex-col gap-3">
            {dialog}
            {/* LANGUAGE — compact segmented control */}
            <section className="card">
                <div className="card-h">
                    <h2 className="card-t">{t('config.language')}</h2>
                </div>
                <div className="grid grid-cols-3 gap-2">
                    {LANGS.map((l) => (
                        <button
                            key={l.code}
                            onClick={() => setLang(l.code)}
                            aria-pressed={lang === l.code}
                            className={`btn ${lang === l.code ? 'btn-a' : 'btn-n'}`}
                        >
                            <span className="text-lg leading-none">{l.flag}</span>
                            <span className="text-xs">{l.label}</span>
                        </button>
                    ))}
                </div>
            </section>

            {/* AQUARIUM — a summary; the fields live in a sheet */}
            <section className="card">
                <div className="card-h">
                    <h2 className="card-t">{t('config.aquarium')}</h2>
                    <span className="pill bg-white/5 text-muted">
                        {height && length && width ? `${height}×${length}×${width} cm` : t('config.notSet')}
                    </span>
                </div>
                <button onClick={() => setSheet('aquarium')} className="btn btn-n w-full">
                    {t('config.edit')}
                </button>
            </section>

            {/* NETWORK */}
            <section className="card">
                <div className="card-h">
                    <h2 className="card-t">{t('config.network')}</h2>
                </div>

                <div className="flex flex-col gap-3">
                    <div className="flex gap-2">
                        <select
                            className="inp flex-1"
                            aria-label={t('config.selectNetwork')}
                            value={ssid}
                            onChange={(e) => setSsid(e.target.value)}
                        >
                            <option value="" className="bg-card">{networks.length > 0 ? t('config.selectNetwork') : t('config.noNetworks')}</option>
                            {networks.map((n) => (
                                <option key={n} value={n} className="bg-card">{n}</option>
                            ))}
                        </select>
                        <button
                            onClick={handleScanWifi}
                            disabled={scanning}
                            className="btn btn-a flex-none"
                        >
                            {scanning ? t('config.scanning') : '📡'}
                        </button>
                    </div>

                    <input
                        type="password" placeholder={t('config.password')}
                        className="inp"
                        value={pass} onChange={(e) => setPass(e.target.value)}
                    />

                    <button onClick={handleSaveWifi} className="btn btn-p w-full">
                        {t('config.saveRestart')}
                    </button>
                </div>
            </section>

            {/* NOTIFICATIONS (ntfy.sh) — a summary; the toggles live in a sheet */}
            <section className="card">
                <div className="card-h">
                    <h2 className="card-t">{t('notify.title')}</h2>
                    <span className={`pill ${notifyStatus?.enabled ? 'bg-accent2/20 text-accent2' : 'bg-white/10 text-muted'}`}>
                        {notifyStatus?.enabled ? t('notify.enabled') : t('notify.disabled')}
                    </span>
                </div>
                <button onClick={() => setSheet('notify')} className="btn btn-n w-full">
                    {t('config.edit')}
                </button>
                <p className="hint">
                    {t('notify.activeCount', {
                        n: String(typeToggles.filter(Boolean).length),
                        total: String(NOTIFY_TYPE_KEYS.length),
                    })}
                </p>
            </section>

            {/* FIRMWARE UPDATE */}
            <section className="card">
                <div className="card-h">
                    <h2 className="card-t">{t('config.otaTitle')}</h2>
                </div>

                <div className="flex flex-col gap-3">
                    <div className="field">
                        <label className="lbl">{t('config.otaSelect')}</label>
                        <input
                            type="file"
                            accept=".bin"
                            onChange={(e) => setOtaFile(e.target.files?.[0] || null)}
                            className="w-full rounded-lg border border-border bg-white/5 p-2 text-xs text-muted file:mr-3 file:rounded-lg file:border-0 file:bg-accent2/20 file:px-3 file:py-2 file:text-xs file:font-bold file:text-accent2"
                        />
                    </div>

                    {otaProgress >= 0 && (
                        <div className="h-3 w-full overflow-hidden rounded-full border border-border bg-black/30">
                            <div className="h-full rounded-full bg-accent2 transition-all duration-300 ease-out" style={{ width: `${otaProgress}%` }} />
                        </div>
                    )}

                    {otaStatus && (
                        <div className={`text-center text-xs font-bold ${otaProgress === 100 ? 'text-accent2' : 'text-danger'}`}>
                            {otaStatus}
                        </div>
                    )}

                    <button
                        onClick={handleOtaUpload}
                        disabled={!otaFile || otaProgress >= 0}
                        className="btn btn-p w-full"
                    >
                        {otaProgress >= 0 && otaProgress < 100 ? `${t('config.otaUploading')} ${otaProgress}%` : t('config.otaUpload')}
                    </button>
                </div>
            </section>

            {/* EMERGENCY / MAINTENANCE */}
            <section className="card">
                <div className="card-h">
                    <h2 className="card-t">{t('config.emergency')}</h2>
                    <span className={`pill ${status?.maintenance ? 'bg-warn/20 text-warn' : 'bg-white/5 text-muted'}`}>
                        {status?.maintenance ? t('home.maintActive') : t('home.maintInactive')}
                    </span>
                </div>

                <button
                    onClick={() => api('POST', '/api/maintenance/toggle')}
                    className={`btn w-full ${status?.maintenance ? 'btn-g' : 'btn-w'}`}
                >
                    {status?.maintenance ? t('config.resumeTpa') : t('config.pauseTpa')}
                </button>

                <div className="sub" />
                <button
                    onClick={async () => {
                        const msg = status?.emergency ? t('confirm.emergencyClear') : t('confirm.emergency');
                        if (await ask(msg)) api('POST', '/api/emergency/stop');
                    }}
                    className={`btn w-full ${status?.emergency ? 'btn-g' : 'btn-dd'}`}
                >
                    {status?.emergency ? t('config.emergencyClear') : t('config.emergencyStop')}
                </button>
                <p className="hint">
                    {status?.emergency ? t('config.emergencyClearHint') : t('config.emergencyHint')}
                </p>
            </section>

            {sheet === 'aquarium' && (
                <Sheet title={t('config.aquarium')} onClose={() => setSheet(null)}>
                    <div className="flex flex-col gap-4">
                        {/* Dimensions */}
                        <div className="field">
                            <label className="lbl">{t('config.dimensions')}</label>
                            <div className="grid grid-cols-3 gap-2">
                                <input
                                    type="number" min="0" step="1" placeholder={t('config.height')}
                                    aria-label={t('config.heightLabel')}
                                    className="inp remove-arrow text-center"
                                    value={height} onChange={(e) => setHeight(e.target.value)}
                                />
                                <input
                                    type="number" min="0" step="1" placeholder={t('config.length')}
                                    aria-label={t('config.lengthLabel')}
                                    className="inp remove-arrow text-center"
                                    value={length} onChange={(e) => setLength(e.target.value)}
                                />
                                <input
                                    type="number" min="0" step="1" placeholder={t('config.width')}
                                    aria-label={t('config.widthLabel')}
                                    className="inp remove-arrow text-center"
                                    value={width} onChange={(e) => setWidth(e.target.value)}
                                />
                            </div>
                            <div className="grid grid-cols-3 gap-2 text-center text-[10px] text-muted">
                                <span>{t('config.heightLabel')}</span>
                                <span>{t('config.lengthLabel')}</span>
                                <span>{t('config.widthLabel')}</span>
                            </div>
                        </div>

                        <div className="field">
                            <label className="lbl">{t('config.margin')}</label>
                            <input
                                type="number" min="0" step="1" placeholder="15"
                                className="inp remove-arrow"
                                value={margin} onChange={(e) => setMargin(e.target.value)}
                            />
                            <span className="hint">{t('config.marginHint')}</span>
                        </div>

                        <div className="grid grid-cols-2 gap-2">
                            <div className="rounded-lg bg-accent/10 px-3 py-2">
                                <span className="block text-[10px] text-muted">{t('config.calcVolume')}</span>
                                <strong className="font-mono text-sm text-accent">{calcVolume().toFixed(1)} L</strong>
                            </div>
                            <div className="rounded-lg bg-accent2/10 px-3 py-2">
                                <span className="block text-[10px] text-muted">{t('config.litersPerCm')}</span>
                                <strong className="font-mono text-sm text-accent2">{litersPerCm().toFixed(2)}</strong>
                            </div>
                        </div>

                        {/* Level sensor */}
                        <div className="sub flex flex-col gap-3">
                            <h3 className="sub-t">{t('config.sensorSection')}</h3>
                            <div className="field">
                                <label className="lbl">{t('config.sensorFull')}</label>
                                <input
                                    type="number" min="0" step="1" placeholder="50"
                                    className="inp remove-arrow"
                                    value={sensorFull} onChange={(e) => setSensorFull(e.target.value)}
                                />
                                <span className="hint">{t('config.sensorFullHint')}</span>
                            </div>
                            <div className="field">
                                <label className="lbl">{t('config.blindZone')}</label>
                                <input
                                    type="number" min="5" max="200" step="1" placeholder="20"
                                    className="inp remove-arrow"
                                    value={blindZone} onChange={(e) => setBlindZone(e.target.value)}
                                />
                                <span className="hint">{t('config.blindZoneHint')}</span>
                            </div>
                            <button onClick={handleCalibrateSensor} className="btn btn-a2 w-full">
                                {t('config.calibrateSensor')}
                            </button>
                        </div>

                        {/* Reservoir & Prime */}
                        <div className="sub flex flex-col gap-4">
                            <h3 className="sub-t">{t('config.reservoirSection')}</h3>
                            <div className="field">
                                <label className="lbl">{t('config.reservoirVol')}</label>
                                <input
                                    type="number" step="1" min="0" placeholder="20"
                                    className="inp remove-arrow"
                                    value={reservoirVol} onChange={(e) => setReservoirVol(e.target.value)}
                                />
                                <span className="hint">{t('config.reservoirHint')}</span>
                            </div>

                            <div className="field">
                                <label className="lbl">{t('config.canisterSafe')}</label>
                                <input
                                    type="number" step="1" min="0" max="100" placeholder="25"
                                    className="inp remove-arrow"
                                    value={canisterSafePct} onChange={(e) => setCanisterSafePct(e.target.value)}
                                />
                                <span className="hint">{t('config.canisterSafeHint')}</span>
                            </div>

                            <div className="field">
                                <label className="lbl">{t('config.feedPause')}</label>
                                <input
                                    type="number" step="1" min="1" max="60" placeholder="10"
                                    className="inp remove-arrow"
                                    value={feedPauseMin} onChange={(e) => setFeedPauseMin(e.target.value)}
                                />
                                <span className="hint">{t('config.feedPauseHint')}</span>
                            </div>

                            <div className="field">
                                <label className="lbl">{t('config.primeRatio')}</label>
                                <input
                                    type="number" step="0.01" min="0" placeholder="0.05"
                                    className="inp remove-arrow"
                                    value={primeRatio} onChange={(e) => setPrimeRatio(e.target.value)}
                                />
                                <span className="hint">{t('config.primeHint')}</span>
                            </div>

                            <div className="rounded-lg bg-accent/10 px-3 py-2">
                                <span className="block text-[10px] text-muted">{t('config.calcPrime')}</span>
                                <strong className="font-mono text-sm text-accent">
                                    {calcPrimeDose() > 0 ? `${calcPrimeDose().toFixed(2)} mL` : t('config.calcPrimeHint')}
                                </strong>
                            </div>
                        </div>

                        <button onClick={handleSaveConfig} className="btn btn-p2 w-full">
                            {t('config.saveConfig')}
                        </button>
                    </div>
                </Sheet>
            )}

            {sheet === 'notify' && (
                <Sheet
                    title={t('notify.title')}
                    badge={
                        <span className={`pill flex-none ${notifyStatus?.enabled ? 'bg-accent2/20 text-accent2' : 'bg-white/10 text-muted'}`}>
                            {notifyStatus?.enabled ? t('notify.enabled') : t('notify.disabled')}
                        </span>
                    }
                    onClose={() => setSheet(null)}
                >
                    <div className="flex flex-col gap-4">
                        <div className="field">
                            <label className="lbl">{t('notify.key')}</label>
                            <div className="flex gap-2">
                                <input
                                    type="text"
                                    placeholder={notifyStatus?.topic || 'iara_topic'}
                                    className="inp flex-1"
                                    value={topicInput}
                                    onChange={(e) => setTopicInput(e.target.value)}
                                />
                                <button onClick={handleSaveTopic} className="btn btn-a flex-none">
                                    {t('notify.save')}
                                </button>
                            </div>
                            <span className="hint">{t('notify.keyHint')}</span>
                        </div>

                        {notifyStatus?.enabled && (
                            <button onClick={handleTestNotify} className="btn btn-a2 w-full">
                                {t('notify.test')}
                            </button>
                        )}

                        <div className="sub flex flex-col gap-3">
                            <div className="field">
                                <label className="lbl">{t('notify.reportTime')}</label>
                                <div className="grid grid-cols-2 gap-2">
                                    <input
                                        type="number" min="0" max="23" step="1"
                                        aria-label={t('tpa.hour')}
                                        className="inp remove-arrow text-center"
                                        value={reportH} onChange={(e) => setReportH(e.target.value)}
                                    />
                                    <input
                                        type="number" min="0" max="59" step="1"
                                        aria-label={t('tpa.minute')}
                                        className="inp remove-arrow text-center"
                                        value={reportM} onChange={(e) => setReportM(e.target.value)}
                                    />
                                </div>
                            </div>

                            <div className="flex flex-col gap-2">
                                {NOTIFY_TYPE_KEYS.map((key, idx) => (
                                    <button
                                        key={key}
                                        onClick={() => handleToggleType(idx)}
                                        aria-pressed={!!typeToggles[idx]}
                                        className={`flex min-h-[48px] items-center justify-between gap-3 rounded-xl border px-4 py-2 text-left transition-all active:scale-[0.98] ${typeToggles[idx] ? 'border-accent2/40 bg-accent2/10' : 'border-border bg-white/5'
                                            }`}
                                    >
                                        <span className="min-w-0 text-sm text-text">{t(key)}</span>
                                        <span className={`inline-flex h-6 w-11 flex-none items-center rounded-full p-0.5 transition-colors ${typeToggles[idx] ? 'bg-accent2' : 'bg-white/20'}`}>
                                            <span className={`inline-block h-5 w-5 rounded-full bg-white shadow-md transition-transform ${typeToggles[idx] ? 'translate-x-5' : 'translate-x-0'}`} />
                                        </span>
                                    </button>
                                ))}
                            </div>

                            <button onClick={handleSaveNotifyConfig} className="btn btn-p2 w-full">
                                {t('notify.saveConfig')}
                            </button>
                        </div>
                    </div>
                </Sheet>
            )}
        </div>
    );
}
