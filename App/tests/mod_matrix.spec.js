import fs from 'node:fs/promises';
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

async function openMatrixPanel(page) {
  await page.locator('[data-utility-tab="lfo"]').click();
  await expect(page.locator('[data-utility-panel="lfo"]')).toBeVisible();
}

test('matrix LFO rows select the matching route card', async ({ page }) => {
  await useSimulator(page);
  await page.goto('/benzknobz.html');
  await bootWithSimulator(page);
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  await openMatrixPanel(page);
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

  await openMatrixPanel(page);

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

test('matrix filters, export, and copy conflicts reflect the visible route set', async ({
  page
}, testInfo) => {
  await page.addInitScript(() => {
    Object.defineProperty(navigator, 'clipboard', {
      configurable: true,
      value: {
        writeText: async (text) => {
          window.__copiedMatrixConflicts = text;
        }
      }
    });
  });
  await useSimulator(page);
  await page.goto('/benzknobz.html');
  await bootWithSimulator(page);
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');
  await openMatrixPanel(page);

  await page.evaluate(() => {
    const runtime = window.__MN42_RUNTIME;
    const originalSendRpc = runtime.sendRpc.bind(runtime);
    runtime.sendRpc = async (message, options) => {
      if (message?.rpc === 'get_mod_matrix') {
        return {
          type: 'mod_matrix',
          command: 'GET_MOD_MATRIX',
          contract_version: 1,
          routes: [
            {
              id: 'lfo0_route0',
              source: 'lfo0',
              source_type: 'lfo',
              route_type: 'slot_value',
              transform: 'slot wobble',
              destination: 'slot7.value',
              mode: 'replace',
              amount: 100,
              range: { min: 0, max: 127 },
              active: true,
              last_value: 64
            },
            {
              id: 'lfo1_route1',
              source: 'lfo1',
              source_type: 'lfo',
              route_type: 'midi_cc7',
              transform: 'cc wobble',
              destination: 'midi.cc74',
              mode: 'replace',
              channel: 1,
              cc_msb: 74,
              amount: 100,
              range: { min: 0, max: 127 },
              active: true,
              last_value: 92
            },
            {
              id: 'lfo1_route2',
              source: 'lfo1',
              source_type: 'lfo',
              route_type: 'slot_value',
              transform: 'inactive slot test',
              destination: 'slot9.value',
              mode: 'replace',
              amount: 50,
              range: { min: 0, max: 127 },
              active: false,
              last_value: 0
            },
            {
              id: 'ef0_slot7',
              source: 'ef0',
              source_type: 'ef',
              transform: 'envelope',
              destination: 'slot7.value',
              mode: 'add_clamp',
              amount: 100,
              range: { min: 0, max: 127 },
              active: true,
              last_value: 88
            },
            {
              id: 'pot7',
              source: 'pot7',
              source_type: 'pot',
              transform: 'baseline',
              destination: 'midi.cc74',
              mode: 'replace',
              midi: { type: 'CC', channel: 1, cc: 74 },
              amount: 127,
              range: { min: 0, max: 127 },
              active: true,
              last_value: 74
            }
          ],
          conflicts: [
            {
              target: 'slot.value',
              slot: 7,
              writers: 'ef0_slot7, lfo0_route0',
              message: '2 live modulators write slot 7 value'
            },
            {
              target: 'midi.cc',
              channel: 1,
              cc: 74,
              writers: 'pot7, lfo1_route1',
              message: '2 live modulators write CC 74 on channel 1'
            }
          ]
        };
      }
      return originalSendRpc(message, options);
    };
  });

  await page.locator('#mod-matrix-refresh').click();
  await expect(page.locator('#mod-matrix-body tbody tr')).toHaveCount(5);
  await expect(page.locator('#mod-matrix-body')).toContainText('Routes 5/5');
  await expect(page.locator('#mod-matrix-body')).toContainText('Conflicts 2/2');

  await page.locator('#mod-matrix-filter-conflicts').check();
  await expect(page.locator('#mod-matrix-body tbody tr')).toHaveCount(4);

  await page.locator('#mod-matrix-filter-lfo').check();
  await expect(page.locator('#mod-matrix-body tbody tr')).toHaveCount(2);

  await page.locator('#mod-matrix-filter-slot').check();
  await expect(page.locator('#mod-matrix-body tbody tr')).toHaveCount(1);
  await expect(page.locator('#mod-matrix-body')).toContainText('Routes 1/5');
  await expect(page.locator('#mod-matrix-body')).toContainText('Conflicts 1/2');

  await page.locator('#mod-matrix-filter-active').check();
  await expect(page.locator('#mod-matrix-body tbody tr')).toHaveCount(1);

  await page.locator('#mod-matrix-copy-conflicts').click();
  await expect
    .poll(async () => page.evaluate(() => window.__copiedMatrixConflicts))
    .toBe('2 live modulators write slot 7 value');

  const [download] = await Promise.all([
    page.waitForEvent('download'),
    page.locator('#mod-matrix-export').click()
  ]);
  expect(download.suggestedFilename()).toMatch(/^moarknobz-mod-matrix-.*\.json$/);
  const exportPath = testInfo.outputPath('mod-matrix-export.json');
  await download.saveAs(exportPath);
  const payload = JSON.parse(await fs.readFile(exportPath, 'utf8'));
  expect(payload.filters).toEqual({
    conflictsOnly: true,
    lfoOnly: true,
    slotOnly: true,
    activeOnly: true
  });
  expect(payload.summary.route_filtered).toBe(1);
  expect(payload.summary.conflict_filtered).toBe(1);
  expect(payload.visible_routes).toHaveLength(1);
  expect(payload.visible_routes[0].id).toBe('lfo0_route0');
  expect(payload.visible_conflicts).toHaveLength(1);
  expect(payload.visible_conflicts[0].target).toBe('slot.value');
});
