import { test, expect } from '@playwright/test';

function installNativeArpHarness() {
  window.localStorage?.clear?.();
  window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
  window.__nativeWrites = [];
  window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  window.__MN42_TEST_HOOKS = {
    mutateTransport(transport) {
      transport.protocol = 'native';
      const lineQueue = [];
      let resolver = null;
      let liveArp = {
        type: 'response',
        command: 'GET_ARP',
        active: false,
        slot: 0,
        length_ticks: 12,
        shape: 0,
        shape_name: 'up',
        swing_percent: 0,
        gate_percent: 50,
        octave_range: 0,
        pattern_length: 4
      };
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
          arp_live: true
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
        if (trimmed.startsWith('GET_PROFILE,0')) {
          pushLine(
            JSON.stringify({
              profile: 0,
              stored: true,
              arp: {
                length_ticks: 12,
                shape: 0,
                swing_percent: 0,
                gate_percent: 50,
                octave_range: 0
              },
              lfos: [],
              routes: []
            })
          );
          return;
        }
        if (trimmed === 'GET_ARP') {
          pushLine(JSON.stringify(liveArp));
          return;
        }
        if (trimmed === 'SET_ARP,6,4,30,75,2') {
          liveArp = {
            ...liveArp,
            type: 'response',
            command: 'SET_ARP',
            status: 'ok',
            length_ticks: 6,
            shape: 4,
            shape_name: 'drunk',
            swing_percent: 30,
            gate_percent: 75,
            octave_range: 2
          };
          pushLine(JSON.stringify(liveArp));
          return;
        }
        if (trimmed === 'ARP_START,7') {
          liveArp = { ...liveArp, active: true, slot: 7 };
          pushLine(JSON.stringify({ arp_started: true, slot: 7, active: true }));
          return;
        }
        if (trimmed === 'ARP_STOP') {
          liveArp = { ...liveArp, active: false };
          pushLine(JSON.stringify({ arp_stopped: true, active: false }));
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

test('live arp controls push runtime shape and start stop state without dirtying config', async ({
  page
}) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
  });

  await page.goto('/benzknobz.html');
  await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Simulator');
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  await page.locator('[data-utility-tab="arp"]').click();
  await expect(page.locator('[data-utility-panel="arp"]')).toBeVisible();
  await expect(page.locator('#live-arp-apply')).toBeEnabled();

  await page.locator('#live-arp-slot').fill('7');
  await page.locator('#live-arp-length').fill('6');
  await page.locator('#live-arp-shape').selectOption('4');
  await page.locator('#live-arp-swing').fill('30');
  await page.locator('#live-arp-gate').fill('75');
  await page.locator('#live-arp-octave').fill('2');

  await page.locator('#live-arp-apply').click();
  await expect(page.locator('#status-label')).toHaveText('Live arp updated');
  await expect(page.locator('#live-arp-status')).toContainText('Drunk');
  await expect(page.locator('#live-arp-status')).toContainText('6 ticks');

  await page.locator('#live-arp-start').click();
  await expect(page.locator('#live-arp-status')).toContainText('Slot 7');
  await expect(page.locator('#live-arp-stop')).toBeEnabled();

  await page.locator('#live-arp-stop').click();
  await expect(page.locator('#live-arp-status')).toContainText('Idle');
  await expect(page.locator('#dirty-badge')).toBeHidden();
  await expect(page.getByRole('button', { name: 'Apply' })).toBeDisabled();
});

test('live arp controls use native runtime commands without config boot', async ({ page }) => {
  await page.addInitScript(installNativeArpHarness);

  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: /simulator/i }).click();
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  await page.locator('[data-utility-tab="arp"]').click();
  await expect(page.locator('#live-arp-apply')).toBeEnabled();

  await page.locator('#live-arp-slot').fill('7');
  await page.locator('#live-arp-length').fill('6');
  await page.locator('#live-arp-shape').selectOption('4');
  await page.locator('#live-arp-swing').fill('30');
  await page.locator('#live-arp-gate').fill('75');
  await page.locator('#live-arp-octave').fill('2');
  await page.locator('#live-arp-apply').click();
  await expect(page.locator('#status-label')).toHaveText('Live arp updated');

  await page.locator('#live-arp-start').click();
  await expect(page.locator('#live-arp-status')).toContainText('Slot 7');
  await page.locator('#live-arp-stop').click();
  await expect(page.locator('#live-arp-status')).toContainText('Idle');

  const writes = await page.evaluate(() => window.__nativeWrites);
  expect(writes).toContain('GET_ARP');
  expect(writes).toContain('SET_ARP,6,4,30,75,2');
  expect(writes).toContain('ARP_START,7');
  expect(writes).toContain('ARP_STOP');
  expect(writes).not.toContain('ENTER_CONFIG_MODE');
});
