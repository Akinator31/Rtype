import { defineConfig, devices } from '@playwright/test';

// The panel against a real master and a game of real players (e2e/master.ts), on Linux: the panel built first
// (npm run build), the server and its helpers built with the tests (RTYPE_BUILD_TESTS)
export default defineConfig({
  testDir: 'e2e',
  globalSetup: './e2e/master.ts',
  fullyParallel: false,
  workers: 1,
  retries: 0,
  forbidOnly: !!process.env.CI,
  reporter: process.env.CI ? [['list'], ['html', { open: 'never' }]] : 'list',
  timeout: 60_000,
  use: {
    trace: 'retain-on-failure',
    screenshot: 'only-on-failure',
  },
  projects: [{ name: 'chromium', use: { ...devices['Desktop Chrome'] } }],
});
