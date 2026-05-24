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
  await page.waitForFunction(() => Boolean(window.__MN42_RUNTIME));
  await page.evaluate(() => {
    window.__MN42_RUNTIME?.useSimulator?.(true);
  });
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');
  await page.evaluate(() => {
    const runtime = window.__MN42_RUNTIME;
    runtime?.stage?.((draft) => {
      draft.efSlots = draft.efSlots || [];
      draft.efSlots[0] = { slots: [] };
      return draft;
    });
  });
  await page.waitForFunction(() => {
    const runtime = window.__MN42_RUNTIME;
    const slots = runtime?.getState?.().staged?.efSlots?.[0]?.slots;
    return Array.isArray(slots) && slots.length === 0;
  });
  await page.getByRole('button', { name: 'Envelope' }).click();

  const efRow = page.locator('#ef-assignment-card .ef-row').first();
  await expect(efRow).toBeVisible();
  const slot08 = efRow.getByRole('button', { name: 'S08' });
  const slot42 = efRow.getByRole('button', { name: 'S42' });
  const slot01 = efRow.getByRole('button', { name: 'S01' });
  await slot42.click();
  await slot08.click();
  await slot01.click();
  await slot01.click();
  await expect(efRow.locator('.ef-row-summary')).toHaveText('2 assigned');

  const staged = await page.evaluate(() => {
    const runtime = window.__MN42_RUNTIME;
    const config = runtime?.getState?.().staged;
    const row = config?.efSlots?.[0] ?? {};
    return {
      first: row,
      allRowsUseSlotsArray:
        Array.isArray(config?.efSlots) &&
        config.efSlots.every((entry) => Array.isArray(entry?.slots))
    };
  });

  expect(staged.first.slots).toEqual([7, 41]);
  expect(staged.first.slot).toBeUndefined();
  expect(staged.allRowsUseSlotsArray).toBe(true);
});
