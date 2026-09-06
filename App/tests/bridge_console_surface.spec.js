import { test, expect } from '@playwright/test';
import { readFileSync } from 'node:fs';

const uiRoot = new URL('../../bridge/ui/', import.meta.url);
const config = {
  serialName: '/dev/saved-mn42',
  midiLabel: 'Saved MIDI',
  midiDestinationName: 'Ableton',
  oscDestinationName: 'TouchDesigner',
  oscHost: '127.0.0.1',
  oscPort: 9000,
  oscListen: 9001,
  oscBind: '127.0.0.1',
  midiTelemetryMode: 'mapped',
  outboundMidiMappings: [],
  midiToOscMappings: []
};

async function consoleFixture(page, { running = false } = {}) {
  const errors = [];
  page.on('pageerror', (error) => errors.push(error.message));
  let state = {
    running,
    serialConnected: running,
    ready: running,
    config: { ...config },
    routes: [],
    deviceSession: {
      deviceAuthority: 'verified',
      configValidation: { status: 'verified' },
      firmwareIdentity: { device_name: 'MN42', fw_version: '0.test' }
    }
  };
  const posts = [];
  let serial;
  await page.routeWebSocket('**/ws', (socket) => {
    serial = socket;
  });
  await page.routeWebSocket('**/events', () => {});
  await page.route('**/bridge-console/**', async (route) => {
    const name = new URL(route.request().url()).pathname.split('/').pop() || 'index.html';
    const type = name.endsWith('.js')
      ? 'text/javascript'
      : name.endsWith('.css')
        ? 'text/css'
        : 'text/html';
    await route.fulfill({ contentType: type, body: readFileSync(new URL(name, uiRoot)) });
  });
  await page.route('**/api/**', async (route) => {
    const path = new URL(route.request().url()).pathname;
    let payload = { state };
    if (route.request().method() === 'POST') {
      posts.push({ path, body: route.request().postDataJSON() });
      if (path === '/api/connect')
        state = {
          ...state,
          running: true,
          serialConnected: true,
          ready: true,
          config: posts.at(-1).body
        };
      payload = { state };
    } else if (path === '/api/ports') payload = { ports: [{ path: '/dev/other-device' }] };
    else if (path === '/api/midi-ports')
      payload = { inputs: [{ name: 'Other MIDI' }], outputs: [{ name: 'Other MIDI' }] };
    else if (path === '/api/presets') payload = { presets: [] };
    await route.fulfill({ json: payload });
  });
  await page.goto('/bridge-console/');
  await expect(page.locator('#summary-status')).toContainText(
    running ? 'Bridge live' : 'Bridge idle'
  );
  await expect.poll(() => Boolean(serial)).toBe(true);
  return {
    posts,
    errors,
    update: (patch) => {
      state = { ...state, ...patch };
    },
    telemetry: (slots, traceId) => serial.send(JSON.stringify({ slots, traceId }))
  };
}

test('Bridge route cards preserve MIDI numbering and reject invalid raw edits', async ({
  page
}) => {
  const fixture = await consoleFixture(page);
  await page.getByRole('button', { name: 'Routing', exact: true }).click();
  const editor = page.locator('#add-outbound-route');
  await editor.locator('[name=sourceNumber]').fill('12');
  await editor.locator('[name=controller]').fill('74');
  await editor.locator('[name=channel]').fill('2');
  await editor.getByRole('button', { name: 'Add route', exact: true }).click();
  await expect(page.locator('#outbound-route-list')).toContainText('Slot 12 → CC 74 · Ch 2');
  await page.getByText('Advanced / Edit raw route definition', { exact: true }).click();
  expect(JSON.parse(await page.locator('#outbound-midi-mappings').inputValue())[0]).toMatchObject({
    sourceIndex: 11,
    channel: 2,
    controller: 74
  });
  await page
    .locator('#outbound-midi-mappings')
    .fill('[{"source":"slots","sourceIndex":11,"channel":17,"controller":74}]');
  await page.getByRole('button', { name: 'Setup', exact: true }).click();
  await page.locator('#start-bridge').click();
  await expect(page.locator('#summary-status')).toContainText('Route definition invalid');
  await page.locator('#custom-setup-name').fill('Invalid draft');
  await page.locator('#save-custom-setup').click();
  await expect(page.locator('#custom-setup-status')).toContainText('Route definition invalid');
  expect(fixture.posts).toHaveLength(0);
  await page.getByRole('button', { name: 'Routing', exact: true }).click();
  await page.locator('#outbound-midi-mappings').fill('[]');
  await editor.locator('[name=source]').selectOption('envelopes');
  await editor.locator('[name=sourceNumber]').fill('1');
  await editor.getByRole('button', { name: 'Add route', exact: true }).click();
  await expect(page.locator('#outbound-route-list')).toContainText('EF 1 → CC 74 · Ch 2');
  await page.getByRole('button', { name: 'Remove EF 1 to CC 74 channel 2' }).click();
  await expect(page.locator('#outbound-route-list')).toContainText('No configured routes');
  expect(fixture.errors).toEqual([]);
});

