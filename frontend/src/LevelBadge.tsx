import type { AQStatus } from './App';

/**
 * Level as a percentage of the calibrated 100% mark. 100% is the level the
 * sensor was calibrated against, so readings above it mean the tank is over
 * its normal line — which is what the overflow guard watches for.
 */
function levelPercent(status: AQStatus | null): number | null {
    if (!status) return null;
    const wl = status.waterLevel;
    const sensorFull = (status.sensorFullDistanceMm || 0) / 10;
    const refCm = status.aqHeight || 0;
    if (wl === undefined || wl < 0 || !sensorFull || !refCm) return null;
    return 100 - ((wl - sensorFull) / refCm) * 100;
}

/** Compact level readout for the header: percentage first, centimetres under. */
export default function LevelBadge({ status }: { status: AQStatus | null }) {
    const pct = levelPercent(status);
    const over = pct !== null && pct > 100;

    return (
        <div className="flex-none text-right leading-tight">
            <div
                className={`text-[15px] font-bold tabular-nums ${over ? 'text-warn' : 'text-accent'}`}
            >
                {pct === null ? '--' : `${pct.toFixed(0)}%`}
            </div>
            <div className="font-mono text-[10px] text-muted">
                {status?.waterLevel !== undefined && status.waterLevel >= 0
                    ? `${status.waterLevel.toFixed(1)} cm`
                    : '--'}
            </div>
        </div>
    );
}

/**
 * A state shown as an icon rather than a row of text: lit when active, dimmed
 * when not. Title carries the wording for anyone who needs it spelled out.
 */
export function StateIcon({ icon, on, label }: { icon: string; on?: boolean; label: string }) {
    return (
        <span
            title={label}
            aria-label={label}
            className={`text-base leading-none transition-opacity ${on ? 'opacity-100' : 'opacity-25 grayscale'}`}
        >
            {icon}
        </span>
    );
}
