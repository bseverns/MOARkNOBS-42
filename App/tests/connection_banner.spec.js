import { test, expect } from '@playwright/test';

test('connection banner shows device identity and firmware version', async ({ page }) => {
  await page.addInitScript(() => {
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');

  await expect(page.locator('#connect-fail-help summary')).toHaveText('What to do if connect fails');

  await page.getByRole('button', { name: /simulator/i }).click();
  await page.getByRole('button', { name: 'Connect' }).click();

  await expect(page.locator('#connection-pill')).toHaveText('Connected');
  await expect(page.locator('#connection-banner')).toContainText('Connected to: MOARkNOBS-42 (FW sim-fw)');
});
