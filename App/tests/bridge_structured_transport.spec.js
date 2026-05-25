import { test, expect } from '@playwright/test';

test('bridge-served app prefers the structured bridge session when available', async ({ page }) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');

    const originalFetch = window.fetch.bind(window);
    const BRIDGE_HTTP = 'http://127.0.0.1:8787';
    const BRIDGE_WS = 'ws://127.0.0.1:8787';
    const eventSockets = new Set();

    window.__bridgeWrites = [];
    window.__bridgeApiCalls = [];
    window.__bridgeStageBodies = [];

    const sessionState = {
      connected: true,
      helloSeen: true,
      ready: true,
      schemaSource: 'device',
      manifest: {
        device_name: 'MOARkNOBS-42',
        fw_version: 'bridge-fw',
        git_sha: 'bridge123',
        build_time: '2026-05-25T12:00:00Z',
        schema_version: 6,
        slot_count: 42,
        pot_count: 42,
        envelope_count: 6,
        led_count: 12,
        capabilities: {
          profile_save: false,
          profile_load: false,
          profile_reset: false,
          macro_snapshot: false,
          scenes: false,
          clock_live: false,
          usb_midi_toggle: false,
          note_dynamics_live: false,
          jitter_live: false
        }
      },
      schema: {
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
      },
      liveConfig: {
        slots: Array.from({ length: 42 }, (_, index) => ({
          index,
          type: 'CC',
          channel: (index % 16) + 1,
          data1: index % 128,
          active: true,
          ef_index: index % 6,
          arg: { enabled: false, method: 0, sourceA: 0, sourceB: 1 }
        })),
        efSlots: Array.from({ length: 6 }, (_, index) => ({ index, slots: [index] })),
        filter: { type: 'LOWPASS', freq: 800, q: 1 },
        arg: { enabled: false, method: 0, a: 0, b: 1 },
        led: { brightness: 64, rgb: { r: 16, g: 32, b: 48 }, hex: '#102030', mode: 'STATIC' }
      },
      stagedConfig: null,
      dirty: false,
      lastApplyResult: null,
      powerSafety: {
        power_profile: 'POWER_CHOKED_V1',
        led_brightness_cap: 26,
        rail_topology_verified: false
      },
      firmwareIdentity: {
        device_name: 'MOARkNOBS-42',
        fw_version: 'bridge-fw',
        schema_version: 6
      },
      lastError: null
    };
    sessionState.stagedConfig = JSON.parse(JSON.stringify(sessionState.liveConfig));

    function structuredMessage(event, payload) {
      return `${JSON.stringify({
        version: 1,
        event,
        at: new Date().toISOString(),
        payload
      })}\n`;
    }

    function broadcast(event, payload) {
      const message = structuredMessage(event, payload);
      for (const socket of [...eventSockets]) {
        socket.__emit('message', { data: message });
      }
    }

    window.fetch = async (input, init = {}) => {
      const url = new URL(typeof input === 'string' ? input : input.url, window.location.href);
      if (url.origin !== BRIDGE_HTTP || !url.pathname.startsWith('/api/device/')) {
        return originalFetch(input, init);
      }

      window.__bridgeApiCalls.push({
        method: init.method || 'GET',
        path: `${url.pathname}${url.search}`
      });

      if (url.pathname === '/api/device/session') {
        return new Response(JSON.stringify({ session: sessionState }), {
          status: 200,
          headers: { 'content-type': 'application/json' }
        });
      }

      if (url.pathname === '/api/device/stage') {
        const body = JSON.parse(init.body || '{}');
        sessionState.stagedConfig = body.config;
        sessionState.dirty = true;
        window.__bridgeStageBodies.push(body.config);
        broadcast('device.config.staged', { config: sessionState.stagedConfig });
        broadcast('device.config.dirty', { dirty: true });
        return new Response(
          JSON.stringify({
            result: { staged: sessionState.stagedConfig, dirty: true }
          }),
          {
            status: 200,
            headers: { 'content-type': 'application/json' }
          }
        );
      }

      if (url.pathname === '/api/device/apply') {
        sessionState.liveConfig = JSON.parse(JSON.stringify(sessionState.stagedConfig));
        sessionState.dirty = false;
        sessionState.lastApplyResult = {
          status: 'ack',
          checksum: 'bridge-checksum-1'
        };
        broadcast('device.apply.ack', { checksum: 'bridge-checksum-1', seq: 1 });
        broadcast('device.config.live', {
          config: sessionState.liveConfig,
          lastApplyResult: sessionState.lastApplyResult
        });
        broadcast('device.config.dirty', { dirty: false });
        return new Response(
          JSON.stringify({
            result: { applied: true, checksum: 'bridge-checksum-1', seq: 1 }
          }),
          {
            status: 200,
            headers: { 'content-type': 'application/json' }
          }
        );
      }

      if (url.pathname === '/api/device/rollback') {
        sessionState.stagedConfig = JSON.parse(JSON.stringify(sessionState.liveConfig));
        sessionState.dirty = false;
        broadcast('device.apply.rollback', { reason: 'operator_request' });
        broadcast('device.config.staged', { config: sessionState.stagedConfig });
        broadcast('device.config.dirty', { dirty: false });
        return new Response(
          JSON.stringify({
            result: { rolledBack: true, reason: 'operator_request' }
          }),
          {
            status: 200,
            headers: { 'content-type': 'application/json' }
          }
        );
      }

      return new Response(JSON.stringify({ error: 'not found' }), { status: 404 });
    };

    class MockWebSocket {
      static CONNECTING = 0;
      static OPEN = 1;
      static CLOSING = 2;
      static CLOSED = 3;

      constructor(url) {
        this.url = String(url);
        this.readyState = MockWebSocket.CONNECTING;
        this.binaryType = 'arraybuffer';
        this.listeners = new Map();
        this.kind = this.url === `${BRIDGE_WS}/ws/events` ? 'events' : 'raw';
        if (this.kind === 'events') {
          eventSockets.add(this);
        }
        setTimeout(() => {
          this.readyState = MockWebSocket.OPEN;
          this.__emit('open', {});
          if (this.kind === 'events') {
            this.__emit('message', {
              data: structuredMessage('device.config.live', {
                config: sessionState.liveConfig,
                lastApplyResult: sessionState.lastApplyResult
              })
            });
            this.__emit('message', {
              data: structuredMessage('device.config.staged', {
                config: sessionState.stagedConfig
              })
            });
            this.__emit('message', {
              data: structuredMessage('device.config.dirty', {
                dirty: sessionState.dirty
              })
            });
            this.__emit('message', {
              data: structuredMessage('device.ready', {
                manifest: sessionState.manifest,
                firmwareIdentity: sessionState.firmwareIdentity,
                powerSafety: sessionState.powerSafety,
                schemaSource: sessionState.schemaSource
              })
            });
          }
        }, 0);
      }

      addEventListener(type, handler, options = {}) {
        const set = this.listeners.get(type) ?? new Set();
        set.add({ handler, once: Boolean(options.once) });
        this.listeners.set(type, set);
      }

      removeEventListener(type, handler) {
        const set = this.listeners.get(type);
        if (!set) return;
        for (const entry of [...set]) {
          if (entry.handler === handler) set.delete(entry);
        }
      }

      __emit(type, event) {
        const set = this.listeners.get(type);
        if (!set) return;
        for (const entry of [...set]) {
          try {
            entry.handler(event);
          } finally {
            if (entry.once) set.delete(entry);
          }
        }
      }

      send(data) {
        if (this.kind !== 'raw') return;
        const trimmed = String(data).trim();
        window.__bridgeWrites.push(trimmed);
        if (trimmed.startsWith('GET_PROFILE,')) {
          this.__emit('message', {
            data: `${JSON.stringify({
              profile: Number(trimmed.split(',')[1] || 0),
              arp: {
                length_ticks: 12,
                shape: 0,
                swing_percent: 0,
                gate_percent: 50,
                octave_range: 0
              },
              lfo: {
                lfos: [
                  {
                    index: 0,
                    shape: 0,
                    frequency_hz: 1,
                    depth: 0,
                    bipolar: false,
                    sync: false,
                    sync_ratio: 0
                  },
                  {
                    index: 1,
                    shape: 1,
                    frequency_hz: 0.5,
                    depth: 0,
                    bipolar: true,
                    sync: false,
                    sync_ratio: 0
                  }
                ],
                routes: []
              }
            })}\n`
          });
          return;
        }
        if (trimmed === 'GET_SCENES') {
          this.__emit('message', {
            data: `${JSON.stringify({ scenes: [] })}\n`
          });
          return;
        }
      }

      close() {
        if (this.kind === 'events') {
          eventSockets.delete(this);
        }
        this.readyState = MockWebSocket.CLOSED;
        this.__emit('close', {});
      }
    }

    window.WebSocket = MockWebSocket;
    window.__MN42_RUNTIME_OPTIONS = {
      wsUrl: `${BRIDGE_WS}/ws`,
      bridgeApiBaseUrl: BRIDGE_HTTP,
      bridgeTransportMode: 'session'
    };
  });

  await page.goto('/index.html');
  await page.waitForLoadState('load');
  await page.evaluate(async () => {
    await window.__MN42_RUNTIME.connect();
  });

  await expect(page.locator('#connection-pill')).toHaveText('Connected');
  await expect(page.locator('#connection-banner')).toContainText('via Bridge session');
  await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Bridge session');
  await expect
    .poll(async () => page.evaluate(() => window.__MN42_RUNTIME.getState().transportMode))
    .toBe('bridge-session');

  await page.evaluate(() => {
    window.__MN42_RUNTIME.stage((draft) => {
      draft.filter = { ...(draft.filter ?? {}), freq: 321 };
      return draft;
    });
  });

  await expect
    .poll(async () => page.evaluate(() => window.__bridgeStageBodies.length))
    .toBeGreaterThan(0);

  const applyResult = await page.evaluate(async () => window.__MN42_RUNTIME.apply());
  expect(applyResult).toEqual(
    expect.objectContaining({ applied: true, checksum: 'bridge-checksum-1' })
  );

  const bridgeState = await page.evaluate(() => ({
    writes: window.__bridgeWrites,
    apiCalls: window.__bridgeApiCalls
  }));
  expect(bridgeState.writes).not.toContain('HELLO');
  expect(bridgeState.writes).not.toContain('GET_MANIFEST');
  expect(bridgeState.writes).not.toContain('GET_SCHEMA');
  expect(bridgeState.writes).not.toContain('GET_CONFIG');
  expect(bridgeState.apiCalls.map((entry) => entry.path)).toContain('/api/device/session?warm=1');
  expect(bridgeState.apiCalls.map((entry) => entry.path)).toContain('/api/device/stage');
  expect(bridgeState.apiCalls.map((entry) => entry.path)).toContain('/api/device/apply');
});

