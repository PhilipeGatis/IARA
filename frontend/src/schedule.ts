import type { AQStatus } from './App';

/** Days that share one dose and one time, so a schedule reads as a few lines. */
export type DoseGroup = { days: number[]; dose: number; h: number; m: number };

/**
 * Collapses a channel's weekly schedule into groups of identical days.
 *
 * Almost every channel doses the same amount at the same time every day, and
 * printing that as seven identical rows made the fertilizer tab a wall of
 * repeats. A channel that does vary still gets one line per distinct dose.
 * Days are 0 = Sunday, the firmware's order.
 */
export function doseGroups(s: AQStatus['stocks'][0]): DoseGroup[] {
    const groups: DoseGroup[] = [];
    (s.doses ?? []).forEach((dose, day) => {
        if (!(Number(dose) > 0)) return;
        const h = s.sH?.[day] ?? 0;
        const m = s.sM?.[day] ?? 0;
        const g = groups.find(x => x.dose === dose && x.h === h && x.m === m);
        if (g) g.days.push(day);
        else groups.push({ days: [day], dose: Number(dose), h, m });
    });
    return groups;
}

/** "Todo dia", or the short day names joined: "Seg, Qua, Sex". */
export function daysLabel(days: number[], shortDays: string[], everyDay: string): string {
    if (days.length === 7) return everyDay;
    return days.map(d => shortDays[d] ?? String(d)).join(', ');
}

export const hhmm = (h: number, m: number) =>
    `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}`;
