/// <reference types="vitest/config" />
import react from '@vitejs/plugin-react';
import { defineConfig } from 'vite';

// The master the development server forwards /api/ to: RTYPE_MASTER, or the panel's default address
const master = process.env.RTYPE_MASTER ?? 'http://127.0.0.1:8080';

export default defineConfig({
  plugins: [react()],
  build: {
    // Every file of the panel is served by the master: no asset inlined as a data: address, which the CSP
    // refuses for fonts
    assetsInlineLimit: 0,
    // One bundle, Chakra UI most of it, served from the machine of the server: no reason to split it
    chunkSizeWarningLimit: 1024,
  },
  server: {
    // The host of the master: the session cookie it gives is sent to every port of the same host
    host: '127.0.0.1',
    proxy: {
      // The Host of the page is kept: the master accepts the WebSocket of a page of http://<Host>
      '/api': { target: master, ws: true, changeOrigin: false },
    },
  },
  test: {
    environment: 'jsdom',
    globals: true,
    setupFiles: ['./src/test/setup.ts'],
    include: ['src/**/*.test.{ts,tsx}'],
    css: false,
  },
});
