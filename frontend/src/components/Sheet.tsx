import { useEffect, type ReactNode } from 'react';

/**
 * Bottom sheet on a phone, centred dialog on a wide screen.
 *
 * Settings that are read once and then left alone were pushing the ones that
 * are actually touched — maintenance, emergency stop — below the fold. They
 * live behind this instead, so the config tab stays a list of what each group
 * is rather than every field it holds.
 *
 * Same shape as the fertilizer channel modal, which predates it.
 */
export default function Sheet({
    title,
    badge,
    onClose,
    children,
}: {
    title: string;
    /** Optional pill in the header, for a state worth seeing before opening. */
    badge?: ReactNode;
    onClose: () => void;
    children: ReactNode;
}) {
    // Escape closes it, and the page behind must not scroll while it is open:
    // on a phone the sheet covers the page, and a stray drag would otherwise
    // scroll what is underneath instead of the sheet's own content.
    useEffect(() => {
        const onKey = (e: KeyboardEvent) => { if (e.key === 'Escape') onClose(); };
        document.addEventListener('keydown', onKey);
        const previous = document.body.style.overflow;
        document.body.style.overflow = 'hidden';
        return () => {
            document.removeEventListener('keydown', onKey);
            document.body.style.overflow = previous;
        };
    }, [onClose]);

    return (
        <div
            className="fixed inset-0 z-50 flex items-end justify-center sm:items-center"
            role="dialog"
            aria-modal="true"
            aria-label={title}
            onClick={(e) => e.target === e.currentTarget && onClose()}
        >
            <div className="absolute inset-0 bg-black/70 backdrop-blur-sm" />

            <div className="animate-slide-up sheet relative z-10 w-full max-w-lg overflow-y-auto overscroll-contain rounded-t-3xl bg-card shadow-2xl sm:rounded-2xl">
                <div className="sticky top-0 z-10 flex items-center gap-3 rounded-t-3xl border-b border-border/60 bg-card px-4 py-3 sm:rounded-t-2xl">
                    <h2 className="min-w-0 flex-1 truncate text-base font-bold text-text">{title}</h2>
                    {badge}
                    <button
                        onClick={onClose}
                        aria-label="close"
                        className="flex h-11 w-11 flex-none items-center justify-center rounded-full bg-white/10 text-muted transition hover:bg-white/20 hover:text-text active:scale-90"
                    >
                        ✕
                    </button>
                </div>

                <div className="p-4">{children}</div>
            </div>
        </div>
    );
}
