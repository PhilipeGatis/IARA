import { useCallback, useRef, useState } from 'react';
import { useT } from './i18n';

/** One answer button. `key` is what the caller gets back. */
export type Choice = { key: string; label: string; primary?: boolean };

/**
 * In-page confirmation.
 *
 * Replaces window.confirm(), which browsers let the user disable ("prevent this
 * page from creating more dialogs"). Once disabled it returns false silently,
 * so every guarded action — start a water change, run a pump, open the solenoid
 * — stops working with no error and no request. A dialog rendered by the page
 * cannot be switched off that way.
 */
export function useConfirm() {
    const { t } = useT();
    const [message, setMessage] = useState<string | null>(null);
    const [choices, setChoices] = useState<Choice[] | null>(null);
    const resolver = useRef<((answer: string | null) => void) | null>(null);

    const prompt = useCallback((text: string, options: Choice[] | null) => {
        setMessage(text);
        setChoices(options);
        return new Promise<string | null>((resolve) => {
            resolver.current = resolve;
        });
    }, []);

    /** Yes/no. Resolves true when confirmed. */
    const ask = useCallback(
        (text: string) => prompt(text, null).then((a) => a === 'ok'),
        [prompt],
    );

    /**
     * More than two answers — "all of them" and "only the ones still pending"
     * are both confirmations, and folding them into one yes/no would hide the
     * choice behind a setting somewhere else.
     *
     * Resolves the chosen key, or null when cancelled.
     */
    const choose = useCallback(
        (text: string, options: Choice[]) => prompt(text, options),
        [prompt],
    );

    const close = (answer: string | null) => {
        setMessage(null);
        setChoices(null);
        resolver.current?.(answer);
        resolver.current = null;
    };

    const dialog = message === null ? null : (
        <div
            className="fixed inset-0 z-50 flex items-end justify-center bg-black/70 p-4 sm:items-center"
            onClick={() => close(null)}
        >
            <div
                className="w-full max-w-[420px] rounded-2xl border border-border bg-card p-4 shadow-xl"
                onClick={(e) => e.stopPropagation()}
            >
                <p className="mb-4 text-sm leading-relaxed text-text">{message}</p>
                {/* Three or more answers stack: side by side they are too narrow
                    to read on a phone, which is where this dashboard is used. */}
                <div className={choices && choices.length > 1 ? 'flex flex-col gap-2' : 'flex gap-2'}>
                    <button className="btn btn-n flex-1" onClick={() => close(null)}>
                        {t('common.cancel')}
                    </button>
                    {choices
                        ? choices.map((c) => (
                            <button
                                key={c.key}
                                className={`btn flex-1 ${c.primary ? 'btn-p' : 'btn-s'}`}
                                onClick={() => close(c.key)}
                            >
                                {c.label}
                            </button>
                        ))
                        : (
                            <button className="btn btn-p flex-1" onClick={() => close('ok')}>
                                {t('common.confirm')}
                            </button>
                        )}
                </div>
            </div>
        </div>
    );

    return { ask, choose, dialog };
}
