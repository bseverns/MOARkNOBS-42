import { test, expect } from '@playwright/test';

test('panic help dialog surfaces recovery actions and firmware lanes on demand', async ({
  page
}) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: /simulator/i }).click();
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  await page.getByRole('button', { name: 'Panic Help' }).click();
  await expect(page.locator('#panic-help-dialog')).toHaveAttribute('open', '');
  await expect(page.locator('#panic-help-context')).toContainText('Simulator is active');
  await expect(page.locator('#panic-help-preflight')).toContainText('Connected');
  await expect(page.locator('#panic-help-preflight')).toContainText('backup ready');
  await expect(page.locator('#panic-help-export-config')).toBeEnabled();
  await expect(page.locator('#panic-help-export-log')).toBeEnabled();
  await expect(page.locator('#panic-help-config-boot')).toBeDisabled();
  await expect(page.locator('#panic-help-dialog')).toContainText(
    'teensy_loader_cli -mmcu=teensy40 -w mn42-firmware.hex'
  );
  await expect(page.locator('#panic-help-dialog')).toContainText(
    'pio run -d firmware -t upload -e teensy40_main'
  );

  await page.locator('#panic-help-export-config').click();
  await expect(page.locator('#status-label')).toHaveText('Config exported');
  await expect(page.locator('.status-message')).toContainText('moarknobz-config-');

  await page.locator('#panic-help-export-log').evaluate((element) => {
    element.click();
  });
  await expect(page.locator('#log')).toContainText('Log exported');
});
