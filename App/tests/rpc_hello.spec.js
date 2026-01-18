import { test, expect } from '@playwright/test';

test('rpc hello handshake exposes status stream', async ({ page }) => {
  await page.addInitScript(() => {
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    window.__mn42StatusLog = [];
  });

  await page.goto('/benzknobz.html');
  await page.waitForFunction(() => !!window.__MN42_RUNTIME);
  await page.evaluate(() => {
    const runtime = window.__MN42_RUNTIME;
    if (!runtime) throw new Error('runtime missing');
    runtime.onStatus((payload) => {
      window.__mn42StatusLog.push(payload);
    });
  });

  const simulatorToggle = page.getByRole('button', { name: /simulator/i });
  await expect(simulatorToggle).toHaveText(/Start simulator/i);
  await simulatorToggle.click();
  await expect(simulatorToggle).toHaveText(/Stop simulator/i);
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  const hello = await page.evaluate(async () => {
    const runtime = window.__MN42_RUNTIME;
    if (!runtime) throw new Error('runtime missing');
    return runtime.sendRpc({ rpc: 'hello' });
  });

  expect(hello).toEqual({ message: 'hello' });
  await page.waitForFunction(() => window.__mn42StatusLog.length > 0);
  const statusLog = await page.evaluate(() => window.__mn42StatusLog);
  expect(Array.isArray(statusLog)).toBe(true);
  expect(statusLog.length).toBeGreaterThan(0);
});
