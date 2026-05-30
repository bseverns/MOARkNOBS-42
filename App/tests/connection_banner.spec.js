import { test, expect } from '@playwright/test';

test('connection banner shows device identity and firmware version', async ({ page }) => {
  await page.addInitScript(() => {
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');

  await expect(page.locator('#connect-fail-help summary')).toHaveText(
    'What to do if connect fails'
  );

  await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Simulator');
  await page.getByRole('button', { name: 'Connect' }).click();

  await expect(page.locator('#connection-pill')).toHaveText('Connected');
  await expect
    .poll(async () => page.evaluate(() => window.__MN42_RUNTIME.getState().transportMode))
    .toBe('simulator');
  await expect(page.locator('#connection-banner')).toContainText(
    'Connected to: MOARkNOBS-42 (FW sim-fw)'
  );
  await expect(page.locator('#connection-banner')).not.toContainText('Bridge');
  await expect(page.locator('#power-safety-pill')).toContainText('Power: POWER_CHOKED_V1');
  await expect(page.locator('#power-safety-pill')).toContainText('LED cap: 26/255');
  await expect(page.locator('#power-safety-pill')).toContainText('Rail: UNVERIFIED');

  await page.getByRole('button', { name: 'Advanced' }).click();
  await expect(page.locator('#power-safety-pill')).toBeHidden();
  await expect(page.locator('#device-monitor')).toContainText('Power profile');
  await expect(page.locator('#device-monitor')).toContainText('POWER_CHOKED_V1');
  await expect(page.locator('#device-monitor')).toContainText('LED cap');
  await expect(page.locator('#device-monitor')).toContainText('26/255');
  await expect(page.locator('#device-monitor')).toContainText('Rail verified');
  await expect(page.locator('#device-monitor')).toContainText('no');
});
