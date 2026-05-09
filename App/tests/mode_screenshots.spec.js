import { test, expect } from '@playwright/test';
import { promises as fs } from 'node:fs';
import path from 'node:path';

async function openMode(page, mode) {
  await page.addInitScript((nextMode) => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', nextMode);
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  }, mode);
  await page.setViewportSize({ width: 1440, height: 1000 });
  await page.goto(`/?mode=${mode}`);
  const connectButton =
    mode === 'stage'
      ? page.locator('#stage-connect')
      : page.getByRole('button', { name: 'Connect' });
  await connectButton.click();
  await expect(page.locator('#connection-pill')).toHaveText('Connected');
}

test.describe('Mode screenshots', () => {
  for (const mode of ['stage', 'basic', 'advanced']) {
    test(`${mode} mode screenshot artifact`, async ({ page }) => {
      await openMode(page, mode);
      const screenshotDir = path.resolve('test-results/screenshots');
      await fs.mkdir(screenshotDir, { recursive: true });
      await page.screenshot({
        path: path.join(screenshotDir, `${mode}-mode.png`),
        fullPage: true
      });
    });
  }
});
