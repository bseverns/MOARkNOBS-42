import { test, expect } from '@playwright/test';
import { MN42_SCHEMA_VERSION } from '../manifest_contract.js';

test.describe('Stage mode', () => {
  test('renders without editor and lab panels', async ({ page }) => {
    await page.goto('/?mode=stage');

    await expect(page.locator('.global-mode-switch [data-ui-mode-btn="stage"]')).toHaveAttribute(
      'aria-pressed',
      'true'
    );
    await expect(page.locator('#performer-panel')).toBeVisible();
    await expect(page.getByRole('button', { name: 'Configure', exact: true })).toBeVisible();
    await expect(page.getByRole('button', { name: 'Lab', exact: true })).toBeVisible();
    await expect(page.locator('#performer-panel')).toContainText(
      'Performance-safe status and recovery. No staged editors or Apply controls here.'
    );
    await expect(page.locator('#stage-profile-summary')).toBeVisible();
    await expect(page.locator('#stage-profile-summary')).toHaveText('Unavailable');
    await expect(page.locator('#stage-scene-summary')).toHaveText('No browser recall');
    await expect(page.locator('#stage-telemetry-state')).toHaveText('Telemetry offline');
    await expect(page.locator('#stage-profile-load')).toBeVisible();
    await expect(page.locator('#stage-scene-recall')).toBeVisible();
    await expect(page.locator('#stage-panic-help')).toBeVisible();
    await expect(page.locator('.connect-actions')).toBeHidden();
    await expect(page.locator('#connect-card')).toBeHidden();
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
    await expect(page.locator('#status')).toBeHidden();
    await expect(page.locator('#stage-power-summary')).toContainText('Power status unavailable');
    await expect(page.locator('#stage-power-summary .power-safety-warning')).toHaveCount(0);
    await expect(page.locator('#global-power-warning')).toBeHidden();
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
    await expect(page.locator('#stage-profile-summary')).toHaveText('Profile A');
    await expect(page.locator('#stage-telemetry-state')).toHaveText('Telemetry live');
    await expect(page.locator('#performer-panel')).toHaveAttribute(
      'data-telemetry-freshness',
      'live'
    );
    await expect(page.locator('#stage-power-summary')).toContainText('POWER_CHOKED_V1');
    await expect(page.locator('#stage-power-summary')).toContainText('LED cap: 26/255');
    await expect(page.locator('#stage-power-summary')).toContainText('Rail: UNVERIFIED');
    await expect(page.locator('#global-power-warning')).toContainText(
      'Power-limited hardware reported'
    );
    await expect(page.locator('.stage-device-details')).not.toHaveAttribute('open', '');
    await expect(page.locator('#stage-power-summary')).toBeHidden();
    await page.locator('.stage-device-details > summary').click();
    await expect(page.locator('#stage-power-summary')).toBeVisible();
    await expect(page.locator('#stage-slots .stage-slot-cell')).toHaveCount(42);
    await expect(page.locator('#stage-envelopes .meter')).toHaveCount(6);
    await expect(page.locator('#stage-slot-focus')).toHaveText(
      /Slot 1 · CC\d+ · Ch 1 · BASE \d+ · ARG→EF [+-]\d+ · OUT \d+/
    );
    await expect(page.locator('#stage-clock-state')).toHaveText(/^(EXT|INT) · \d+\.\d BPM · Running$/);
    await expect
      .poll(() => page.locator('#stage-envelopes .meter[data-state="active"]').count())
      .toBeGreaterThan(0);
    await expect(page.locator('#stage-envelopes .meter').first()).toContainText('ACTIVE');
    await expect(page.locator('#stage-envelopes .meter').first()).toContainText('No routes');
    await page.locator('#stage-slots .stage-slot-cell').nth(16).click();
    await expect(page.locator('#stage-slot-focus')).toContainText('Slot 17');
  });

  test('Motion drawer is read-only, records while closed, and exposes per-source traces', async ({
    page
  }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    });
    await page.goto('/?mode=stage');
    await page.locator('#stage-connect').click();
    await expect(page.locator('#connection-pill')).toHaveText('Connected');

    const drawer = page.locator('#stage-motion');
    const panel = page.locator('#stage-motion-panel');
    await expect(drawer).toBeVisible();
    await expect(drawer).not.toHaveAttribute('open', '');
    await expect(panel).toBeHidden();
    await expect(panel).toHaveAttribute('data-scope-rendering', 'false');
    await expect(panel.locator('[data-ef-index]')).toHaveCount(6);

    await drawer.locator(':scope > summary').click();
    await expect(panel).toBeVisible();
    await expect(panel).toHaveAttribute('data-scope-rendering', 'true');
    await expect(panel.locator('[data-scope-role="status"]')).toHaveText(/Telemetry/i);
    await expect(panel.locator('[data-scope-lfo-index="0"]')).not.toHaveText('--');
    await expect(panel.locator('[data-scope-lfo-index="1"]')).not.toHaveText('--');
    await expect.poll(() => panel.locator('[data-state="active"]').count()).toBeGreaterThan(0);
    await expect(panel.locator('[data-scope-role="snapshot"]')).toHaveCount(0);
    await expect(panel.locator('[data-scope-role="refresh"]')).toHaveCount(0);
    await expect(panel.locator('[data-scope-role="fps"]')).toHaveCount(0);
    await expect(drawer.locator('[data-scope-summary]')).toHaveText(
      /(\d+ EFs? active|\d+ active \+ \d+ recent) · 2 LFOs/
    );
    await expect(panel.locator('[data-scope-role="view-state"]')).toHaveText(
      /VIEW: ACTIVE · \d+\/6 EFs(?: \([^)]*\))? · 2 LFOs always visible/
    );
    await expect(panel.locator('[data-scope-window="5"]')).toHaveAttribute(
      'aria-pressed',
      'true'
    );
    await panel.locator('[data-scope-window="2"]').click();
    await expect(panel.locator('[data-scope-window="2"]')).toHaveAttribute(
      'aria-pressed',
      'true'
    );
    await expect(panel.locator('canvas')).toHaveAttribute('aria-label', /over 2 seconds/);

    await panel.getByRole('button', { name: 'All', exact: true }).click();
    await expect(panel.getByRole('button', { name: 'All', exact: true })).toHaveAttribute(
      'aria-pressed',
      'true'
    );
    await expect(drawer.locator('[data-scope-summary]')).toHaveText('All 6 EFs · 2 LFOs');
    await drawer.locator(':scope > summary').click();
    await expect(panel).toHaveAttribute('data-scope-rendering', 'false');

    await drawer.locator(':scope > summary').click();
    await expect(panel).toHaveAttribute('data-scope-rendering', 'true');
    await page.getByRole('button', { name: 'Configure', exact: true }).click();
    await expect(drawer).not.toHaveAttribute('open', '');
    await expect(panel).toHaveAttribute('data-scope-rendering', 'false');
  });

  test('shows split-rail metadata without a power warning or dirtying config hydration', async ({
    page
  }) => {
    await page.addInitScript(({ schemaVersion }) => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
      window.__MN42_TEST_HOOKS = {
        mutateTransport(transport) {
          transport.protocol = 'native';
          const manifest = {
            device_name: 'MOARkNOBS-42',
            fw_version: 'sim-fw',
            git_sha: 'mismatch01',
            build_time: '2026-05-30T12:00:00Z',
            schema_version: schemaVersion,
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
            schema_version: schemaVersion,
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
            schema_version: schemaVersion,
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
    }, { schemaVersion: MN42_SCHEMA_VERSION });
    await page.goto('/?mode=stage');

    await page.locator('#stage-connect').click();

    await expect(page.locator('#connection-pill')).toHaveText('Connected');
    await expect(page.locator('#stage-dirty-state')).toHaveText('Clean');
    await expect(page.locator('#dirty-badge')).toBeHidden();
    await expect(page.locator('#stage-slots .stage-slot-cell')).toHaveCount(42);
    await expect(page.locator('.stage-device-details')).not.toHaveAttribute('open', '');
    await expect(page.locator('#stage-power-summary')).toBeHidden();
    await page.locator('.stage-device-details > summary').click();
    await expect(page.locator('#stage-power-summary')).toBeVisible();
    await expect(page.locator('#stage-power-summary')).toContainText('SPLIT_RAIL_REWORK');
    await expect(page.locator('#stage-power-summary')).toContainText('255/255');
    await expect(page.locator('#stage-power-summary .power-safety-warning')).toHaveCount(0);
    await expect(page.locator('#global-power-warning')).toBeHidden();

    await page.getByRole('button', { name: 'Lab', exact: true }).click();
    await expect(page.locator('#performer-panel')).toBeHidden();
    await expect(page.locator('#stage-power-summary .power-safety-warning')).toHaveCount(0);
  });

  test('switching from Stage to Lab restores bench tools', async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    });
    await page.goto('/?mode=stage');
    await page.locator('#stage-connect').click();
    await expect(page.locator('#connection-pill')).toHaveText('Connected');

    await page.getByRole('button', { name: 'Lab', exact: true }).click();

    await expect(page.locator('#performer-panel')).toBeHidden();
    await expect(page.locator('#live-panel')).toBeVisible();
    await expect(page.locator('#editor-panel')).toBeVisible();
    await expect(page.locator('#filter-settings')).toBeVisible();
    await expect(page.locator('#arg-settings')).toBeVisible();
    await expect(page.locator('#led-settings')).toBeVisible();
    await expect(page.locator('#device-monitor-section')).toBeVisible();
    await expect(page.locator('#slot-detail-panel')).toBeVisible();
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
    await page.locator('#simulator-toggle').click();
    await page.getByRole('button', { name: 'Connect' }).click();

    await expect(page.locator('#device-monitor')).toContainText('OLED present');
    await expect(page.locator('#device-monitor')).toContainText('OLED ready');
    await expect(page.locator('#device-monitor')).toContainText('OLED status');
    await expect(page.locator('#device-monitor')).toContainText('ok');
    await expect(page.locator('#device-monitor')).toContainText('Brownouts');
    await expect(page.locator('#device-monitor')).toContainText('2');
    await expect(page.locator('#device-monitor')).toContainText('EEPROM primary');
    await expect(page.locator('#device-monitor')).toContainText('EEPROM backup');
    await expect(page.locator('#device-monitor')).toContainText('EEPROM load');
    await expect(page.locator('#device-monitor')).toContainText('Primary');
  });

  test('switching from Stage to Configure restores the calm editor surface', async ({ page }) => {
    await page.addInitScript(() => {
      window.__MN42_RUNTIME_OPTIONS = { useSimulator: true };
    });
    await page.goto('/?mode=stage');
    await page.locator('#stage-connect').click();
    await expect(page.locator('#connection-pill')).toHaveText('Connected');

    await page.getByRole('button', { name: 'Configure', exact: true }).click();

    await expect(page.locator('#performer-panel')).toBeHidden();
    await expect(page.locator('#stage-panel')).toBeVisible();
    await expect(page.locator('#editor-panel')).toBeVisible();
    await expect(page.locator('#connect-card')).toBeVisible();
    await expect(page.locator('#check-compatibility')).toBeHidden();
    await expect(page.locator('#config-mode')).toBeHidden();
    await expect(page.locator('#filter-settings')).toBeHidden();
    await expect(page.locator('#arg-settings')).toBeHidden();
    await expect(page.locator('#scope-panel')).toBeHidden();
    await expect(page.locator('#stage-power-summary')).toBeHidden();
    await expect(page.locator('#stage-scene-recall')).toBeHidden();
    await expect(page.locator('#stage-panic-help')).toBeHidden();
    await expect(page.locator('#usb-midi-toggle')).toBeHidden();
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
    await expect(page.locator('#stage-profile-load')).toBeDisabled();
    await expect(page.locator('#stage-scene-recall')).toBeDisabled();
    await expect(page.locator('#stage-draft-blocked')).toBeVisible();
    await expect(page.locator('#stage-draft-blocked')).toContainText(
      'resolve it in Configure with Apply or Discard'
    );
    await expect(page.locator('#dirty-badge')).toBeVisible();
    await expect(page.locator('#apply')).toBeHidden();
    await expect(page.locator('#rollback')).toHaveCount(0);
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
