import { test, expect } from '@playwright/test';

test('scope panel streams telemetry and emits snapshots', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = {
      useSimulator: true
    };
    window.__mn42ScopeBlob = null;
    const originalCreate = URL.createObjectURL.bind(URL);
    URL.createObjectURL = (blob) => {
      window.__mn42ScopeBlob = blob;
      return originalCreate(blob);
    };
  });

  await page.goto('/benzknobz.html');
  const simulatorToggle = page.getByRole('button', { name: /simulator/i });
  await simulatorToggle.click();
  await page.getByRole('button', { name: 'Connect' }).click();

  await page.waitForFunction(() => {
    const label = document.getElementById('scope-status');
    return label && /Telemetry/i.test(label.textContent ?? '');
  });
  await expect(page.locator('#scope-status')).toHaveText(/Telemetry/i, { timeout: 10000 });

  await page.getByRole('button', { name: 'PNG snapshot' }).click();
  await page.waitForFunction(() => window.__mn42ScopeBlob instanceof Blob, { timeout: 5000 });
  const snapshot = await page.evaluate(() => ({
    size: window.__mn42ScopeBlob?.size ?? 0,
    type: window.__mn42ScopeBlob?.type ?? ''
  }));
  expect(snapshot.size).toBeGreaterThan(0);
  expect(snapshot.type).toBe('image/png');
});
