import type { AQStatus } from './App';

/**
 * Level as a percentage of the calibrated 100% mark. 100% is the level the
 * sensor was calibrated against, so readings above it mean the tank is over
 * its normal line — which is what the overflow guard watches for.
 *
 * The denominator is the *usable* height, volume / litresPerCm, not aqHeight.
 * The firmware measures percentages that way — it is the height the water
 * actually occupies, with the border margin already taken out — and this used
 * aqHeight, so the dashboard and the controller reported different percentages
 * for the same water. Mirrors levelPercentFromDistance() in TpaPlan.h.
 */
export function levelPercent(status: AQStatus | null): number | null {
    if (!status) return null;
    const wl = status.waterLevel;
    const sensorFull = (status.sensorFullDistanceMm || 0) / 10;
    const effH = status.litersPerCm ? status.aquariumVolume / status.litersPerCm : 0;
    if (wl === undefined || wl < 0 || !sensorFull || !effH) return null;
    return ((sensorFull + effH - wl) / effH) * 100;
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
 * The level sensor's own health, next to the other header states.
 *
 * StateIcon is wrong for this one. It dims what is off, and neither of this
 * icon's states wants dimming: a working sensor dimmed reads as a broken one,
 * and a broken sensor dimmed is the case that most needs to be seen. So both
 * states stay lit and the glyph carries the meaning — a dish when the sensor
 * is answering, a warning when it has gone quiet, pulsing so it catches the
 * eye. Two glyphs rather than two colours: emoji do not take a text colour.
 */
export function SensorIcon({ ok, okLabel, downLabel }: { ok?: boolean; okLabel: string; downLabel: string }) {
    const down = ok === false;
    return (
        <span
            title={down ? downLabel : okLabel}
            aria-label={down ? downLabel : okLabel}
            className={`text-base leading-none transition-opacity ${down ? 'animate-pulse' : ''}`}
        >
            {down ? '⚠️' : '📡'}
        </span>
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
