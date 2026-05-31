import { test, expect } from '@playwright/test';

test('browser-only slot labels and Take Control metadata do not dirty staged firmware config', async ({
  page
}) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');
  await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Simulator');
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('.slot-editor')).toBeVisible();

  const labelInput = page
    .locator('.slot-editor label:has-text("Slot label (browser only)") input')
    .first();
  await labelInput.fill('Verse cue');
  await labelInput.dispatchEvent('change');

  const takeoverToggle = page
    .locator('.slot-editor label:has-text("Take Control (browser only)") input')
    .first();
  await takeoverToggle.check();

  await expect(page.locator('#dirty-badge')).toBeHidden();
  await expect(page.getByRole('button', { name: 'Apply' })).toBeDisabled();

  const stateAfterLocalOnlyEdit = await page.evaluate(() => ({
    dirty: window.__MN42_RUNTIME.getState().dirty,
    diff: window.__MN42_RUNTIME.diff()
  }));
  expect(stateAfterLocalOnlyEdit.dirty).toBe(false);
  const diffAfterLocalOnlyEdit = stateAfterLocalOnlyEdit.diff;
  expect(diffAfterLocalOnlyEdit).toEqual([]);

  await page.evaluate(async () => {
    await window.__MN42_RUNTIME.disconnect();
  });
  await expect(page.locator('#connection-pill')).toContainText('Disconnected');

  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  const labelInputAfterReconnect = page
    .locator('.slot-editor label:has-text("Slot label (browser only)") input')
    .first();
  const takeoverAfterReconnect = page
    .locator('.slot-editor label:has-text("Take Control (browser only)") input')
    .first();

  await expect(labelInputAfterReconnect).toHaveValue('Verse cue');
  await expect(takeoverAfterReconnect).toBeChecked();
  await expect(page.locator('#dirty-badge')).toBeHidden();
});
