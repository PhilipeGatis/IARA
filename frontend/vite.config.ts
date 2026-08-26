import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import viteCompression from 'vite-plugin-compression'
import pkg from './package.json'

import { execSync } from 'child_process'

/**
 * Fallback version for the dev server only.
 *
 * This cannot identify a shipped build. The natural order is compile, then
 * commit, so at compile time HEAD is still the previous commit and the tree is
 * dirty with the changes being compiled — every image reported its own parent.
 * The version the dashboard displays comes from /version.json, stamped into the
 * LittleFS image at buildfs time by scripts/fs_version.py.
 */
function getDevVersion() {
  try {
    const gitHash = execSync('git rev-parse --short HEAD').toString().trim()
    return `${pkg.version}-${gitHash}-dev`
  } catch (e) {
    return pkg.version
  }
}

// https://vite.dev/config/
export default defineConfig({
  define: {
    __APP_VERSION__: JSON.stringify(getDevVersion()),
  },
  plugins: [
    react(),
    viteCompression({
      algorithm: 'gzip', // ESPAsyncWebServer native support
      ext: '.gz',
      deleteOriginFile: true // Delete bloated uncompressed HTML/JS/CSS to save ESP32 flash memory
    })
  ],
  build: {
    outDir: '../data', // Spit the finalized UI right where PlatformIO LittleFS looks
    emptyOutDir: true,
    target: 'esnext',
    modulePreload: { polyfill: false }, // Avoid obsolete code bloat
    assetsInlineLimit: 100000,
    rollupOptions: {
      output: {
        // Flat file output to simplify ESP32 serving (no nested subfolders)
        entryFileNames: `assets/[name].js`,
        chunkFileNames: `assets/[name]-[hash].js`,
        assetFileNames: `assets/[name].[ext]`,
        manualChunks(id) {
          if (id.includes('node_modules')) {
            if (id.includes('lucide-react')) return 'vendor-icons';
            if (id.includes('react-dom')) return 'vendor-react-dom';
            if (id.includes('react/')) return 'vendor-react-core';
            return 'vendor';
          }
        }
      }
    }
  }
})
