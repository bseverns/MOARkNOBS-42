import { test, expect } from '@playwright/test';

test('LFO edits expose a save action and persist through set_profile', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');
  await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Simulator');
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  await page.locator('[data-utility-tab="lfo"]').click();
  await expect(page.locator('[data-utility-panel="lfo"]')).toBeVisible();
  await expect(page.locator('#lfo-status')).toContainText('2 LFOs');

  const save = page.locator('#lfo-save');
  await expect(save).toHaveText('Save slot LFOs');
  await expect(save).toBeDisabled();

  const firstDepth = page.locator('#lfo-editor .lfo-section').first().getByLabel('Depth');
  await firstDepth.fill('0.42');
  await firstDepth.dispatchEvent('change');

  await expect(page.locator('#lfo-status')).toContainText('edited locally');
  await expect(save).toHaveText('Push & save LFO changes');
  await expect(save).toBeEnabled();

  await save.click();
  await expect(page.locator('#lfo-status')).toContainText('saved');
  await expect(save).toHaveText('Save slot LFOs');
  await expect(save).toBeDisabled();

  const profile = await page.evaluate(() =>
    window.__MN42_RUNTIME.sendRpc({ rpc: 'get_profile', slot: 0 })
  );
  expect(profile.lfos[0].depth).toBe(0.42);
});
