import { test, expect } from '@playwright/test';

async function bootSimulator(page) {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');
  await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Simulator');
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('.slot-editor')).toBeVisible();
}

async function expectBrowserOnlyStateClean(page) {
  await expect(page.locator('#dirty-badge')).toBeHidden();
  await expect(page.getByRole('button', { name: 'Apply' })).toBeDisabled();
  const state = await page.evaluate(() => ({
    dirty: window.__MN42_RUNTIME.getState().dirty,
    diff: window.__MN42_RUNTIME.diff()
  }));
  expect(state.dirty).toBe(false);
  expect(state.diff).toEqual([]);
}

test('changing a browser-only slot label does not dirty staged firmware config', async ({
  page
}) => {
  await bootSimulator(page);

  const labelInput = page
    .locator('.slot-editor label:has-text("Slot label (browser only)") input')
    .first();
  await labelInput.fill('Verse cue');
  await labelInput.dispatchEvent('change');

  await expectBrowserOnlyStateClean(page);
});

test('changing browser-only Take Control metadata does not require Apply', async ({ page }) => {
  await bootSimulator(page);

  const takeoverToggle = page
    .locator('.slot-editor label:has-text("Take Control (browser only)") input')
    .first();
  await takeoverToggle.check();

  await expectBrowserOnlyStateClean(page);

  await page.evaluate(async () => {
    await window.__MN42_RUNTIME.disconnect();
  });
  await expect(page.locator('#connection-pill')).toContainText('Disconnected');

  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');
  await expect(
    page.locator('.slot-editor label:has-text("Take Control (browser only)") input').first()
  ).toBeChecked();
  await expectBrowserOnlyStateClean(page);
});
