import { test, expect } from '@playwright/test';

function useSimulator(page) {
  return page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });
}

async function bootWithSimulator(page) {
  await page.waitForFunction(() => document.documentElement.dataset.mn42Booted === 'true');
  await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Simulator');
}

test('matrix LFO rows select the matching route card', async ({ page }) => {
  await useSimulator(page);
  await page.goto('/benzknobz.html');
  await bootWithSimulator(page);
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  await page.locator('[data-utility-tab="lfo"]').click();
  await expect(page.locator('[data-utility-panel="lfo"]')).toBeVisible();
  await expect(page.locator('#lfo-status')).toContainText('2 LFOs');

  await page.locator('#mod-matrix-refresh').click();

  const lfoRow = page
    .locator('#mod-matrix-body tbody tr')
    .filter({ has: page.locator('td').filter({ hasText: /^lfo0$/ }) })
    .first();
  await expect(lfoRow).toBeVisible();
  await lfoRow.click();

  await expect(page.locator('#lfo-editor [data-route-index="0"]')).toHaveAttribute(
    'data-selected',
    'true'
  );
  await expect(lfoRow).toHaveAttribute('data-selected', 'true');
  await expect(page.locator('#mod-matrix-status')).toContainText('Selected Route 1');
});

test('matrix discloses when the route preview is capped', async ({ page }) => {
  await useSimulator(page);
  await page.goto('/benzknobz.html');
  await bootWithSimulator(page);
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  await page.locator('[data-utility-tab="lfo"]').click();
  await expect(page.locator('[data-utility-panel="lfo"]')).toBeVisible();

  await page.evaluate(() => {
    const runtime = window.__MN42_RUNTIME;
    const originalSendRpc = runtime.sendRpc.bind(runtime);
    runtime.sendRpc = async (message, options) => {
      if (message?.rpc === 'get_mod_matrix') {
        return {
          command: 'GET_MOD_MATRIX',
          contract_version: 1,
          routes: Array.from({ length: 81 }, (_, index) => ({
            id: `lfo0_route${index}`,
            source: 'lfo0',
            source_type: 'lfo',
            transform: 'test route',
            destination: `slot${index}.value`,
            mode: 'replace',
            amount: 100,
            range: { min: 0, max: 127 },
            active: true,
            last_value: 64
          })),
          conflicts: []
        };
      }
      return originalSendRpc(message, options);
    };
  });

  await page.locator('#mod-matrix-refresh').click();
  await expect(page.locator('#mod-matrix-body')).toContainText('Showing 80 of 81');
  await expect(page.locator('#mod-matrix-body tbody tr')).toHaveCount(80);
});
