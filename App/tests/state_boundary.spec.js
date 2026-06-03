import { test, expect } from '@playwright/test';

test('live runtime controls do not dirty staged config or require Apply', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');
  await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Simulator');
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');
  await page.locator('#slots [data-index="1"]').click();

  await expect(page.locator('#note-dynamics-apply')).toBeEnabled();
  await page.locator('#note-dynamics-velocity').fill('12');
  await page.locator('#note-dynamics-probability').fill('83');
  await page.locator('#note-dynamics-apply').click();
  await expect(page.locator('#status-label')).toHaveText('Note dynamics updated');

  await expect(page.locator('#dirty-badge')).toBeHidden();
  await expect(page.getByRole('button', { name: 'Apply' })).toBeDisabled();

  const state = await page.evaluate(() => ({
    dirty: window.__MN42_RUNTIME.getState().dirty,
    diff: window.__MN42_RUNTIME.diff()
  }));
  expect(state.dirty).toBe(false);
  expect(state.diff).toEqual([]);
});

test('failed Apply restores staged and live truth and updates error status UI', async ({
  page
}) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__nativeWrites = [];
    window.__nativeSetAllPayload = null;
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    window.__MN42_TEST_HOOKS = {
      mutateTransport(transport) {
        transport.protocol = 'native';
        const lineQueue = [];
        let resolver = null;
        let setAllBuffer = '';
        const pushLine = (line) => {
          if (resolver) {
            const pending = resolver;
            resolver = null;
            pending(line);
            return;
          }
          lineQueue.push(line);
        };
        const manifest = {
          device_name: 'MOARkNOBS-42',
          fw_version: 'native-fw',
          git_sha: 'abc12345',
          build_time: '2026-03-28T12:00:00Z',
          schema_version: 6,
          slot_count: 42,
          pot_count: 42,
          envelope_count: 6,
          arg_method_count: 14,
          led_count: 12,
          capabilities: {
            profile_save: false,
            profile_load: false,
            profile_reset: false,
            macro_snapshot: false,
            scenes: false
          }
        };
        const schema = {
          schema_version: 6,
          type: 'object',
          required: ['slots', 'efSlots', 'filter', 'arg', 'led'],
          properties: {
            slots: { type: 'array', items: { type: 'object' } },
            efSlots: { type: 'array', items: { type: 'object' } },
            filter: { type: 'object' },
            arg: { type: 'object' },
            led: { type: 'object' }
          }
        };
        let config = {
          slots: Array.from({ length: 42 }, (_, index) => ({
            index,
            type: 'CC',
            channel: (index % 16) + 1,
            data1: index % 128,
            active: true,
            ef_index: index % 6
          })),
          efSlots: Array.from({ length: 6 }, (_, index) => ({ index, slots: [index] })),
          filter: { type: 'LOWPASS', freq: 800, q: 1 },
          arg: { enabled: false, method: 0, a: 0, b: 1 },
          led: { brightness: 64, rgb: { r: 16, g: 32, b: 48 }, hex: '#102030', mode: 'STATIC' }
        };

        transport.writeLine = async (line) => {
          const trimmed = String(line ?? '').trim();
          window.__nativeWrites.push(trimmed);
          if (trimmed === 'HELLO') {
            pushLine(JSON.stringify({ hello: 'mn42' }));
            return;
          }
          if (trimmed === 'GET_MANIFEST') {
            pushLine(JSON.stringify(manifest));
            return;
          }
          if (trimmed === 'GET_SCHEMA') {
            pushLine(JSON.stringify(schema));
            return;
          }
          if (trimmed === 'GET_CONFIG') {
            pushLine(JSON.stringify(config));
            return;
          }
          if (trimmed.startsWith('SET_ALL ')) {
            setAllBuffer += trimmed.slice(8);
            try {
              const payload = JSON.parse(setAllBuffer);
              window.__nativeSetAllPayload = payload;
              setAllBuffer = '';
              pushLine(
                JSON.stringify({ type: 'ack', seq: payload.seq, checksum: 'wrong-checksum' })
              );
            } catch (_) {
              // wait for more chunks
            }
            return;
          }
          throw new Error(`Unexpected native line: ${trimmed}`);
        };

        transport.nextLine = async () => {
          if (lineQueue.length) {
            return lineQueue.shift();
          }
          return new Promise((resolve) => {
            resolver = resolve;
          });
        };
      }
    };
  });

  await page.goto('/benzknobz.html');
  await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Simulator');
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  await page.evaluate(() => {
    window.__MN42_RUNTIME.stage((draft) => {
      draft.filter = { ...(draft.filter ?? {}), freq: 321 };
      return draft;
    });
  });

  await expect(page.locator('#dirty-badge')).toBeVisible();
  await page.getByRole('button', { name: 'Apply' }).click();
  await expect(page.locator('#status-label')).toHaveText('Apply failed');
  await expect(page.locator('#status .status-message')).toContainText('ACK');
  await expect(page.locator('#dirty-badge')).toBeHidden();

  const state = await page.evaluate(() => ({
    dirty: window.__MN42_RUNTIME.getState().dirty,
    diff: window.__MN42_RUNTIME.diff(),
    live: window.__MN42_RUNTIME.getState().live,
    staged: window.__MN42_RUNTIME.getState().staged
  }));
  expect(state.dirty).toBe(false);
  expect(state.diff).toEqual([]);
  expect(state.live).toEqual(state.staged);
});
