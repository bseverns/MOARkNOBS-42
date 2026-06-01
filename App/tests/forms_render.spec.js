import { test, expect } from '@playwright/test';

test('a real staged firmware config edit enables Apply and clears only after ACK', async ({
  page
}) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = {
      useSimulator: true
    };
  });

  await page.goto('/benzknobz.html');
  await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Simulator');
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  const freqInput = page.locator('[data-schema-target="filter"] input[type="number"]').first();
  await expect(freqInput).toHaveCount(1);
  await freqInput.fill('1234');
  await freqInput.dispatchEvent('change');

  await expect(page.locator('#dirty-badge')).toBeVisible();
  const apply = page.getByRole('button', { name: 'Apply' });
  await expect(apply).toBeEnabled();
  const diffBeforeApply = await page.evaluate(() => window.__MN42_RUNTIME.diff());
  expect(diffBeforeApply.length).toBeGreaterThan(0);

  await apply.click();
  await expect(page.locator('#status-label')).toHaveText('Synced', { timeout: 5000 });
  await expect(page.locator('#dirty-badge')).toBeHidden();
});
