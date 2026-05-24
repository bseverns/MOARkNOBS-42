import { test, expect } from '@playwright/test';

test('session log persists across reload and can be exported and cleared', async ({ page }) => {
  await page.addInitScript(() => {
    if (!window.sessionStorage?.getItem?.('mn42-session-log-test-init')) {
      window.localStorage?.clear?.();
      window.sessionStorage?.setItem?.('mn42-session-log-test-init', '1');
    }
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: /simulator/i }).click();
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  await page.evaluate(async () => {
    await window.__MN42_RUNTIME.disconnect();
  });
  await expect(page.locator('#connection-pill')).toContainText('Disconnected');

  const log = page.locator('#log');
  await expect(log).toContainText('CONNECTED');
  await expect(log).toContainText('DISCONNECTED');

  await page.reload();
  await expect(log).toContainText('CONNECTED');
  await expect(log).toContainText('DISCONNECTED');

  await page.locator('.debug-log-bridge').evaluate((element) => {
    element.open = true;
  });
  const exportButton = page.getByRole('button', { name: 'Export session log' });
  await expect(exportButton).toBeEnabled();
  await page.locator('#session-log-export').evaluate((element) => {
    element.click();
  });
  await expect(log).toContainText('Log exported');

  await page.locator('#session-log-clear').evaluate((element) => {
    element.click();
  });
  await expect(log).toHaveText('No session events yet.');
  await expect(page.locator('#session-log-count')).toHaveText('0 entries stored in this browser');

  await page.reload();
  await expect(log).toHaveText('No session events yet.');
});
