import { test, expect } from '@playwright/test';

async function openRecoveryDrawer(page) {
  await page.locator('#recovery-drawer').evaluate((element) => {
    element.open = true;
  });
}

test('native transport adapter speaks HELLO/GET_*/SET_ALL instead of JSON-RPC', async ({ page }) => {
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
          free_ram: 48000,
          free_flash: 512000,
          capabilities: {
            profile_save: false,
            profile_load: false,
            profile_reset: false,
            macro_snapshot: false,
            scenes: false
          }
        };
        let config = {
          fw_version: manifest.fw_version,
          schema_version: manifest.schema_version,
          pots: Array.from({ length: manifest.pot_count }, (_, idx) => ({
            index: idx,
            channel: (idx % 16) + 1,
            cc: idx % 128
          })),
          slots: Array.from({ length: manifest.slot_count }, (_, idx) => ({
            index: idx,
            type: 1,
            type_name: 'CC',
            channel: (idx % 16) + 1,
            data1: idx % 128,
            ef_index: idx % manifest.envelope_count,
            ef: {
              index: idx % manifest.envelope_count,
              filter_index: 0,
              filter_name: 'LINEAR',
              frequency: 1000,
              q: 0.707,
              oversample: 4,
              smoothing: 0.2,
              baseline: 0,
              gain: 1,
              mode: 0,
              auto_baseline: false,
              auto_gain: false,
              attack_ms: 10,
              release_ms: 100,
              rms_ms: 20,
              baseline_tau_ms: 400,
              gain_tau_ms: 600,
              gate_threshold: 3,
              gate_hysteresis: 1,
              activity_threshold: 2,
              gain_target: 64
            },
            active: true,
            arp_note: 0,
            sysexTemplate: '',
            ef_payload: {
              type: 0,
              type_name: 'LINEAR',
              freq: 1000,
              q: 0.707
            },
            arg: {
              enabled: false,
              method: 0,
              method_name: 'PLUS',
              sourceA: 0,
              sourceB: 1
            }
          })),
          efSlots: Array.from({ length: manifest.envelope_count }, (_, idx) => ({
            index: idx,
            slots: [idx]
          })),
          envelopes: {
            routing: Array.from({ length: manifest.pot_count }, (_, idx) => idx % manifest.envelope_count),
            followers: Array.from({ length: manifest.envelope_count }, (_, idx) => ({
              index: idx,
              active: true,
              filter: 'LINEAR',
              baseline: 0,
              oversample: 4,
              smoothing: 0.2
            })),
            mode: 0,
            mode_name: 'SEF',
            arg_method: 0,
            arg_method_name: 'PLUS',
            arg_enable: true,
            arg_pair: { a: 0, b: 1 },
            filter: { frequency: 800, q: 1 }
          },
          led: {
            brightness: 64,
            rgb: { r: 17, g: 34, b: 51 },
            hex: '#112233',
            mode: 'STATIC'
          }
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
          if (trimmed === 'GET_CONFIG') {
            pushLine(JSON.stringify(config));
            return;
          }
          if (trimmed.startsWith('SET_ALL ')) {
            setAllBuffer += trimmed.slice(8);
            try {
              const payload = JSON.parse(setAllBuffer);
              window.__nativeSetAllPayload = payload;
              config = payload.config;
              setAllBuffer = '';
              pushLine(JSON.stringify({ type: 'ack', seq: payload.seq, checksum: payload.checksum }));
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
  await page.getByRole('button', { name: /simulator/i }).click();
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');

  const manifest = await page.evaluate(() => window.__MN42_RUNTIME.getState().manifest);
  expect(manifest.device_name).toBe('MOARkNOBS-42');
  expect(manifest.fw_version).toBe('native-fw');

  const freqInput = page.locator('[data-schema-target="filter"] input[type="number"]').first();
  await freqInput.waitFor({ state: 'visible' });
  await freqInput.fill('321');
  await freqInput.dispatchEvent('change');
  const applyResult = await page.evaluate(async () => {
    try {
      return { ok: true, result: await window.__MN42_RUNTIME.apply() };
    } catch (err) {
      return {
        ok: false,
        message: err?.message ?? String(err),
        validation: err?.validation ?? null,
        state: window.__MN42_RUNTIME.getState()
      };
    }
  });
  expect(applyResult).toEqual(expect.objectContaining({ ok: true }));

  const result = await page.evaluate(() => ({
    writes: window.__nativeWrites,
    payload: window.__nativeSetAllPayload
  }));
  expect(result.writes).toContain('HELLO');
  expect(result.writes).toContain('GET_MANIFEST');
  expect(result.writes).toContain('GET_CONFIG');
  expect(result.writes.some((line) => line.startsWith('SET_ALL '))).toBe(true);
  expect(result.payload?.config?.filter?.freq).toBe(321);
  expect(typeof result.payload?.checksum).toBe('string');
});

test('native transport disables unsupported profile and recovery controls', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    window.__MN42_TEST_HOOKS = {
      mutateTransport(transport) {
        transport.protocol = 'native';
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
          free_ram: 48000,
          free_flash: 512000,
          capabilities: {
            profile_save: false,
            profile_load: false,
            profile_reset: false,
            macro_snapshot: false,
            scenes: false
          }
        };
        const config = {
          schema_version: 6,
          pots: Array.from({ length: 42 }, (_, idx) => ({ index: idx, channel: 1, cc: idx })),
          slots: Array.from({ length: 42 }, (_, idx) => ({
            index: idx,
            type_name: 'CC',
            channel: 1,
            data1: idx,
            ef_index: -1,
            ef: { index: -1, filter_name: 'LINEAR', filter_index: 0, frequency: 1000, q: 0.707 }
          })),
          efSlots: Array.from({ length: 6 }, () => ({ slots: [] })),
          envelopes: {
            routing: Array.from({ length: 42 }, () => -1),
            followers: Array.from({ length: 6 }, (_, idx) => ({ index: idx }))
          },
          led: { brightness: 0, rgb: { r: 0, g: 0, b: 0 }, hex: '#000000', mode: 'STATIC' }
        };
        const queue = [];
        let resolver = null;
        const pushLine = (line) => {
          if (resolver) {
            const pending = resolver;
            resolver = null;
            pending(line);
            return;
          }
          queue.push(line);
        };
        transport.writeLine = async (line) => {
          const trimmed = String(line ?? '').trim();
          if (trimmed === 'HELLO') {
            pushLine(JSON.stringify({ hello: 'mn42' }));
            return;
          }
          if (trimmed === 'GET_MANIFEST') {
            pushLine(JSON.stringify(manifest));
            return;
          }
          if (trimmed === 'GET_CONFIG') {
            pushLine(JSON.stringify(config));
            return;
          }
          throw new Error(`Unexpected line: ${trimmed}`);
        };
        transport.nextLine = async () => {
          if (queue.length) return queue.shift();
          return new Promise((resolve) => {
            resolver = resolve;
          });
        };
      }
    };
  });

  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: /simulator/i }).click();
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');
  await openRecoveryDrawer(page);

  await expect(page.getByRole('button', { name: 'Save profile', exact: true })).toBeDisabled();
  await expect(page.getByRole('button', { name: 'Switch profile', exact: true })).toBeDisabled();
  await expect(page.getByRole('button', { name: 'Reset profile', exact: true })).toBeDisabled();
  await expect(page.getByRole('button', { name: '1. Switch target', exact: true })).toBeDisabled();
  await expect(page.getByRole('button', { name: '3. Save profile', exact: true })).toBeDisabled();
  await expect(page.getByRole('button', { name: 'Save Macro Snapshot' })).toBeDisabled();
  await expect(page.locator('.scene-save').first()).toBeDisabled();
  await expect(page.locator('#profile-hint')).toContainText(
    'does not expose browser-driven profile save, switch, or reset yet'
  );
  await expect(page.locator('#macro-status')).toContainText('unavailable on this firmware');
  await expect(page.locator('#scene-status')).toContainText('unavailable on this firmware');
});

