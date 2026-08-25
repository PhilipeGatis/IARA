/** @type {import('tailwindcss').Config} */
const withAlpha = (v) => `rgb(var(${v}) / <alpha-value>)`;

export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        bg: withAlpha('--bg-rgb'),
        card: withAlpha('--card-rgb'),
        card2: withAlpha('--card2-rgb'),
        accent: withAlpha('--accent-rgb'),
        accent2: withAlpha('--accent2-rgb'),
        good: withAlpha('--good-rgb'),
        warn: withAlpha('--warn-rgb'),
        danger: withAlpha('--danger-rgb'),
        text: withAlpha('--text-rgb'),
        muted: withAlpha('--muted-rgb'),
        border: withAlpha('--border-rgb'),
      },
      spacing: {
        nav: 'calc(var(--nav-h) + var(--safe-b) + 12px)',
      },
    },
  },
  plugins: [],
}
