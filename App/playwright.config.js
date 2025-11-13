import { defineConfig } from '@playwright/test';
import { fileURLToPath } from 'node:url';

const PORT = Number(process.env.APP_PORT ?? process.env.PORT ?? 4173);
const appDir = fileURLToPath(new URL('.', import.meta.url));

export default defineConfig({
  testDir: './tests',
  fullyParallel: false,
  retries: process.env.CI ? 2 : 0,
  use: {
    baseURL: `http://127.0.0.1:${PORT}`,
    trace: 'on-first-retry'
  },
  webServer: {
    command: 'node ./tests/dev-server.mjs',
    url: `http://127.0.0.1:${PORT}/benzknobz.html`,
    reuseExistingServer: !process.env.CI,
    stdout: 'pipe',
    stderr: 'pipe',
    cwd: appDir,
    timeout: 15_000
  }
});
