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
    .locator('.slot-editor label:has-text("Slot label") input')
    .first();
  await labelInput.fill('Verse cue');
  await labelInput.dispatchEvent('change');

  await expectBrowserOnlyStateClean(page);
  await page.getByRole('button', { name: 'Stage', exact: true }).click();
  await expect(page.locator('#stage-slots .stage-slot-cell').first()).toContainText('Verse cue');
  await expect(page.locator('#stage-slots .stage-slot-cell').first()).toHaveAttribute(
    'aria-label',
    'Verse cue, slot 1'
  );
  await expect(page.locator('#stage-slot-focus')).toContainText('Verse cue · S1');
});

test('slot UI does not advertise unsupported browser pickup behavior', async ({ page }) => {
  await bootSimulator(page);

  await expect(page.getByText('Take Control', { exact: false })).toHaveCount(0);
  await expect(page.locator('#slots .takeover')).toHaveCount(0);
  await expect(page.locator('#slots .slot-button button')).toHaveCount(0);
  expect(await page.evaluate(() => window.__MN42_RUNTIME.setPotGuard)).toBeUndefined();
});

test('Configure keeps browser presentation metadata out of slot behavior', async ({ page }) => {
  await bootSimulator(page);

  await expect(page.getByText('Knob sends MIDI badge', { exact: false })).toHaveCount(0);
  await expect(page.getByLabel('Slot label')).toBeVisible();
});
