import { useState, useEffect } from 'react';
import { api, type AQStatus } from '../App';
import { useT, type Lang } from '../i18n';

export default function ConfigTab({ status }: { status: AQStatus | null }) {
    const { t, lang, setLang } = useT();
    const [height, setHeight] = useState('');
    const [length, setLength] = useState('');
    const [width, setWidth] = useState('');
    const [margin, setMargin] = useState('');
    const [primeRatio, setPrimeRatio] = useState('');
    const [reservoirVol, setReservoirVol] = useState('');
    const [canisterSafePct, setCanisterSafePct] = useState('');
    const [ssid, setSsid] = useState('');
    const [pass, setPass] = useState('');
    const [networks, setNetworks] = useState<string[]>([]);
    const [scanning, setScanning] = useState(false);

    useEffect(() => {
        if (status) {
            if (!height && status.aqHeight) setHeight(status.aqHeight.toString());
            if (!length && status.aqLength) setLength(status.aqLength.toString());
            if (!width && status.aqWidth) setWidth(status.aqWidth.toString());
            if (!margin && status.aqMarginCm) setMargin(status.aqMarginCm.toString());
            if (!primeRatio && status.primeRatio) setPrimeRatio(status.primeRatio.toString());
            if (!reservoirVol && status.reservoirVolume) setReservoirVol(status.reservoirVolume.toString());
            if (!canisterSafePct && (status as any).canisterSafePct) setCanisterSafePct((status as any).canisterSafePct.toString());
        }
    }, [status]);

    const handleSaveConfig = () => {
        api('POST', '/api/config/aquarium', {
            aqHeight: parseInt(height) || 0,
            aqLength: parseInt(length) || 0,
            aqWidth: parseInt(width) || 0,
            aqMarginCm: parseInt(margin) || 0,
            primeRatio: parseFloat(primeRatio) || 0,
            reservoirVolume: parseInt(reservoirVol) || 0,
            canisterSafePct: parseInt(canisterSafePct) || 0,
        });
    };

    const handleScanWifi = async () => {
        setScanning(true);
        try {
            const res = await fetch('/api/wifi/scan');
            const data = await res.json();
            setNetworks(data.networks || []);
        } catch {
            alert(t('config.scanError'));
        }
        setScanning(false);
    };

    const handleSaveWifi = async () => {
        if (!ssid) return;
        try {
            const res = await fetch('/api/wifi/save', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ssid, password: pass }),
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
        const l = parseInt(length) || 0;
        const w = parseInt(width) || 0;
        const mg = parseInt(margin) || 0;
        const effH = Math.max(0, h - mg);
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

    const handleLangChange = (newLang: Lang) => {
        setLang(newLang);
    };

    return (
        <div className="flex flex-col gap-4 pb-4">
            {/* LANGUAGE SELECTOR */}
            <div className="rounded-2xl bg-card p-5 shadow-md">
                <h2 className="mb-4 text-base font-medium tracking-wide text-text/90 uppercase">{t('config.language')}</h2>
                <div className="flex gap-2">
                    {([
                        { code: 'pt' as Lang, flag: '🇧🇷', label: 'Português' },
                        { code: 'en' as Lang, flag: '🇺🇸', label: 'English' },
                        { code: 'ja' as Lang, flag: '🇯🇵', label: '日本語' },
                    ]).map((l) => (
                        <button
                            key={l.code}
                            onClick={() => handleLangChange(l.code)}
                            className={`flex-1 flex flex-col items-center gap-1.5 rounded-xl py-3 px-2 transition-all active:scale-95 border-2 ${lang === l.code
                                ? 'border-accent bg-accent/10 text-accent shadow-md'
                                : 'border-transparent bg-white/5 text-muted hover:bg-white/10'
                                }`}
                        >
                            <span className="text-2xl">{l.flag}</span>
                            <span className="text-xs font-bold tracking-wide">{l.label}</span>
                        </button>
                    ))}
                </div>
            </div>

            {/* AQUARIUM CONFIG */}
            <div className="rounded-2xl bg-card p-5 shadow-md">
                <h2 className="mb-4 text-base font-medium tracking-wide text-text/90 uppercase">{t('config.aquarium')}</h2>
                <div className="flex flex-col gap-4">
                    <div className="flex flex-col gap-2">
                        <label className="text-xs font-bold text-muted uppercase tracking-wider">{t('config.dimensions')}</label>
                        <div className="grid grid-cols-3 gap-2">
                            <div className="flex flex-col gap-1">
                                <label className="text-[10px] text-muted">{t('config.heightLabel')}</label>
                                <input
                                    type="number" min="0" step="1" placeholder={t('config.height')}
                                    className="w-full rounded-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm text-text outline-none transition-colors focus:border-accent"
                                    value={height} onChange={(e) => setHeight(e.target.value)}
                                />
                            </div>
                            <div className="flex flex-col gap-1">
                                <label className="text-[10px] text-muted">{t('config.lengthLabel')}</label>
                                <input
                                    type="number" min="0" step="1" placeholder={t('config.length')}
                                    className="w-full rounded-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm text-text outline-none transition-colors focus:border-accent"
                                    value={length} onChange={(e) => setLength(e.target.value)}
                                />
                            </div>
                            <div className="flex flex-col gap-1">
                                <label className="text-[10px] text-muted">{t('config.widthLabel')}</label>
                                <input
                                    type="number" min="0" step="1" placeholder={t('config.width')}
                                    className="w-full rounded-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm text-text outline-none transition-colors focus:border-accent"
                                    value={width} onChange={(e) => setWidth(e.target.value)}
                                />
                            </div>
                        </div>
                    </div>

                    <div className="flex flex-col gap-1">
                        <label className="text-xs font-bold text-muted uppercase tracking-wider">{t('config.margin')}</label>
                        <input
                            type="number" min="0" step="1" placeholder="Ex: 3"
                            className="w-full rounded-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm text-text outline-none transition-colors focus:border-accent"
                            value={margin} onChange={(e) => setMargin(e.target.value)}
                        />
                        <span className="text-[10px] text-muted italic mt-1">{t('config.marginHint')}</span>
                    </div>

                    <div className="flex gap-4">
                        <div className="flex-1 rounded-lg bg-accent/10 px-4 py-2">
                            <span className="text-[10px] text-muted">{t('config.calcVolume')}</span>
                            <strong className="ml-2 text-sm text-accent">{calcVolume().toFixed(1)} L</strong>
                        </div>
                        <div className="flex-1 rounded-lg bg-accent2/10 px-4 py-2">
                            <span className="text-[10px] text-muted">{t('config.litersPerCm')}</span>
                            <strong className="ml-2 text-sm text-accent2">{litersPerCm().toFixed(2)}</strong>
                        </div>
                    </div>

                    <div className="flex flex-col gap-1">
                        <label className="text-xs font-bold text-muted uppercase tracking-wider">{t('config.primeRatio')}</label>
                        <input
                            type="number" step="0.01" min="0" placeholder="Ex: 0.05"
                            className="w-full rounded-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm text-text outline-none transition-colors focus:border-accent"
                            value={primeRatio} onChange={(e) => setPrimeRatio(e.target.value)}
                        />
                        <span className="text-[10px] text-muted italic mt-1">{t('config.primeHint')}</span>
                    </div>

                    <div className="flex flex-col gap-1">
                        <label className="text-xs font-bold text-muted uppercase tracking-wider">{t('config.reservoirVol')}</label>
                        <input
                            type="number" step="1" min="0" placeholder="Ex: 20"
                            className="w-full rounded-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm text-text outline-none transition-colors focus:border-accent"
                            value={reservoirVol} onChange={(e) => setReservoirVol(e.target.value)}
                        />
                        <span className="text-[10px] text-muted italic mt-1">{t('config.reservoirHint')}</span>
                    </div>

                    <div className="rounded-lg bg-accent/10 px-4 py-3">
                        <span className="text-[10px] text-muted">{t('config.calcPrime')}</span>
                        <strong className="ml-2 text-sm text-accent">{calcPrimeDose() > 0 ? `${calcPrimeDose().toFixed(2)} mL` : t('config.calcPrimeHint')}</strong>
                    </div>

                    <button
                        onClick={handleSaveConfig}
                        className="mt-2 rounded-full bg-accent2 px-6 py-2.5 text-sm font-bold uppercase tracking-wider text-black shadow-md transition-all hover:bg-teal-300 active:scale-95"
                    >
                        {t('config.saveConfig')}
                    </button>
                </div>
            </div>

            {/* NETWORK CONFIG */}
            <div className="rounded-2xl bg-card p-5 shadow-md">
                <h2 className="mb-4 text-base font-medium tracking-wide text-text/90 uppercase">{t('config.network')}</h2>

                <div className="flex flex-col gap-4">
                    <div className="flex gap-2">
                        <select
                            className="flex-1 rounded-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm font-medium text-white outline-none transition-colors focus:border-accent"
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
                            className="flex-none rounded-md bg-accent/20 px-4 py-2 text-xs font-bold uppercase tracking-wider text-accent transition hover:bg-accent/30 active:scale-95 disabled:opacity-50"
                        >
                            {scanning ? t('config.scanning') : '📡'}
                        </button>
                    </div>

                    <input
                        type="password" placeholder={t('config.password')}
                        className="w-full rounded-md border-b-2 border-muted bg-white/5 px-3 py-2 text-sm text-text outline-none transition-colors focus:border-accent"
                        value={pass} onChange={(e) => setPass(e.target.value)}
                    />

                    <button
                        onClick={handleSaveWifi}
                        className="rounded-full bg-accent px-6 py-2.5 text-sm font-bold uppercase tracking-wider text-black shadow-md transition-all hover:bg-blue-300 active:scale-95"
                    >
                        {t('config.saveRestart')}
                    </button>
                </div>
            </div>

            {/* EMERGENCY ACTIONS */}
            <div className="rounded-2xl bg-card p-5 shadow-md">
                <h2 className="mb-4 text-base font-medium tracking-wide text-text/90 uppercase">{t('config.emergency')}</h2>

                <button
                    onClick={() => api('POST', '/api/maintenance')}
                    className="w-full rounded-full bg-warn/20 px-6 py-2.5 text-sm font-bold uppercase tracking-wider text-warn shadow-md transition-all hover:bg-warn/30 active:scale-95"
                >
                    {t('config.pauseTpa')}
                </button>
            </div>
        </div>
    );
}