test('native transport supports profile, macro, and scene actions when firmware advertises them', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
    window.__nativeWrites = [];
    window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    window.__MN42_TEST_HOOKS = {
      mutateTransport(transport) {
        transport.protocol = 'native';
        const queue = [];
        let resolver = null;
        const clone = (value) => JSON.parse(JSON.stringify(value));
        const pushLine = (line) => {
          if (resolver) {
            const pending = resolver;
            resolver = null;
            pending(line);
            return;
          }
          queue.push(line);
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
          free_ram: 48000,
          free_flash: 512000,
          capabilities: {
            profile_save: true,
            profile_load: true,
            profile_reset: true,
            macro_snapshot: true,
            scenes: true
          }
        };
        const baseConfig = {
          schema_version: 6,
          pots: Array.from({ length: 42 }, (_, idx) => ({ index: idx, channel: 1, cc: idx })),
          slots: Array.from({ length: 42 }, (_, idx) => ({
            index: idx,
            type: 1,
            type_name: 'CC',
            channel: 1,
            data1: idx,
            ef_index: idx % 6,
            ef: {
              index: idx % 6,
              filter_name: 'LINEAR',
              filter_index: 0,
              frequency: 1000,
              q: 0.707,
              oversample: 4,
              smoothing: 0.2,
              baseline: 0,
              gain: 1,
              mode: 0,
              auto_baseline: false,
              auto_gain: false,
              attack_ms: 10,
              release_ms: 100,
              rms_ms: 20,
              baseline_tau_ms: 400,
              gain_tau_ms: 600,
              gate_threshold: 3,
              gate_hysteresis: 1,
              activity_threshold: 2,
              gain_target: 64
            },
            active: true,
            arp_note: 0,
            sysexTemplate: '',
            ef_payload: { type: 0, type_name: 'LINEAR', freq: 1000, q: 0.707 },
            arg: {
              enabled: false,
              method: 0,
              method_name: 'PLUS',
              sourceA: 0,
              sourceB: 1
            }
          })),
          efSlots: Array.from({ length: 6 }, (_, idx) => ({ index: idx, slots: [idx] })),
          envelopes: {
            routing: Array.from({ length: 42 }, (_, idx) => idx % 6),
            followers: Array.from({ length: 6 }, (_, idx) => ({
              index: idx,
              active: true,
              filter: 'LINEAR',
              baseline: 0,
              oversample: 4,
              smoothing: 0.2
            })),
            mode: 0,
            mode_name: 'SEF',
            arg_method: 0,
            arg_method_name: 'PLUS',
            arg_enable: true,
            arg_pair: { a: 0, b: 1 },
            filter: { frequency: 800, q: 1 }
          },
          led: {
            brightness: 32,
            rgb: { r: 8, g: 16, b: 24 },
            hex: '#081018',
            mode: 'STATIC'
          }
        };

        let config = clone(baseConfig);
        let macroSnapshot = null;
        const profileSlots = Array.from({ length: 4 }, () => clone(baseConfig));
        const scenes = Array.from({ length: 6 }, () => null);

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
          if (trimmed === 'GET_CONFIG') {
            pushLine(JSON.stringify(config));
            return;
          }
          if (trimmed === 'SAVE_PROFILE,0') {
            profileSlots[0] = clone(config);
            pushLine(JSON.stringify({ profile_saved: true, profile: 0 }));
            return;
          }
          if (trimmed === 'LOAD_PROFILE,0') {
            config = clone(profileSlots[0]);
            config.led.brightness = 77;
            pushLine(JSON.stringify({ profile_loaded: true, profile: 0 }));
            return;
          }
          if (trimmed === 'RESET_PROFILE,0') {
            profileSlots[0] = clone(baseConfig);
            config = clone(baseConfig);
            pushLine(JSON.stringify({ profile_reset: true, profile: 0 }));
            return;
          }
          if (trimmed === 'SAVE_MACRO_SLOT') {
            macroSnapshot = clone(config);
            pushLine(JSON.stringify({ macro_saved: true, macro_available: true }));
            return;
          }
          if (trimmed === 'RECALL_MACRO_SLOT') {
            if (macroSnapshot) {
              config = clone(macroSnapshot);
              pushLine(JSON.stringify({ macro_recalled: true, macro_available: true }));
            } else {
              pushLine(JSON.stringify({ macro_recalled: false, macro_available: false, error: 'No macro stored' }));
            }
            return;
          }
          if (trimmed.startsWith('{')) {
            const payload = JSON.parse(trimmed);
            if (payload.cmd === 'SAVE_SCENE') {
              scenes[payload.slot] = {
                name: payload.name ?? '',
                config: clone(config)
              };
              pushLine(
                JSON.stringify({
                  cmd: 'SAVE_SCENE',
                  scene_saved: true,
                  scene_slot: payload.slot,
                  scene_name: payload.name ?? '',
                  scene_available: true
                }),
              );
              return;
            }
            if (payload.cmd === 'GET_SCENES') {
              pushLine(
                JSON.stringify({
                  cmd: 'GET_SCENES',
                  scenes: scenes.map((entry, slot) => ({
                    slot,
                    name: entry?.name ?? '',
                    available: Boolean(entry)
                  }))
                }),
              );
              return;
            }
            if (payload.cmd === 'RECALL_SCENE') {
              const entry = scenes[payload.slot];
              if (!entry) {
                pushLine(
                  JSON.stringify({
                    cmd: 'RECALL_SCENE',
                    scene_recalled: false,
                    scene_slot: payload.slot,
                    scene_name: '',
                    scene_available: false,
                    scene_error: 'No snapshot stored in this slot'
                  }),
                );
                return;
              }
              config = clone(entry.config);
              pushLine(
                JSON.stringify({
                  cmd: 'RECALL_SCENE',
                  scene_recalled: true,
                  scene_slot: payload.slot,
                  scene_name: entry.name,
                  scene_available: true
                }),
              );
              return;
            }
          }
          throw new Error(`Unexpected line: ${trimmed}`);
        };

        transport.nextLine = async () => {
          if (queue.length) return queue.shift();
          return new Promise((resolve) => {
            resolver = resolve;
          });
        };
      }
    };
  });

  await page.goto('/benzknobz.html');
  await page.getByRole('button', { name: /simulator/i }).click();
  await page.getByRole('button', { name: 'Connect' }).click();
  await expect(page.locator('#connection-pill')).toContainText('Connected');
  await openRecoveryDrawer(page);

  await expect(page.getByRole('button', { name: 'Save profile', exact: true })).toBeEnabled();
  await expect(page.getByRole('button', { name: 'Switch profile', exact: true })).toBeEnabled();
  await expect(page.getByRole('button', { name: 'Reset profile', exact: true })).toBeEnabled();
  await expect(page.getByRole('button', { name: 'Save Macro Snapshot' })).toBeEnabled();
  await expect(page.locator('.scene-save').first()).toBeEnabled();

  await page.getByRole('button', { name: 'Save profile', exact: true }).click();
  await page.getByRole('button', { name: 'Switch profile', exact: true }).click();
  await page.getByRole('button', { name: 'Reset profile', exact: true }).click();

  await page.evaluate(async () => {
    await window.__MN42_RUNTIME.sendMacroCommand('SAVE_MACRO_SLOT');
    await window.__MN42_RUNTIME.sendMacroCommand('RECALL_MACRO_SLOT');
    await window.__MN42_RUNTIME.sendSceneCommand({ cmd: 'SAVE_SCENE', slot: 1, name: 'Verse' });
    await window.__MN42_RUNTIME.requestScenes();
    await window.__MN42_RUNTIME.sendSceneCommand({ cmd: 'RECALL_SCENE', slot: 1 });
  });

  const writes = await page.evaluate(() => window.__nativeWrites);
  expect(writes).toContain('SAVE_PROFILE,0');
  expect(writes).toContain('LOAD_PROFILE,0');
  expect(writes).toContain('RESET_PROFILE,0');
  expect(writes).toContain('SAVE_MACRO_SLOT');
  expect(writes).toContain('RECALL_MACRO_SLOT');
  expect(writes).toContain('GET_CONFIG');
  expect(writes).toContain(JSON.stringify({ cmd: 'SAVE_SCENE', slot: 1, name: 'Verse' }));
  expect(writes).toContain(JSON.stringify({ cmd: 'GET_SCENES' }));
  expect(writes).toContain(JSON.stringify({ cmd: 'RECALL_SCENE', slot: 1 }));
});
