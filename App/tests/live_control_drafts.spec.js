import { test, expect } from '@playwright/test';

async function createLiveControlHarness(page) {
  return page.evaluate(async () => {
    const { createTransportToolbarController } = await import(
      '/views/controllers/transport_toolbar_controller.js'
    );
    document.body.innerHTML = `
      <span id="connection-pill" data-stage="live"></span>
      <input id="jitter-depth" value="0.20" />
      <input id="jitter-smoothness" value="0.50" />
      <button id="jitter-apply">Push live override</button>
      <span id="jitter-status"></span>
      <select id="clock-source"><option value="external">External</option><option value="internal">Internal</option></select>
      <input id="clock-bpm" value="120" />
      <select id="clock-out"><option value="off">Off</option><option value="on">On</option></select>
      <button id="clock-apply">Push live clock</button>
      <span id="clock-status"></span>
    `;
    const calls = [];
    const runtime = {
      getState: () => ({ transportMode: 'simulator', dirty: false }),
      sendRpc: async (payload) => {
        calls.push(structuredClone(payload));
        if (payload.rpc === 'set_jitter') {
          return { depth: payload.depth, smoothness: payload.smoothness };
        }
        if (payload.rpc === 'set_clock') {
          return {
            follow_external: payload.followExternal,
            clock_out_enabled: payload.clockOutEnabled,
            tapped_bpm: payload.tappedBpm,
            external_bpm: 123.4,
            external_signal: true,
            running: true,
            source: payload.followExternal ? 'external' : 'internal'
          };
        }
        return {};
      }
    };
    const controller = createTransportToolbarController({
      runtime,
      setStatus() {},
      resolveDeviceName: () => 'MOARkNOBS-42',
      resolveFirmwareVersion: () => 'test',
      elements: {
        connectionPill: document.getElementById('connection-pill'),
        jitterDepthInput: document.getElementById('jitter-depth'),
        jitterSmoothnessInput: document.getElementById('jitter-smoothness'),
        jitterApplyBtn: document.getElementById('jitter-apply'),
        jitterStatusEl: document.getElementById('jitter-status'),
        deviceClockSourceSelect: document.getElementById('clock-source'),
        deviceClockBpmInput: document.getElementById('clock-bpm'),
        deviceClockOutSelect: document.getElementById('clock-out'),
        deviceClockApplyBtn: document.getElementById('clock-apply'),
        deviceClockStatusEl: document.getElementById('clock-status')
      }
    });
    controller.bind();
    controller.onManifest({ capabilities: { jitter_live: true, clock_live: true } });
    controller.onConnected();
    controller.onTelemetry({
      jitter: { depth: 0.2, smoothness: 0.5 },
      clock: {
        follow_external: true,
        clock_out_enabled: false,
        tapped_bpm: 120,
        external_bpm: 123.4,
        external_signal: true,
        running: true,
        source: 'external'
      }
    });
    window.__liveControlHarness = { controller, calls };
    return true;
  });
}

test('jitter local draft survives telemetry until Push live override succeeds', async ({ page }) => {
  await page.goto('/views/controllers/transport_toolbar_controller.js');
  await createLiveControlHarness(page);

  await page.locator('#jitter-depth').fill('0.80');
  await page.evaluate(() => window.__liveControlHarness.controller.onTelemetry({
    jitter: { depth: 0.2, smoothness: 0.5 }
  }));
  await page.locator('#jitter-smoothness').fill('0.75');
  await page.evaluate(() => window.__liveControlHarness.controller.onTelemetry({
    jitter: { depth: 0.2, smoothness: 0.5 }
  }));
  await page.locator('#jitter-apply').click();

  await expect.poll(() => page.evaluate(() => window.__liveControlHarness.calls.at(-1))).toEqual({
    rpc: 'set_jitter',
    depth: 0.8,
    smoothness: 0.75
  });
  await expect(page.locator('#jitter-depth')).toHaveValue('0.80');
  await expect(page.locator('#jitter-smoothness')).toHaveValue('0.75');
  await expect(page.locator('#jitter-status')).toContainText('Depth 0.80');
});

test('device clock local draft survives telemetry until live Apply succeeds', async ({ page }) => {
  await page.goto('/views/controllers/transport_toolbar_controller.js');
  await createLiveControlHarness(page);

  await page.locator('#clock-source').selectOption('internal');
  await page.evaluate(() => window.__liveControlHarness.controller.onTelemetry({
    clock: {
      follow_external: true,
      clock_out_enabled: false,
      tapped_bpm: 120,
      external_bpm: 123.4,
      source: 'external'
    }
  }));
  await page.locator('#clock-bpm').fill('146.5');
  await page.locator('#clock-out').selectOption('on');
  await page.evaluate(() => window.__liveControlHarness.controller.onTelemetry({
    clock: {
      follow_external: true,
      clock_out_enabled: false,
      tapped_bpm: 120,
      external_bpm: 123.4,
      source: 'external'
    }
  }));
  await page.locator('#clock-apply').click();

  await expect.poll(() => page.evaluate(() => window.__liveControlHarness.calls.at(-1))).toEqual({
    rpc: 'set_clock',
    followExternal: false,
    clockOutEnabled: true,
    tappedBpm: 146.5
  });
  await expect(page.locator('#clock-source')).toHaveValue('internal');
  await expect(page.locator('#clock-bpm')).toHaveValue('146.5');
  await expect(page.locator('#clock-out')).toHaveValue('on');
  await expect(page.locator('#clock-status')).toContainText('internal');
});

test('disconnect deliberately clears unsent Jitter and Clock drafts', async ({ page }) => {
  await page.goto('/views/controllers/transport_toolbar_controller.js');
  await createLiveControlHarness(page);

  await page.locator('#jitter-depth').fill('0.80');
  await page.locator('#clock-bpm').fill('146.5');
  await page.evaluate(() => window.__liveControlHarness.controller.onDisconnected());

  await expect(page.locator('#jitter-depth')).toHaveValue('0.20');
  await expect(page.locator('#jitter-smoothness')).toHaveValue('0.50');
  await expect(page.locator('#clock-source')).toHaveValue('external');
  await expect(page.locator('#clock-bpm')).toHaveValue('120.0');
  await expect(page.locator('#clock-out')).toHaveValue('off');
});
