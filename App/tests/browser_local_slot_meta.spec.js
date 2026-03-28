import { test, expect } from '@playwright/test';

test('browser-local slot metadata does not dirty device config and survives reconnect', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: /simulator/i }).click();
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('.slot-editor')).toBeVisible();

  const labelInput = page.locator('.slot-editor label:has-text("Slot label (browser only)") input').first();
  await labelInput.fill('Verse cue');
  await labelInput.dispatchEvent('change');

  const takeoverToggle = page.locator('.slot-editor label:has-text("Take Control (browser only)") input').first();
  await takeoverToggle.check();

  await expect(page.locator('#dirty-badge')).toBeHidden();
  await expect(page.getByRole('button', { name: 'Apply' })).toBeDisabled();

  const diffAfterLocalOnlyEdit = await page.evaluate(() => window.__MN42_RUNTIME.diff());
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
