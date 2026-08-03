import { test, expect } from '@playwright/test';

async function bootSimulator(page) {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');
  await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Simulator');
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('.slot-editor')).toBeVisible();
}

async function expectBrowserOnlyStateClean(page) {
  await expect(page.locator('#dirty-badge')).toBeHidden();
  await expect(page.locator('#apply')).toBeDisabled();
  const state = await page.evaluate(() => ({
    dirty: window.__MN42_RUNTIME.getState().dirty,
    diff: window.__MN42_RUNTIME.diff()
  }));
  expect(state.dirty).toBe(false);
  expect(state.diff).toEqual([]);
}

test('changing a browser-only slot label does not dirty staged firmware config', async ({
  page
}) => {
  await bootSimulator(page);

  const labelInput = page
    .locator('.slot-editor label:has-text("Slot label (browser only)") input')
    .first();
  await labelInput.fill('Verse cue');
  await labelInput.dispatchEvent('change');

  await expectBrowserOnlyStateClean(page);
});

test('changing browser-only Take Control metadata does not require Apply', async ({ page }) => {
  await bootSimulator(page);

  const takeoverToggle = page
    .locator('.slot-editor label:has-text("Take Control (browser only)") input')
    .first();
  await takeoverToggle.check();

  await expectBrowserOnlyStateClean(page);

  await page.evaluate(async () => {
    await window.__MN42_RUNTIME.disconnect();
  });
  await expect(page.locator('#connection-pill')).toContainText('Disconnected');

  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');
  await expect(
    page.locator('.slot-editor label:has-text("Take Control (browser only)") input').first()
  ).toBeChecked();
  await expectBrowserOnlyStateClean(page);
});

test('only the selected slot takeover shortcut participates in keyboard order', async ({ page }) => {
  await bootSimulator(page);

  await expect(page.locator('#slots .takeover[tabindex="0"]')).toHaveCount(1);
  await expect(page.locator('.slot-button[data-index="0"] .takeover')).toHaveAttribute(
    'tabindex',
    '0'
  );

  await page.locator('.slot-button[data-index="5"]').click();

  await expect(page.locator('#slots .takeover[tabindex="0"]')).toHaveCount(1);
  await expect(page.locator('.slot-button[data-index="0"] .takeover')).toHaveAttribute(
    'tabindex',
    '-1'
  );
  await expect(page.locator('.slot-button[data-index="5"] .takeover')).toHaveAttribute(
    'tabindex',
    '0'
  );
});
