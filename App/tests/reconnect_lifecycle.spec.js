import { test, expect } from '@playwright/test';

test('reconnect closes the active transport before opening one replacement', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.__transportLifecycle = [];
    window.__transportCount = 0;
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    window.__MN42_TEST_HOOKS = {
      mutateTransport(transport) {
        const id = ++window.__transportCount;
        const originalOpen = transport.open.bind(transport);
        const originalClose = transport.close.bind(transport);

        transport.open = async () => {
          window.__transportLifecycle.push(`open:${id}:start`);
          await originalOpen();
          window.__transportLifecycle.push(`open:${id}:end`);
        };
        transport.close = async () => {
          window.__transportLifecycle.push(`close:${id}:start`);
          await new Promise((resolve) => setTimeout(resolve, 25));
          await originalClose();
          window.__transportLifecycle.push(`close:${id}:end`);
        };
      }
    };
  });

  await page.goto('/benzknobz.html');
  await page.locator('#cold-start-simulator').click();
  await expect(page.locator('#connection-pill')).toHaveText('Connected');
  await expect(page.locator('#connect')).toHaveText('Reconnect');

  const result = await page.evaluate(async () => {
    const first = window.__MN42_RUNTIME.connect();
    const second = window.__MN42_RUNTIME.connect();
    const sharedAttempt = first === second;
    await Promise.all([first, second]);
    return {
      sharedAttempt,
      transportCount: window.__transportCount,
      lifecycle: [...window.__transportLifecycle]
    };
  });

  expect(result.sharedAttempt).toBe(true);
  expect(result.transportCount).toBe(2);
  expect(result.lifecycle.indexOf('close:1:end')).toBeLessThan(
    result.lifecycle.indexOf('open:2:start')
  );
  await expect(page.locator('#connection-pill')).toHaveText('Connected');
  await expect(page.locator('#connect')).toHaveText('Reconnect');
  await expect(page.locator('#stage-connect')).toHaveText('Reconnect');
});

test('interruption preserves workspace and staged intent until reconnect readback', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true, simulatorFixture: 'demo' };
  });
  await page.goto('/benzknobz.html');
  await page.locator('#cold-start-simulator').click();
  await expect(page.locator('#connection-pill')).toHaveText('Connected');
  await page.locator('.workspace-tool-shell > summary').click();
  await page.getByRole('button', { name: 'Device Monitor', exact: true }).click();
  await page.getByRole('button', { name: 'Profile Performance', exact: true }).click();
  await page.getByRole('button', { name: 'EF Controls', exact: true }).click();
  await page.locator('[data-performance-tab="lfo"]').click();
  const input = page.locator('.ef-modulation-cluster input[type="number"]').first();
  await input.fill('123');
  await input.dispatchEvent('change');
  await expect(page.locator('#dirty-badge')).toBeVisible();
  await expect(page.locator('#apply')).toBeEnabled();

  await page.evaluate(() => window.__MN42_RUNTIME.disconnect());
  await expect(page.locator('#connection-pill')).toContainText('Disconnected');
  await expect(page.locator('#device-monitor-section')).toBeVisible();
  await expect(page.locator('[data-performance-panel="lfo"]')).toBeVisible();
  await expect(page.locator('#apply')).toBeDisabled();

  await page.locator('#connect').click();
  await expect(page.locator('#connection-pill')).toHaveText('Connected');
  await expect(page.locator('#device-monitor-section')).toBeVisible();
  await expect(page.locator('[data-performance-panel="lfo"]')).toBeVisible();
  await expect(page.locator('#apply')).toBeEnabled();
  await expect(page.locator('#dirty-badge')).toBeVisible();
});
