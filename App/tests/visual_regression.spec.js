import { test, expect } from '@playwright/test';

// These approved images intentionally use the App's native system-ui and
// ui-monospace stacks. Keep one canonical rendering platform rather than
// weakening the pixel threshold to accommodate unrelated OS font rasterizers.
test.beforeEach(() => {
  test.skip(
    process.platform !== 'darwin',
    'Approved App visual baselines use macOS system fonts and pinned Chromium.'
  );
});

const SNAPSHOT_OPTIONS = {
  animations: 'disabled',
  caret: 'hide',
  maxDiffPixelRatio: 0.003,
  maskColor: '#121821'
};

async function openConnected(page, viewport, mode = 'basic') {
  await page.setViewportSize(viewport);
  await page.addInitScript((initialMode) => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', initialMode);
    window.__MN42_RUNTIME_OPTIONS = {
      useSimulator: true,
      simulatorFixture: 'demo',
      workspacePanels: 'minimal'
    };
  }, mode);
  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: 'Connect controller' }).click();
  await expect(page.locator('#connection-pill')).toHaveText('Connected');
}

function dynamicMasks(page) {
  return [
    page.locator('.slot-state'),
    page.locator('.stage-slot-value'),
    page.locator('.meter-value'),
    page.locator('progress'),
    page.locator('canvas'),
    page.locator('.selected-tuning-evidence'),
    page.locator('.signal-contributions'),
    page.locator('.slot-signal-output'),
    page.locator('.signal-source')
  ];
}

test('approved disconnected cold start at 1440 × 1000', async ({ page }) => {
  await page.setViewportSize({ width: 1440, height: 1000 });
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.__MN42_RUNTIME_OPTIONS = { workspacePanels: 'minimal' };
  });
  await page.goto('/benzknobz.html');
  await expect(page).toHaveScreenshot('cold-start-disconnected-1440x1000.png', SNAPSHOT_OPTIONS);
});

test('approved connected workspace at 1440 × 1000', async ({ page }) => {
  await openConnected(page, { width: 1440, height: 1000 });
  await expect(page).toHaveScreenshot('cold-start-connected-1440x1000.png', {
    ...SNAPSHOT_OPTIONS,
    mask: dynamicMasks(page)
  });
});

test('approved Lab Profile Performance with multiple open panels at 1440 × 1000', async ({ page }) => {
  await openConnected(page, { width: 1440, height: 1000 }, 'advanced');
  await page.locator('.workspace-tool-shell > summary').click();
  for (const name of ['Profile Performance', 'Device Monitor', 'Scope']) {
    await page.getByRole('button', { name, exact: true }).click();
  }
  await page.locator('[data-performance-tab="incoming"]').click();
  await expect(page).toHaveScreenshot('lab-profile-performance-multiple-open-1440x1000.png', {
    ...SNAPSHOT_OPTIONS,
    mask: dynamicMasks(page)
  });
});
