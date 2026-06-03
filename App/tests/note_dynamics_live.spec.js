import { test, expect } from '@playwright/test';

function installNativeHarness() {
  window.localStorage?.clear?.();
  window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
  window.__nativeWrites = [];
  window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  window.__MN42_TEST_HOOKS = {
    mutateTransport(transport) {
      transport.protocol = 'native';
      const lineQueue = [];
      let resolver = null;
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
        build_time: '2026-06-03T12:00:00Z',
        schema_version: 6,
        slot_count: 42,
        pot_count: 42,
        envelope_count: 6,
        arg_method_count: 14,
        led_count: 12,
        capabilities: {
          note_dynamics_live: true
        }
      };
      const schema = {
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
      const config = {
        schema_version: 6,
        pots: Array.from({ length: 42 }, (_, index) => ({
          index,
          channel: (index % 16) + 1,
          cc: index % 128
        })),
        slots: Array.from({ length: 42 }, (_, index) => ({
          index,
          type_name: 'CC',
          channel: (index % 16) + 1,
          data1: index % 128,
          active: true,
          ef_index: index % 6
        })),
        efSlots: Array.from({ length: 6 }, (_, index) => ({ index, slots: [index] })),
        filter: { type: 'LINEAR', freq: 1000, q: 1 },
        arg: { enabled: false, method: 0, a: 0, b: 1 },
        led: { brightness: 64, rgb: { r: 16, g: 32, b: 48 }, mode: 'STATIC' }
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
        if (trimmed === 'SET_NOTE_DYNAMICS,-12,83') {
          pushLine(
            JSON.stringify({
              type: 'response',
              status: 'ok',
              command: 'SET_NOTE_DYNAMICS',
              velocity_shift: -12,
              change_probability: 83
            })
          );
          return;
        }
        throw new Error(`Unexpected native line: ${trimmed}`);
      };

      transport.nextLine = async () => {
        if (lineQueue.length) return lineQueue.shift();
        return new Promise((resolve) => {
          resolver = resolve;
        });
      };
    }
  };
}

test('push note dynamics sends only the live native note dynamics command', async ({ page }) => {
  await page.addInitScript(installNativeHarness);

  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: /simulator/i }).click();
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  await expect(page.locator('#note-dynamics-apply')).toBeEnabled();
  await page.locator('#note-dynamics-velocity').fill('-12');
  await page.locator('#note-dynamics-probability').fill('83');
  await page.locator('#note-dynamics-apply').click();

  await expect(page.locator('#status-label')).toHaveText('Note dynamics updated');
  const writes = await page.evaluate(() => window.__nativeWrites);
  expect(writes).toContain('SET_NOTE_DYNAMICS,-12,83');
  expect(writes).not.toContain('ENTER_CONFIG_MODE');
  expect(writes).not.toContain('RESET_PROFILE,0');
});
