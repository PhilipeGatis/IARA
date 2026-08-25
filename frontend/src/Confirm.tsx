import { useCallback, useRef, useState } from 'react';

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
    const [message, setMessage] = useState<string | null>(null);
    const resolver = useRef<((ok: boolean) => void) | null>(null);

    const ask = useCallback((text: string) => {
        setMessage(text);
        return new Promise<boolean>((resolve) => {
            resolver.current = resolve;
        });
    }, []);

    const close = (ok: boolean) => {
        setMessage(null);
        resolver.current?.(ok);
        resolver.current = null;
    };

    const dialog = message === null ? null : (
        <div
            className="fixed inset-0 z-50 flex items-end justify-center bg-black/70 p-4 sm:items-center"
            onClick={() => close(false)}
        >
            <div
                className="w-full max-w-[420px] rounded-2xl border border-border bg-card p-4 shadow-xl"
                onClick={(e) => e.stopPropagation()}
            >
                <p className="mb-4 text-sm leading-relaxed text-text">{message}</p>
                <div className="flex gap-2">
                    <button className="btn btn-n flex-1" onClick={() => close(false)}>
                        Cancelar
                    </button>
                    <button className="btn btn-p flex-1" onClick={() => close(true)}>
                        Confirmar
                    </button>
                </div>
            </div>
        </div>
    );

    return { ask, dialog };
}