test('saved rig recall preserves selected ports and profile remains advisory', async ({ page }) => {
  const fixture = await consoleFixture(page);
  await page.locator('#serial-name').fill(config.serialName);
  await page.getByText('Edit routing details', { exact: true }).click();
  await page.locator('#midi-label').fill(config.midiLabel);
  await page.locator('#custom-setup-name').fill('Friday performance');
  await page.locator('#custom-setup-profile').fill('Performance A');
  await page.locator('#save-custom-setup').click();
  await expect(page.locator('#recall-setup')).toBeVisible();
  await page.reload();
  await expect(page.locator('#recall-select')).toContainText('Friday performance');
  await expect(page.locator('#first-run-guide')).not.toHaveAttribute('open', '');
  await page.locator('#start-saved-setup').click();
  await expect(page.locator('#routing-heartbeat-heading')).toBeVisible();
  expect(fixture.posts).toHaveLength(1);
  expect(fixture.posts[0]).toMatchObject({
    path: '/api/connect',
    body: { serialName: config.serialName, midiLabel: config.midiLabel }
  });
  expect(fixture.posts[0].body).not.toHaveProperty('activeProfile');
  await expect(page.locator('#stage-performance-setup')).toContainText('Friday performance');
  await expect(page.locator('#stage-performance-setup')).toContainText(
    'advisory MN42 profile: Performance A'
  );
  await page.getByRole('button', { name: 'Setup', exact: true }).click();
  await expect(page.locator('#start-saved-setup')).toBeDisabled();
  await expect(page.locator('#recall-summary')).not.toContainText('Not started');
  expect(fixture.errors).toEqual([]);
});

test('passive soundcheck expects only mapped slots and shows a truthful output verdict', async ({
  page
}) => {
  const fixture = await consoleFixture(page, { running: true });
  await page.getByRole('button', { name: 'Monitor', exact: true }).click();
  fixture.telemetry([10], 'baseline');
  await page.locator('#start-soundcheck').click();
  fixture.telemetry([11], 'movement');
  fixture.update({
    routes: [
      { flow: 'serial->osc', kind: 'telemetry', traceId: 'movement', hostTimestampMs: Date.now() }
    ]
  });
  await expect(page.locator('#soundcheck-status')).toContainText('Soundcheck complete', {
    timeout: 5000
  });
  await expect(page.locator('#soundcheck-status')).toContainText('Confirm sound or visuals');
  await expect(page.locator('#soundcheck-results')).toContainText('MIDI output: Not configured');
  expect(fixture.posts).toHaveLength(0);
  await expect(page.locator('#device-session-grid')).not.toBeVisible();
  await page.getByText('Device details', { exact: true }).click();
  await expect(page.locator('#device-session-grid')).toBeVisible();
  await page.getByRole('button', { name: 'View route trace', exact: true }).click();
  await expect(page.locator('#routes-heading')).toBeFocused();
  expect(fixture.errors).toEqual([]);
});

test('patchbay stays neutral when idle and exposes disconnection with text', async ({ page }) => {
  const fixture = await consoleFixture(page, { running: true });
  await page.setViewportSize({ width: 390, height: 844 });
  await page.getByRole('button', { name: 'Monitor', exact: true }).click();
  await expect(page.locator('#route-device-osc strong')).toHaveText('not seen');
  await expect(page.locator('#route-device-osc')).not.toHaveClass(/is-recent/);
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(
    true
  );
  await page.screenshot({ path: '/tmp/mn42-bridge-monitor-mobile.png', fullPage: true });
  await page.setViewportSize({ width: 1280, height: 1000 });
  await page.screenshot({ path: '/tmp/mn42-bridge-monitor-desktop.png', fullPage: true });
  fixture.update({ serialConnected: false });
  await expect(page.locator('#route-device-osc strong')).toHaveText('USB disconnected');
  await expect(page.locator('#start-soundcheck')).toBeDisabled();
  expect(fixture.errors).toEqual([]);
});

test('soundcheck waits for mapped MIDI and interrupts on disconnect', async ({ page }) => {
  const fixture = await consoleFixture(page, { running: true });
  fixture.update({
    config: {
      ...config,
      outboundMidiMappings: [{ source: 'slots', sourceIndex: 0, channel: 1, controller: 74 }]
    }
  });
  await page.getByRole('button', { name: 'Monitor', exact: true }).click();
  await expect(page.locator('#route-device-midi small')).toContainText('1 configured MIDI routes');
  fixture.telemetry([10], 'baseline');
  await page.locator('#start-soundcheck').click();
  fixture.telemetry([11], 'movement');
  fixture.update({
    routes: [
      { flow: 'serial->osc', kind: 'telemetry', traceId: 'movement', hostTimestampMs: Date.now() }
    ]
  });
  await expect(page.locator('#soundcheck-results')).toContainText('OSC output: ✓ Output observed');
  await expect(page.locator('#soundcheck-results')).toContainText('MIDI output: Following signal');
  await expect(page.locator('#soundcheck-status')).not.toContainText('complete');
  fixture.update({ serialConnected: false });
  await expect(page.locator('#soundcheck-status')).toContainText('Soundcheck interrupted');
  expect(fixture.posts).toHaveLength(0);
  expect(fixture.errors).toEqual([]);
});

test('configuration recovery and route alerts lead to the right actions', async ({ page }) => {
  const fixture = await consoleFixture(page, { running: true });
  fixture.update({
    deviceSession: {
      deviceAuthority: 'uncertain',
      dirty: true,
      configValidation: { status: 'verified' }
    },
    alerts: {
      active: [
        {
          code: 'osc_route_stale',
          severity: 'warn',
          message: 'Inspect OSC route',
          at: new Date().toISOString()
        }
      ]
    }
  });
  await page.getByRole('button', { name: 'Monitor', exact: true }).click();
  await expect(page.locator('#patch-config')).toContainText('Config needs attention');
  await expect(page.locator('#stage-open-configurator')).toHaveText('Open App to resolve');
  await page.locator('#alert-history').getByRole('button', { name: 'View route trace' }).click();
  await expect(page.locator('#routes-heading')).toBeFocused();
  expect(fixture.errors).toEqual([]);
});