test('bridge-served app falls back to the raw bridge websocket when structured session is unavailable', async ({
  page
}) => {
  await page.addInitScript(() => {
    window.localStorage?.clear?.();
    window.localStorage?.setItem?.('moarknobs:ui-mode', 'advanced');

    const originalFetch = window.fetch.bind(window);
    const BRIDGE_HTTP = 'http://127.0.0.1:8787';
    const BRIDGE_WS = 'ws://127.0.0.1:8787';

    window.__bridgeWrites = [];
    window.__bridgeApiCalls = [];

    const manifest = {
      device_name: 'MOARkNOBS-42',
      fw_version: 'raw-bridge-fw',
      git_sha: 'raw12345',
      build_time: '2026-05-25T13:00:00Z',
      schema_version: 6,
      slot_count: 42,
      pot_count: 42,
      envelope_count: 6,
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
    const config = {
      pots: Array.from({ length: 42 }, (_, index) => ({
        index,
        channel: (index % 16) + 1,
        cc: index % 128
      })),
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

    window.fetch = async (input, init = {}) => {
      const url = new URL(typeof input === 'string' ? input : input.url, window.location.href);
      if (url.origin === BRIDGE_HTTP && url.pathname === '/api/device/session') {
        window.__bridgeApiCalls.push({
          method: init.method || 'GET',
          path: `${url.pathname}${url.search}`
        });
        return new Response(
          JSON.stringify({
            error: { code: 'bridge_unavailable', message: 'Structured session unavailable' }
          }),
          {
            status: 503,
            headers: { 'content-type': 'application/json' }
          }
        );
      }
      return originalFetch(input, init);
    };

    class MockWebSocket {
      static CONNECTING = 0;
      static OPEN = 1;
      static CLOSING = 2;
      static CLOSED = 3;

      constructor(url) {
        this.url = String(url);
        this.readyState = MockWebSocket.CONNECTING;
        this.binaryType = 'arraybuffer';
        this.listeners = new Map();
        this.buffer = [];
        setTimeout(() => {
          this.readyState = MockWebSocket.OPEN;
          this.__emit('open', {});
        }, 0);
      }

      addEventListener(type, handler, options = {}) {
        const set = this.listeners.get(type) ?? new Set();
        set.add({ handler, once: Boolean(options.once) });
        this.listeners.set(type, set);
      }

      removeEventListener(type, handler) {
        const set = this.listeners.get(type);
        if (!set) return;
        for (const entry of [...set]) {
          if (entry.handler === handler) set.delete(entry);
        }
      }

      __emit(type, event) {
        const set = this.listeners.get(type);
        if (!set) return;
        for (const entry of [...set]) {
          try {
            entry.handler(event);
          } finally {
            if (entry.once) set.delete(entry);
          }
        }
      }

      __push(line) {
        this.__emit('message', { data: `${line}\n` });
      }

      send(data) {
        const trimmed = String(data).trim();
        window.__bridgeWrites.push(trimmed);
        if (this.url !== `${BRIDGE_WS}/ws`) return;
        if (trimmed === 'HELLO') {
          this.__push(JSON.stringify({ hello: 'mn42' }));
          return;
        }
        if (trimmed === 'GET_MANIFEST') {
          this.__push(JSON.stringify(manifest));
          return;
        }
        if (trimmed === 'GET_SCHEMA') {
          this.__push(JSON.stringify(schema));
          return;
        }
        if (trimmed === 'GET_CONFIG') {
          this.__push(JSON.stringify(config));
        }
      }

      close() {
        this.readyState = MockWebSocket.CLOSED;
        this.__emit('close', {});
      }
    }

    window.WebSocket = MockWebSocket;
    window.__MN42_RUNTIME_OPTIONS = {
      wsUrl: `${BRIDGE_WS}/ws`,
      bridgeApiBaseUrl: BRIDGE_HTTP,
      bridgeTransportMode: 'session'
    };
  });

  await page.goto('/index.html');
  await page.waitForLoadState('load');
  await page.evaluate(async () => {
    await window.__MN42_RUNTIME.connect();
  });

  await expect(page.locator('#connection-pill')).toHaveText('Connected');
  await expect(page.locator('#connection-banner')).toContainText('via Bridge raw transport');
  await expect(page.locator('#transport-lane-chip')).toHaveText('Transport · Bridge raw');
  await expect
    .poll(async () => page.evaluate(() => window.__MN42_RUNTIME.getState().transportMode))
    .toBe('bridge-raw');

  const bridgeState = await page.evaluate(() => ({
    writes: window.__bridgeWrites,
    apiCalls: window.__bridgeApiCalls
  }));
  expect(bridgeState.apiCalls.map((entry) => entry.path)).toContain('/api/device/session?warm=1');
  expect(bridgeState.writes).toContain('HELLO');
  expect(bridgeState.writes).toContain('GET_MANIFEST');
  expect(bridgeState.writes).toContain('GET_SCHEMA');
  expect(bridgeState.writes).toContain('GET_CONFIG');
});
