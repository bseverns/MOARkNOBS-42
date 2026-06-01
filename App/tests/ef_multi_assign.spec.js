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
      const followerCount =
        runtime?.getState?.().manifest?.envelope_count ??
        (Array.isArray(draft.efSlots) && draft.efSlots.length > 0 ? draft.efSlots.length : 6);
      draft.slots = Array.isArray(draft.slots)
        ? draft.slots.map((slot) => ({
            ...(slot ?? {}),
            ef_index: -1,
            ef: { ...(slot?.ef ?? {}), index: -1 }
          }))
        : [];
      draft.efSlots = Array.from({ length: followerCount }, () => ({ slots: [] }));
      if (draft.envelopes && typeof draft.envelopes === 'object') {
        draft.envelopes = {
          ...draft.envelopes,
          routing: Array.isArray(draft.envelopes.routing)
            ? draft.envelopes.routing.map(() => -1)
            : draft.envelopes.routing
        };
      }
      return draft;
    });
  });
  await page.waitForFunction(() => {
    const runtime = window.__MN42_RUNTIME;
    const efSlots = runtime?.getState?.().staged?.efSlots;
    return Array.isArray(efSlots) && efSlots.length > 0 && Array.isArray(efSlots[0]?.slots);
  });
  await page.getByRole('button', { name: 'Envelope' }).click();

  const efRow = page.locator('#ef-assignment-card .ef-row').first();
  await expect(efRow).toBeVisible();
  await expect(efRow.locator('.ef-row-summary')).toHaveText('Unassigned');
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
