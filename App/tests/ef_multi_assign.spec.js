import { test, expect } from '@playwright/test';

test('ef assignment editor stages multi-slot follower routes', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = {
      useSimulator: true
    };
  });

  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: /simulator/i }).click();
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  const efInput = page.locator('#ef-assignment-card .ef-row input[type="text"]').first();
  await expect(efInput).toBeVisible();
  await efInput.fill('41, 7, 41, foo, -1, 7');
  await efInput.dispatchEvent('change');
  await expect(efInput).toHaveValue('7, 41');

  const staged = await page.evaluate(() => {
    const runtime = window.__MN42_RUNTIME;
    const config = runtime?.getState?.().staged;
    const row = config?.efSlots?.[0] ?? {};
    return {
      first: row,
      allRowsUseSlotsArray: Array.isArray(config?.efSlots) && config.efSlots.every((entry) => Array.isArray(entry?.slots))
    };
  });

  expect(staged.first.slots).toEqual([7, 41]);
  expect(staged.first.slot).toBeUndefined();
  expect(staged.allRowsUseSlotsArray).toBe(true);
});
