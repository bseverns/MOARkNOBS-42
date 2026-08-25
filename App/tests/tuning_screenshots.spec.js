import { test, expect } from '@playwright/test';
import { promises as fs } from 'node:fs';
import path from 'node:path';

async function bootConfigureSimulator(page) {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'basic');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });
  await page.setViewportSize({ width: 1440, height: 1000 });
  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toHaveText('Connected');
  // Element captures should show the tuning surface itself, not the global
  // fixed Apply bar that normally overlays the viewport once a recipe is staged.
  await page.locator('#change-bar').evaluate((element) => {
    element.style.display = 'none';
  });
}

test('selected-slot tuning screenshot states', async ({ page }) => {
  await bootConfigureSimulator(page);
  const screenshotDir = path.resolve('test-results/screenshots/tuning');
  await fs.mkdir(screenshotDir, { recursive: true });
  const tuning = page.getByRole('group', { name: 'Tune This Slot' });

  for (const [buttonName, filename] of [
    ['Clean / Neutral', 'ef-clean.png'],
    ['Smooth', 'ef-smooth.png'],
    ['Punchy', 'ef-punchy.png'],
    ['Gate', 'ef-gate.png']
  ]) {
    await tuning.getByRole('button', { name: buttonName, exact: true }).click();
    await tuning.screenshot({ path: path.join(screenshotDir, filename) });
  }

  await tuning.getByRole('button', { name: 'Customize in Lab' }).click();
  await page.getByRole('tab', { name: 'ARG', exact: true }).click();
  await page.getByRole('button', { name: 'Average Together', exact: true }).click();
  await page.locator('.slot-editor').screenshot({
    path: path.join(screenshotDir, 'arg-average.png')
  });

  await page.getByRole('tab', { name: 'Slot LFO', exact: true }).click();
  await page.locator('.slot-lfo-lane').first().getByRole('button', {
    name: 'Subtle Centered Motion',
    exact: true
  }).click();
  await page.locator('.slot-editor').screenshot({
    path: path.join(screenshotDir, 'lfo-centered.png')
  });
});
