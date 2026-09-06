import { test, expect } from '@playwright/test';

async function bootAdvancedSimulator(page) {
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

test('fixed slot LFO lanes can be staged, applied, and read back', async ({ page }) => {
  await bootAdvancedSimulator(page);
  await page.locator('#editor-panel').getByRole('tab', { name: 'Slot LFO', exact: true }).click();

  const lane = page.locator('.slot-lfo-lane').first();
  await expect(lane.getByRole('heading', { name: 'LFO 1' })).toBeVisible();
  await lane.getByLabel('Enable LFO 1').check();
  await lane.getByLabel('LFO 1 combine mode').selectOption('4');
  await lane.getByLabel('LFO 1 amount (%)', { exact: true }).fill('35');
  await lane.getByLabel('LFO 1 amount (%)', { exact: true }).dispatchEvent('change');

  await expect(page.locator('#dirty-badge')).toBeVisible();
  await expect(page.locator('#slot-detail-lfo')).toContainText('L1 Centered +35%');
  await expect(page.locator('.slot-button[data-index="0"] .slot-modulation')).toContainText('L1');
  await expect(page.locator('.stage-slot-cell[data-index="0"] .stage-slot-modulation')).toContainText('L1');
  await page.locator('#apply').click();
  await expect(page.locator('#status-label')).toHaveText('Synced', { timeout: 5000 });
  await expect(page.locator('#dirty-badge')).toBeHidden();

  const applied = await page.evaluate(() => window.__MN42_RUNTIME.getState().live.slots[0].lfo);
  expect(applied[0]).toEqual({ enabled: true, mode: 4, amount: 35 });
});

test('fixed slot LFO lane edits survive profile save and reload', async ({ page }) => {
  await bootAdvancedSimulator(page);
  await page.locator('#editor-panel').getByRole('tab', { name: 'Slot LFO', exact: true }).click();
  const amount = page.locator('.slot-lfo-lane').nth(1).getByLabel('LFO 2 amount (%)', { exact: true });
  await page.locator('.slot-lfo-lane').nth(1).getByLabel('Enable LFO 2').check();
  await amount.fill('-24');
  await amount.dispatchEvent('change');

  await page.locator('#apply').click();
  await expect(page.locator('#status-label')).toHaveText('Synced', { timeout: 5000 });
  await page.locator('#recovery-drawer').evaluate((element) => {
    element.open = true;
  });
  await page.locator('#profile-save').click();
  await expect(page.locator('#status-label')).toHaveText('Profile saved', { timeout: 5000 });

  await amount.fill('0');
  await amount.dispatchEvent('change');
  await page.getByRole('button', { name: 'Switch to Profile A now', exact: true }).click();
  await expect(page.locator('#status-label')).toHaveText('Draft protected');
  await page.locator('#change-discard').click();
  await expect(page.locator('#dirty-badge')).toBeHidden();
  await page.getByRole('button', { name: 'Switch to Profile A now', exact: true }).click();
  await expect(page.locator('#status-label')).toHaveText('Profile switched', { timeout: 5000 });
  await expect(amount).toHaveValue('-24');
});
