import { test, expect } from '@playwright/test';
import { describeSlotSignal } from '../views/slot_signal_path.js';

async function boot(page, mode = 'basic') {
  const errors = [];
  page.on('pageerror', (error) => errors.push(error.stack));
  page.on('console', (message) => {
    if (message.text().includes('runtime listener error')) errors.push(message.text());
  });
  await page.addInitScript((uiMode) => {
    localStorage.clear();
    localStorage.setItem('moarknobs:ui-mode', uiMode);
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  }, mode);
  await page.goto('/');
  await page.getByRole('button', { name: 'Connect', exact: true }).click();
  await expect(page.locator('#connection-pill')).toHaveText('Connected');
  expect(errors).toEqual([]);
  await expect(page.locator('.slot-editor')).toBeVisible();
  return errors;
}

test('selected slot explains measured contributions without synthesizing missing evidence', async ({
  page
}) => {
  const errors = await boot(page);
  await expect(page.locator('#selected-slot-signal')).toBeVisible();
  await expect(page.locator('[data-configure-zone=envelope]')).toBeVisible();
  await expect(page.locator('[data-configure-zone=arg]')).toBeVisible();
  await expect(page.locator('[data-configure-zone=lfo]')).toBeVisible();
  await expect(page.locator('.configure-modulation > [data-configure-zone=arg]')).toHaveCount(0);
  await expect(page.locator('[data-configure-zone=envelope] [data-configure-zone=arg]')).toBeVisible();
  await expect(page.locator('.shared-generator-summary').first()).toContainText('shared generator');
  expect(errors).toEqual([]);
});

test('causal signal links focus the matching reactive or motion controls', async ({ page }) => {
  const errors = await boot(page);
  await page.getByRole('button', { name: 'Focus Reactive · EF / ARG controls' }).click();
  await expect(page.locator('[data-configure-zone=envelope]').getByLabel('Source')).toBeFocused();
  await page.getByRole('button', { name: 'Focus LFO 2 controls' }).click();
  await expect(page.getByRole('checkbox', { name: 'Use LFO 2', exact: true })).toBeFocused();
  await page.getByRole('button', { name: 'Edit generator in Lab →', exact: true }).first().click();
  await expect(
    page.getByRole('tab', { name: 'Profile LFO & Routes', exact: true })
  ).toHaveAttribute('aria-selected', 'true');
  await expect(page.locator('#lfo-editor .lfo-section').first().locator('select').first()).toBeFocused();
  expect(errors).toEqual([]);
});

test('signal evidence handles startup, stale snapshots, and ARG as one reactive contribution', () => {
  expect(describeSlotSignal({ telemetry: null }).measured).toBe(false);
  const telemetry = {
    receivedAt: 10_000,
    slots: [64],
    slotOutputs: [103],
    slotContributions: [
      { index: 0, baseline: 64, ef: 31, lfos: [12, -4], output: 103, activeMask: 7 }
    ]
  };
  const signal = describeSlotSignal({
    telemetry,
    slot: { efIndex: 0, arg: { enabled: true, sourceA: 0, sourceB: 3 } },
    connected: true,
    now: 10_200
  });
  expect(signal).toMatchObject({
    baseline: 64,
    reactive: 31,
    lfos: [12, -4],
    output: 103,
    measured: true
  });
  expect(signal.source).toContain('EF 1 + EF 4 → EF shaping');
  expect(describeSlotSignal({ telemetry, connected: true, now: 15_000 })).toMatchObject({
    freshness: 'Stale telemetry',
    reactive: null,
    measured: false
  });
  expect(
    describeSlotSignal({
      telemetry: { ...telemetry, slotContributions: [] },
      connected: true,
      now: 10_200
    })
  ).toMatchObject({ baseline: 64, output: 103, reactive: null });
  expect(
    describeSlotSignal({
      telemetry: {
        ...telemetry,
        slotContributions: [{ ...telemetry.slotContributions[0], output: 50 }]
      },
      connected: true,
      now: 10_200
    }).measured
  ).toBe(false);
  expect(describeSlotSignal({ telemetry, connected: false, now: 10_200 })).toMatchObject({
    freshness: 'Disconnected',
    output: null,
    reactive: null
  });
});

