import { test, expect } from '@playwright/test';

test.describe('Stage mode', () => {
  test('renders without editor and lab panels', async ({ page }) => {
    await page.goto('/?mode=stage');

    await expect(page.locator('#performer-panel [data-ui-mode-btn="stage"]')).toHaveAttribute(
      'aria-pressed',
      'true'
    );
    await expect(page.locator('#performer-panel')).toBeVisible();
    await expect(page.locator('#performer-panel [data-ui-mode-btn="basic"]')).toBeVisible();
    await expect(page.locator('#performer-panel [data-ui-mode-btn="advanced"]')).toBeVisible();
    await expect(page.locator('#editor-panel')).toBeHidden();
    await expect(page.locator('#power-safety-pill')).toBeHidden();
    await expect(page.locator('#filter-settings')).toBeHidden();
    await expect(page.locator('#arg-settings')).toBeHidden();
    await expect(page.locator('#led-settings')).toBeHidden();
    await expect(page.locator('#device-monitor-section')).toBeHidden();
    await expect(page.locator('#midi-panel')).toBeHidden();
    await expect(page.locator('#scope-panel')).toBeHidden();
    await expect(page.locator('.debug-log-bridge')).toBeHidden();
    await expect(page.locator('#simulator-toggle')).toBeHidden();
    await expect(page.locator('#import-preset')).toBeHidden();
    await expect(page.locator('#export-preset')).toBeHidden();
    await expect(page.locator('#preset-picker')).toBeHidden();
    await expect(page.locator('#apply-save-profile')).toBeHidden();
    await expect(page.locator('#status')).toBeVisible();
  });

  test('shows simulator manifest power fields in the performer panel', async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    });
    await page.goto('/?mode=stage');

    await page.locator('#stage-connect').click();

    await expect(page.locator('#connection-pill')).toHaveText('Connected');
    await expect(page.locator('#stage-device-name')).toHaveText('MOARkNOBS-42');
    await expect(page.locator('#stage-fw-version')).toHaveText('sim-fw');
    await expect(page.locator('#stage-power-summary')).toContainText('POWER_CHOKED_V1');
    await expect(page.locator('#stage-power-summary')).toContainText('LED cap: 26/255');
    await expect(page.locator('#stage-power-summary')).toContainText('Rail: UNVERIFIED');
    await expect(page.locator('#stage-slots .stage-slot-cell')).toHaveCount(42);
    await expect(page.locator('#stage-envelopes .meter')).toHaveCount(6);
  });

  test('shows release-boundary mismatch warning without dirtying config hydration', async ({
    page
  }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
      window.__MN42_TEST_HOOKS = {
        mutateTransport(transport) {
          transport.protocol = 'native';
          const manifest = {
            device_name: 'MOARkNOBS-42',
            fw_version: 'sim-fw',
            git_sha: 'mismatch01',
            build_time: '2026-05-30T12:00:00Z',
            schema_version: 6,
            slot_count: 42,
            pot_count: 42,
            envelope_count: 6,
            arg_method_count: 14,
            led_count: 52,
            power_profile: 'SPLIT_RAIL_REWORK',
            led_brightness_cap: 255,
            rail_topology_verified: true,
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
          const config = {
            schema_version: 6,
            pots: Array.from({ length: 42 }, (_, idx) => ({ index: idx, channel: 1, cc: idx })),
            slots: Array.from({ length: 42 }, (_, idx) => ({
              index: idx,
              type: 'CC',
              type_name: 'CC',
              channel: 1,
              data1: idx,
              active: true,
              ef_index: idx % 6,
              arg: { enabled: false, method: 0, sourceA: 0, sourceB: 1 }
            })),
            efSlots: Array.from({ length: 6 }, (_, idx) => ({ index: idx, slots: [idx] })),
            filter: { type: 'LOWPASS', freq: 800, q: 1, idle_floor: 24 },
            arg: { method: 'PLUS', a: 0, b: 1, enable: false },
            led: { brightness: 64, rgb: { r: 16, g: 32, b: 48 }, hex: '#102030', mode: 'STATIC' }
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
            if (trimmed === 'GET_SCHEMA') {
              pushLine(JSON.stringify(schema));
              return;
            }
            if (trimmed === 'GET_CONFIG') {
              pushLine(JSON.stringify(config));
            }
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
    await page.goto('/?mode=stage');

    await page.locator('#stage-connect').click();

    await expect(page.locator('#connection-pill')).toHaveText('Connected');
    await expect(page.locator('#stage-dirty-state')).toHaveText('Clean');
    await expect(page.locator('#dirty-badge')).toBeHidden();
    await expect(page.locator('#stage-slots .stage-slot-cell')).toHaveCount(42);
    await expect(page.locator('#stage-power-summary')).toContainText('Release boundary mismatch');
    await expect(page.locator('#stage-power-summary')).toContainText('SPLIT_RAIL_REWORK');
    await expect(page.locator('#stage-power-summary')).toContainText('26/255');

    await page.locator('#performer-panel [data-ui-mode-btn="advanced"]').click();
    await expect(page.locator('#performer-panel')).toBeVisible();
    await expect(page.locator('#stage-power-summary')).toContainText('Release boundary mismatch');
  });

  test('switching from Stage to Advanced restores bench tools', async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    });
    await page.goto('/?mode=stage');
    await page.locator('#stage-connect').click();
    await expect(page.locator('#connection-pill')).toHaveText('Connected');

    await page.locator('#performer-panel [data-ui-mode-btn="advanced"]').click();

    await expect(page.locator('#performer-panel')).toBeVisible();
    await expect(page.locator('#live-panel')).toBeVisible();
    await expect(page.locator('#editor-panel')).toBeVisible();
    await expect(page.locator('#filter-settings')).toBeVisible();
    await expect(page.locator('#arg-settings')).toBeVisible();
    await expect(page.locator('#led-settings')).toBeVisible();
    await expect(page.locator('#device-monitor-section')).toBeVisible();
    await expect(page.locator('#simulator-toggle')).toBeVisible();

    const liveBox = await page.locator('#live-panel').boundingBox();
    const editorBox = await page.locator('#editor-panel').boundingBox();
    const utilityBox = await page.locator('#connect-card').boundingBox();
    if (!liveBox || !editorBox || !utilityBox) {
      throw new Error('Advanced workbench columns did not render');
    }
    expect(liveBox.x).toBeLessThan(editorBox.x);
    expect(editorBox.x).toBeLessThan(utilityBox.x);
    expect(utilityBox.width).toBeGreaterThan(300);
  });

  test('device monitor shows EEPROM health from the manifest', async ({ page }) => {
    await page.addInitScript(() => {
      window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    });
    await page.goto('/benzknobz.html');
    await page.getByRole('button', { name: /simulator/i }).click();
    await page.getByRole('button', { name: 'Connect' }).click();

    await expect(page.locator('#device-monitor')).toContainText('Brownouts');
    await expect(page.locator('#device-monitor')).toContainText('2');
    await expect(page.locator('#device-monitor')).toContainText('EEPROM primary');
    await expect(page.locator('#device-monitor')).toContainText('EEPROM backup');
    await expect(page.locator('#device-monitor')).toContainText('EEPROM load');
    await expect(page.locator('#device-monitor')).toContainText('Primary');
  });

  test('switching from Stage to Basic restores the calm editor surface', async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    });
    await page.goto('/?mode=stage');
    await page.locator('#stage-connect').click();
    await expect(page.locator('#connection-pill')).toHaveText('Connected');

    await page.locator('#performer-panel [data-ui-mode-btn="basic"]').click();

    await expect(page.locator('#performer-panel')).toBeVisible();
    await expect(page.locator('#stage-panel')).toBeVisible();
    await expect(page.locator('#editor-panel')).toBeVisible();
    await expect(page.locator('#filter-settings')).toBeHidden();
    await expect(page.locator('#arg-settings')).toBeHidden();
    await expect(page.locator('#scope-panel')).toBeHidden();
  });

  test('dirty staged state does not expose editing controls in Stage', async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    });
    await page.goto('/?mode=stage');
    await page.locator('#stage-connect').click();
    await expect(page.locator('#connection-pill')).toHaveText('Connected');

    await page.evaluate(() => {
      window.__MN42_RUNTIME.stage((draft) => {
        draft.slots[0].data1 = 77;
        return draft;
      });
    });
    await page.waitForFunction(() => window.__MN42_RUNTIME?.getState?.().dirty === true);

    await expect(page.locator('#stage-dirty-state')).toHaveText('Dirty');
    await expect(page.locator('#dirty-badge')).toBeVisible();
    await expect(page.locator('#apply')).toBeHidden();
    await expect(page.locator('#rollback')).toBeHidden();
    await expect(page.locator('#editor-panel')).toBeHidden();
    await expect(page.locator('.slot-editor')).toBeHidden();
    await expect(page.locator('#led-settings')).toBeHidden();
    await expect(page.locator('#export-preset')).toBeHidden();
  });

  test('panic help opens the recovery dialog without writing to the device', async ({ page }) => {
    await page.addInitScript(() => {
      window.__nativeWrites = [];
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
      window.__MN42_TEST_HOOKS = {
        mutateTransport(transport) {
          const originalWriteLine = transport.writeLine.bind(transport);
          transport.writeLine = async (line) => {
            window.__nativeWrites.push(String(line ?? '').trim());
            return originalWriteLine(line);
          };
        }
      };
    });
    await page.goto('/?mode=stage');
    await page.locator('#stage-connect').click();
    await expect(page.locator('#connection-pill')).toHaveText('Connected');
    const writesBefore = await page.evaluate(async () => {
      const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
      let last = window.__nativeWrites.length;
      let stableReads = 0;
      for (let attempt = 0; attempt < 12; attempt += 1) {
        await sleep(100);
        const next = window.__nativeWrites.length;
        if (next === last) {
          stableReads += 1;
          if (stableReads >= 2) return next;
        } else {
          stableReads = 0;
          last = next;
        }
      }
      return window.__nativeWrites.length;
    });

    await page.locator('#stage-panic-help').click();

    await expect(page.locator('#status-label')).toHaveText('Panic & recovery');
    await expect(page.locator('#panic-help-dialog')).toHaveAttribute('open', '');
    await expect(page.locator('#panic-help-dialog')).toContainText('Ctrl0 + Ctrl1 + Ctrl2');
    await expect(page.locator('#panic-help-dialog')).toContainText(
      'pio run -d firmware -t upload -e teensy40_main'
    );
    await page.waitForTimeout(300);
    await expect.poll(() => page.evaluate(() => window.__nativeWrites.length)).toBe(writesBefore);
  });
});
