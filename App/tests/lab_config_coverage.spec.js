import { test, expect } from '@playwright/test';
import fs from 'node:fs';

const schema = JSON.parse(
  fs.readFileSync(new URL('../config_schema.json', import.meta.url), 'utf8')
);
const contract = JSON.parse(
  fs.readFileSync(new URL('../../interop/mn42_contract.json', import.meta.url), 'utf8')
);

function schemaLeaves(node, path) {
  if (node?.type === 'object' && node.properties) {
    return Object.entries(node.properties).flatMap(([key, child]) =>
      schemaLeaves(child, `${path}.${key}`)
    );
  }
  if (node?.type === 'array') return schemaLeaves(node.items ?? {}, `${path}.*`);
  return [path];
}

async function bootLabSimulator(page) {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });
  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');
  await expect(page.locator('.slot-editor')).toBeVisible();
}

async function collectSlotPaths(page, collected) {
  const paths = await page.locator('[data-device-config-path]').evaluateAll((elements) =>
    elements.flatMap((element) =>
      String(element.dataset.deviceConfigPath ?? '')
        .split(/\s+/)
        .filter((path) => path.startsWith('slots.*.'))
    )
  );
  paths.forEach((path) => collected.add(path));
}

function selectedSlotControl(page, label) {
  return page
    .locator(`.slot-editor label:has(.control-label:text-is("${label}"))`)
    .locator('input, select');
}

async function setSelectedSlotEfMode(page, mode) {
  await page.evaluate((nextMode) => {
    const runtime = window.__MN42_RUNTIME;
    runtime.stage((draft) => {
      draft.slots[0].ef = { ...draft.slots[0].ef, mode: nextMode };
      return draft;
    });
  }, mode);
  await page.getByRole('tab', { name: 'Mapping', exact: true }).click();
  await page.getByRole('tab', { name: 'Envelope', exact: true }).click();
}

test('Lab exposes every device configuration root and the legacy mode stays separate', async ({
  page
}) => {
  await bootLabSimulator(page);

  const excluded = new Set(contract.device_schema.exclude_properties ?? []);
  const expectedRoots = Object.keys(schema.properties)
    .filter((root) => !excluded.has(root))
    .sort();
  const actualRoots = await page.locator('[data-device-config-root]').evaluateAll((elements) =>
    elements.map((element) => element.dataset.deviceConfigRoot).sort()
  );

  expect(actualRoots).toEqual(expectedRoots);
  for (const root of expectedRoots) {
    await expect(page.locator(`[data-device-config-root="${root}"]`)).toBeVisible();
  }

  const slotModeBefore = await page.evaluate(
    () => window.__MN42_RUNTIME.getState().staged.slots[0].ef.mode
  );
  await page.locator('#envelope-mode-settings select').selectOption('LOG');
  await expect
    .poll(() => page.evaluate(() => window.__MN42_RUNTIME.getState().staged.envelopeMode))
    .toBe('LOG');
  expect(
    await page.evaluate(() => window.__MN42_RUNTIME.getState().staged.slots[0].ef.mode)
  ).toBe(slotModeBefore);
});

test('Lab selected-slot editor covers every slot schema leaf', async ({ page }) => {
  await bootLabSimulator(page);
  const covered = new Set();

  await page.evaluate(() => {
    const runtime = window.__MN42_RUNTIME;
    runtime.stage((draft) => {
      draft.slots[0] = { ...draft.slots[0], type: 'SysEx' };
      return draft;
    });
  });
  await page.getByRole('tab', { name: 'Envelope', exact: true }).click();
  await page.getByRole('tab', { name: 'Mapping', exact: true }).click();
  await expect(selectedSlotControl(page, 'SysEx Template')).toBeVisible();
  await collectSlotPaths(page, covered);

  await page.getByRole('tab', { name: 'Envelope', exact: true }).click();
  await collectSlotPaths(page, covered);

  await setSelectedSlotEfMode(page, 1);
  await expect(selectedSlotControl(page, 'RMS window (ms)')).toBeVisible();
  await collectSlotPaths(page, covered);

  await setSelectedSlotEfMode(page, 2);
  await expect(selectedSlotControl(page, 'Gate threshold')).toBeVisible();
  await collectSlotPaths(page, covered);

  await page.getByRole('tab', { name: 'ARG', exact: true }).click();
  await collectSlotPaths(page, covered);
  await page.getByRole('tab', { name: 'Slot LFO', exact: true }).click();
  await collectSlotPaths(page, covered);

  const expectedPaths = schemaLeaves(schema.properties.slots, 'slots').sort();
  expect([...covered].sort()).toEqual(expectedPaths);
});

test('two slots sharing one follower retain independent detection modes', async ({ page }) => {
  await bootLabSimulator(page);
  await page.evaluate(() => {
    const runtime = window.__MN42_RUNTIME;
    runtime.stage((draft) => {
      for (const index of [0, 1]) {
        draft.slots[index] = {
          ...draft.slots[index],
          efIndex: 0,
          ef: { ...draft.slots[index].ef, index: 0 }
        };
      }
      return draft;
    });
  });

  await page.getByRole('tab', { name: 'Envelope', exact: true }).click();
  await selectedSlotControl(page, 'Detection mode').selectOption('GATE');
  await expect
    .poll(() => page.evaluate(() => window.__MN42_RUNTIME.getState().staged.slots[0].ef.mode))
    .toBe(2);

  await page.locator('#slots [data-index="1"]').click();
  await expect(selectedSlotControl(page, 'Detection mode')).toBeVisible();
  await selectedSlotControl(page, 'Detection mode').selectOption('RMS');
  await expect
    .poll(() => page.evaluate(() => window.__MN42_RUNTIME.getState().staged.slots[1].ef.mode))
    .toBe(1);

  expect(
    await page.evaluate(() =>
      window.__MN42_RUNTIME.getState().staged.slots.slice(0, 2).map((slot) => ({
        follower: slot.ef.index,
        mode: slot.ef.mode
      }))
    )
  ).toEqual([
    { follower: 0, mode: 2 },
    { follower: 0, mode: 1 }
  ]);

  await page.locator('#apply').click();
  await expect(page.locator('#status-label')).toHaveText('Synced', { timeout: 5000 });
  expect(
    await page.evaluate(() =>
      window.__MN42_RUNTIME.getState().live.slots.slice(0, 2).map((slot) => slot.ef.mode)
    )
  ).toEqual([2, 1]);
});
