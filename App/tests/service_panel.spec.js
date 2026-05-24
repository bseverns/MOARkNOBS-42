import { test, expect } from '@playwright/test';

test('service tab surfaces firmware handoff guidance and backup actions', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: /simulator/i }).click();
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  await page.locator('[data-utility-tab="service"]').click();
  await expect(page.locator('[data-utility-panel="service"]')).toBeVisible();
  await expect(page.locator('#service-verdict')).toContainText('Simulator session only');
  await expect(page.locator('#service-connection-state')).toHaveText('Connected');
  await expect(page.locator('#service-firmware-state')).toContainText('sim-fw');
  await expect(page.locator('#service-backup-state')).toHaveText('Ready');
  await expect(page.locator('#service-export-config')).toBeEnabled();
  await expect(page.locator('#service-export-log')).toBeEnabled();
  await expect(page.locator('#service-config-boot')).toBeDisabled();
  await expect(page.locator('[data-utility-panel="service"]')).toContainText(
    'pio run -d firmware -t upload -e teensy40_main'
  );
  await expect(page.locator('[data-utility-panel="service"]')).toContainText(
    'teensy_loader_cli -mmcu=teensy40 -w mn42-firmware.hex'
  );

  await page.locator('#service-export-config').click();
  await expect(page.locator('#status-label')).toHaveText('Config exported');
  await expect(page.locator('.status-message')).toContainText('moarknobz-config-');

  await page.locator('#service-export-log').evaluate((element) => {
    element.click();
  });
  await expect(page.locator('#log')).toContainText('Log exported');
});