test('ARG and both motion lanes stage only the selected slot and keep place in Lab', async ({
  page
}) => {
  const errors = await boot(page);
  await page.locator('#slots [data-index="11"]').click();
  const before = await page.evaluate(() => structuredClone(window.__MN42_RUNTIME.getState().live));
  const combine = page.locator('[data-configure-zone=arg]');
  await combine.getByRole('button', { name: 'Strongest Wins', exact: true }).click();
  await expect(combine.locator('.recipe-explanation')).toContainText('stronger source');
  const motion = page.locator('.configure-lfo-lane').nth(1);
  await motion.locator('summary').click();
  await motion.getByRole('button', { name: 'Subtle Centered Motion', exact: true }).click();
  await expect(motion.getByLabel('Use LFO 2')).toBeChecked();
  await expect(motion.locator('.staged-control-marker:visible')).not.toHaveCount(0);
  const after = await page.evaluate(() => structuredClone(window.__MN42_RUNTIME.getState()));
  expect(after.live).toEqual(before);
  expect(after.staged.slots[11].arg).toMatchObject({ enabled: true, method: 11 });
  expect(after.staged.slots[11].lfo[1]).toMatchObject({ enabled: true, mode: 4, amount: 25 });
  expect(after.staged.slots[10]).toEqual(before.slots[10]);
  await motion.getByRole('button', { name: 'Customize in Lab' }).click();
  await expect(page.getByRole('tab', { name: 'Slot LFO', exact: true })).toHaveAttribute(
    'aria-selected',
    'true'
  );
  await expect(page.locator('.slot-lfo-lane').nth(1).getByLabel('Enable LFO 2')).toBeFocused();
  await page.locator('#return-configure').click();
  await expect(page.locator('.configure-lfo-lane').nth(1).getByLabel('Use LFO 2')).toBeFocused();
  await expect(page.locator('.slot-signal-heading')).toContainText('12 ·');
  expect(errors).toEqual([]);
});

test('Review focuses the exact staged field and logarithmic Lab sliders reset to confirmed values', async ({
  page
}) => {
  const errors = await boot(page);
  await page
    .locator('[data-configure-zone=envelope]')
    .locator(':scope > .tuning-customize')
    .click();
  const attack = page.getByRole('spinbutton', { name: 'Attack time (ms)', exact: true });
  const original = Number(await attack.inputValue());
  await attack.fill('25');
  await attack.dispatchEvent('change');
  await page.locator('#change-review').click();
  const row = page.locator('.change-review-row').filter({ hasText: 'Attack (ms)' });
  await expect(row).toContainText('25');
  await row.getByRole('button', { name: 'Focus field' }).click();
  await expect(attack).toBeFocused();
  const slider = page.getByRole('slider', { name: 'Attack time (ms) slider', exact: true });
  await slider.fill('500');
  await slider.dispatchEvent('input');
  expect(Number(await attack.inputValue())).toBeCloseTo(Math.sqrt(60_000), 0);
  await page
    .getByRole('button', { name: 'Reset Attack time (ms) to confirmed value', exact: true })
    .click();
  await expect(attack).toHaveValue(String(original));
  await expect(page.locator('#change-bar')).toBeHidden();
  const state = await page.evaluate(() => window.__MN42_RUNTIME.getState());
  expect(state.dirty).toBe(false);
  expect(errors).toEqual([]);
});

test('State drawer groups starting points, device memory, and file backups without writing on open', async ({
  page
}) => {
  const errors = await boot(page);
  const before = await page.evaluate(() => structuredClone(window.__MN42_RUNTIME.getState().live));
  const drawer = page.locator('#recovery-drawer');
  await drawer.locator('summary').click();
  for (const id of [
    'preset-picker',
    'profile-save',
    'profile-load',
    'profile-download',
    'profile-upload',
    'export-preset',
    'import-preset'
  ]) {
    await expect(drawer.locator(`#${id}`)).toBeVisible();
  }
  expect(await page.evaluate(() => window.__MN42_RUNTIME.getState().live)).toEqual(before);
  expect(await page.evaluate(() => window.__MN42_RUNTIME.getState().dirty)).toBe(false);
  expect(errors).toEqual([]);
});

test('musician workspace stays usable at desktop and phone widths with the Apply dock visible', async ({
  page
}) => {
  const errors = await boot(page);
  await page.setViewportSize({ width: 1440, height: 1000 });
  await page
    .locator('[data-configure-zone=arg]')
    .getByRole('button', { name: 'Strongest Wins', exact: true })
    .click();
  await expect(page.locator('#change-bar')).toBeVisible();
  await page.evaluate(() => window.scrollTo(0, 0));
  await page.screenshot({ path: '/tmp/mn42-app-configure-desktop.png', fullPage: true });
  await page.setViewportSize({ width: 390, height: 844 });
  const depth = page.getByRole('spinbutton', { name: 'LFO 2 depth (%)', exact: true });
  await depth.focus();
  await expect(depth).toBeInViewport();
  const field = await depth.boundingBox();
  const dock = await page.locator('#change-bar').boundingBox();
  expect(field.y + field.height).toBeLessThanOrEqual(dock.y);
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(
    true
  );
  await page.screenshot({ path: '/tmp/mn42-app-configure-mobile.png', fullPage: true });
  expect(errors).toEqual([]);
});
