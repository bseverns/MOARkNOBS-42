import { test, expect } from '@playwright/test';

async function bootConfigureSimulator(page) {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'basic');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });
  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toHaveText('Connected');
}

test('Configure translates selected-slot tuning into staged musician-facing actions', async ({ page }) => {
  await bootConfigureSimulator(page);

  const tuning = page.getByRole('group', { name: 'Tune This Slot' });
  await expect(tuning).toBeVisible();
  await expect(tuning.getByLabel(/Reactive assignment/)).toHaveValue('0');
  await expect(tuning.locator('.tuning-character-current')).toContainText('Literal · LINEAR');
  await expect(tuning.locator('.tuning-response-summary')).toContainText('Response');
  await expect(tuning.locator('.tuning-amount-summary')).toContainText('Amount');
  const direction = tuning.locator('label:has(.control-label:text-is("Direction")) select');
  await expect(direction).toHaveValue('add_clamp');

  const before = await page.evaluate(() => {
    const slot = window.__MN42_RUNTIME.getState().staged.slots[0];
    return {
      data1: slot.data1,
      baseline: slot.ef.baseline,
      gain: slot.ef.gain,
      source: slot.efIndex
    };
  });

  await tuning.getByRole('button', { name: 'Smooth', exact: true }).click();
  await expect(page.locator('#dirty-badge')).toBeVisible();
  await expect(tuning.locator('.tuning-recipe-result')).toContainText('Smooth');
  await expect(tuning.locator('.tuning-recipe-result')).toContainText('LOWPASS');

  const afterRecipe = await page.evaluate(() => {
    const slot = window.__MN42_RUNTIME.getState().staged.slots[0];
    return {
      data1: slot.data1,
      baseline: slot.ef.baseline,
      gain: slot.ef.gain,
      source: slot.efIndex,
      filter: slot.ef.filter_name,
      oversample: slot.ef.oversample,
      smoothing: slot.ef.smoothing
    };
  });
  expect(afterRecipe).toEqual({
    ...before,
    filter: 'LOWPASS',
    oversample: 8,
    smoothing: 0.12
  });

  await direction.selectOption('subtract');
  await expect
    .poll(() => page.evaluate(() => window.__MN42_RUNTIME.getState().staged.slots[0].ef.destination_mode))
    .toBe('subtract');

  await tuning.getByLabel(/Reactive assignment/).selectOption('2');
  await expect
    .poll(() => page.evaluate(() => {
      const slot = window.__MN42_RUNTIME.getState().staged.slots[0];
      return [slot.efIndex, slot.ef.index];
    }))
    .toEqual([2, 2]);
});

test('Configure places live EF and resolved slot evidence beside tuning', async ({ page }) => {
  await bootConfigureSimulator(page);

  const evidence = page.locator('.selected-tuning-evidence');
  await expect(evidence).toBeVisible();
  await expect(evidence).toContainText('EF 1');
  await expect(evidence).toContainText(/Current \d+/);
  await expect(evidence).toContainText(/(Active|Recent|Inactive)/);
  await expect(evidence).toContainText(/Slot output \d+/);
});

test('Configure can stage the pre-Apply state without issuing an immediate device write', async ({ page }) => {
  await bootConfigureSimulator(page);
  const original = await page.evaluate(() => window.__MN42_RUNTIME.getState().live.slots[0].data1);
  const dataInput = page.locator('.slot-editor label:has-text("CC number") input').first();
  await dataInput.fill(String(original + 7));
  await dataInput.dispatchEvent('change');
  await page.locator('#apply').click();
  await expect(page.locator('#status-label')).toHaveText('Synced');

  const tuning = page.getByRole('group', { name: 'Tune This Slot' });
  const returnButton = tuning.getByRole('button', { name: 'Return to pre-Apply state' });
  await expect(returnButton).toBeVisible();
  await returnButton.click();

  await expect(page.locator('#status-label')).toHaveText('Previous state staged');
  await expect(page.locator('#dirty-badge')).toBeVisible();
  const state = await page.evaluate(() => window.__MN42_RUNTIME.getState());
  expect(state.live.slots[0].data1).toBe(original + 7);
  expect(state.staged.slots[0].data1).toBe(original);
});

test('Lab keeps exact controls, technical tokens, recipes, and deck shortcut hints', async ({ page }) => {
  await bootConfigureSimulator(page);
  const tuning = page.getByRole('group', { name: 'Tune This Slot' });
  await tuning.getByRole('button', { name: 'Customize in Lab' }).click();

  await expect(page.getByRole('button', { name: 'Lab', exact: true })).toHaveAttribute('aria-pressed', 'true');
  await expect(page.getByRole('tab', { name: 'Envelope' })).toHaveAttribute('aria-selected', 'true');
  await expect(page.getByLabel('Response shape')).toBeVisible();
  await expect(page.getByLabel('Response shape').locator('option[value="LOWPASS"]')).toHaveText(
    'Smooth · LOWPASS'
  );
  await expect(page.locator('.slot-editor')).toContainText('Deck shortcut: Ctrl3 double');

  await page.getByRole('tab', { name: 'ARG', exact: true }).click();
  const argPanel = page.locator('.slot-editor');
  const sourcesBefore = await page.evaluate(() => {
    const arg = window.__MN42_RUNTIME.getState().staged.slots[0].arg;
    return [arg.sourceA, arg.sourceB];
  });
  await argPanel.getByRole('button', { name: 'Strongest Wins', exact: true }).click();
  await expect(page.getByLabel('Combine method').locator('option[value="MAXX"]')).toHaveText(
    'Strongest Wins · MAXX'
  );
  await expect(argPanel).toContainText('Deck shortcut: Ctrl4 double');
  const argAfter = await page.evaluate(() => window.__MN42_RUNTIME.getState().staged.slots[0].arg);
  expect(argAfter).toMatchObject({ enabled: true, method: 11, method_name: 'MAXX' });
  expect([argAfter.sourceA, argAfter.sourceB]).toEqual(sourcesBefore);

  await page.getByRole('tab', { name: 'Slot LFO', exact: true }).click();
  const lane = page.locator('.slot-lfo-lane').first();
  await lane.getByRole('button', { name: 'Subtle Centered Motion', exact: true }).click();
  await expect(lane.getByLabel('Enable LFO 1')).toBeChecked();
  await expect(lane.getByLabel('LFO 1 combine mode')).toHaveValue('4');
  await expect(lane.getByLabel('LFO 1 amount (%)', { exact: true })).toHaveValue('25');
  await expect(lane).toContainText('Deck shortcut: Ctrl5 double');
});
