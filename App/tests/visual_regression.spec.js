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

async function openMode(page, mode, viewport, { freeze = true } = {}) {
  await page.setViewportSize(viewport);
  await page.addInitScript((nextMode) => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', nextMode);
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  }, mode);
  await page.goto(`/?mode=${mode}`);
  await (mode === 'stage'
    ? page.locator('#stage-connect')
    : page.getByRole('button', { name: 'Connect' })).click();
  await expect(page.locator('#connection-pill')).toHaveText('Connected');

  // Freeze simulator timers after hydration. This keeps the approved images
  // focused on visual hierarchy rather than synthetic telemetry phase.
  if (freeze) await page.clock.pauseAt('2026-08-25T12:00:00-05:00');
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

test('approved Configure hierarchy at 1440 × 1000', async ({ page }) => {
  await openMode(page, 'basic', { width: 1440, height: 1000 });
  await expect(page).toHaveScreenshot('configure-1440x1000.png', {
    ...SNAPSHOT_OPTIONS,
    mask: dynamicMasks(page)
  });
});

test('approved Lab hierarchy at 1920 × 1080', async ({ page }) => {
  await openMode(page, 'advanced', { width: 1920, height: 1080 });
  await expect(page).toHaveScreenshot('lab-1920x1080.png', {
    ...SNAPSHOT_OPTIONS,
    mask: dynamicMasks(page)
  });
});

test('approved Stage hierarchy at 1440 × 1000', async ({ page }) => {
  await openMode(page, 'stage', { width: 1440, height: 1000 });
  await expect(page).toHaveScreenshot('stage-1440x1000.png', {
    ...SNAPSHOT_OPTIONS,
    mask: dynamicMasks(page)
  });
});

test('approved open Incoming MIDI workspace at 1356 px', async ({ page }) => {
  await openMode(page, 'advanced', { width: 1356, height: 1700 }, { freeze: false });
  await page.getByRole('tab', { name: 'Incoming MIDI', exact: true }).click();
  const workspace = page.locator('#profile-performance-workspace');
  await workspace.getByRole('button', { name: 'Add binding' }).click();
  await expect(workspace.locator('.midi-binding-card')).toBeVisible();
  await page.locator('#apply').click();
  await expect(page.locator('#status-label')).toHaveText('Synced');
  await page.clock.pauseAt('2026-08-25T12:00:00-05:00');
  await expect(workspace).toHaveScreenshot('incoming-midi-open-1356.png', SNAPSHOT_OPTIONS);
});
