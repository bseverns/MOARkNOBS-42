import { test, expect } from '@playwright/test';
import { createBridgeSessionClient } from '../runtime/bridge_session_client.js';

const BRIDGE_CONTRACT = Object.freeze({
  bridge_api_version: 1,
  event_contract_version: 1,
  bridge_version: '1.0.0-test',
  bridge_source_sha: 'test-source-sha',
  supported_schema_versions: [8],
  verified_apply: true,
  structured_session: true
});

function contractResponse() {
  return {
    ok: true,
    status: 200,
    text: async () => JSON.stringify({ contract: BRIDGE_CONTRACT })
  };
}

function failingFetch(code, deviceSession = null, failureClass = undefined) {
  return async (url) => {
    if (String(url).endsWith('/api/contract')) return contractResponse();
    return {
      ok: false,
      status: 409,
      text: async () => JSON.stringify({
        error: { code, message: code, failureClass },
        state: { deviceSession }
      })
    };
  };
}

function successfulFetch(result, deviceSession = null) {
  return async (url) => {
    if (String(url).endsWith('/api/contract')) return contractResponse();
    return {
      ok: true,
      status: 200,
      text: async () => JSON.stringify({
        result,
        state: { deviceSession }
      })
    };
  };
}

test('Bridge client negotiates and caches the explicit contract', async () => {
  const requested = [];
  const client = createBridgeSessionClient({
    baseUrl: 'http://bridge.test',
    fetchImpl: async (url) => {
      requested.push(String(url));
      return contractResponse();
    }
  });

  await expect(client.getContract()).resolves.toEqual(BRIDGE_CONTRACT);
  await expect(client.getContract()).resolves.toEqual(BRIDGE_CONTRACT);
  expect(requested).toEqual(['http://bridge.test/api/contract']);
});

test('Bridge client rejects unsupported API, event, schema, and capability contracts', async () => {
  const cases = [
    ['unsupported_bridge_api_version', { bridge_api_version: 2 }],
    ['unsupported_bridge_event_contract_version', { event_contract_version: 2 }],
    ['unsupported_bridge_schema_version', { supported_schema_versions: [7] }],
    ['unsupported_bridge_capabilities', { verified_apply: false }],
    ['unsupported_bridge_capabilities', { structured_session: false }]
  ];

  for (const [code, override] of cases) {
    const client = createBridgeSessionClient({
      baseUrl: 'http://bridge.test',
      fetchImpl: async () => ({
        ok: true,
        status: 200,
        text: async () => JSON.stringify({
          contract: { ...BRIDGE_CONTRACT, ...override }
        })
      })
    });
    await expect(client.getContract()).rejects.toMatchObject({ code });
  }
});

test('Bridge client rejects event frames outside the negotiated contract', async () => {
  class MockWebSocket {
    static CONNECTING = 0;
    static OPEN = 1;
    static CLOSING = 2;
    static CLOSED = 3;

    constructor() {
      this.readyState = MockWebSocket.CONNECTING;
      this.listeners = new Map();
      queueMicrotask(() => {
        this.readyState = MockWebSocket.OPEN;
        this.emit('open', {});
      });
    }

    addEventListener(type, handler) {
      const handlers = this.listeners.get(type) ?? new Set();
      handlers.add(handler);
      this.listeners.set(type, handlers);
    }

    removeEventListener(type, handler) {
      this.listeners.get(type)?.delete(handler);
    }

    emit(type, event) {
      for (const handler of this.listeners.get(type) ?? []) handler(event);
    }

    close() {
      this.readyState = MockWebSocket.CLOSED;
    }
  }

  const errors = [];
  const events = [];
  const socketClient = createBridgeSessionClient({
    baseUrl: 'http://bridge.test',
    eventUrl: 'ws://bridge.test/ws/events',
    fetchImpl: async () => contractResponse(),
    WebSocketImpl: class extends MockWebSocket {
      constructor() {
        super();
        queueMicrotask(() =>
          this.emit('message', {
            data: `${JSON.stringify({
              version: 2,
              event: 'device.ready',
              at: 'now',
              payload: {}
            })}\n`
          })
        );
      }
    }
  });
  await socketClient.openEvents({
    onEvent: (event) => events.push(event),
    onError: (error) => errors.push(error)
  });
  await new Promise((resolve) => setTimeout(resolve, 0));

  expect(events).toEqual([]);
  expect(errors).toHaveLength(1);
  expect(errors[0]).toMatchObject({
    code: 'unsupported_bridge_event_contract_version'
  });
  socketClient.closeEvents();
});

test('Bridge client classifies preflight, firmware, and unknown Apply failures', async () => {
  const cases = [
    ['stale_session_revision', 'preflight-rejected'],
    ['apply_in_progress', 'preflight-rejected'],
    ['device_checksum', 'device-rejected-before-commit'],
    ['apply_timeout', 'transmission-unknown']
  ];

  for (const [code, expectedClass] of cases) {
    const client = createBridgeSessionClient({
      baseUrl: 'http://bridge.test',
      fetchImpl: failingFetch(code, { sessionRevision: 9 })
    });
    let error;
    try {
      await client.applyConfig({});
    } catch (caught) {
      error = caught;
    }
    expect(error?.code).toBe(code);
    expect(error?.bridgeFailureClass).toBe(expectedClass);
    expect(error?.bridgeSession?.sessionRevision).toBe(9);
  }
});

test('Bridge client prefers the server-owned failure class', async () => {
  const client = createBridgeSessionClient({
    baseUrl: 'http://bridge.test',
    fetchImpl: failingFetch(
      'legacy_or_future_code',
      { sessionRevision: 10 },
      'preflight-rejected'
    )
  });

  await expect(client.applyConfig({})).rejects.toMatchObject({
    code: 'legacy_or_future_code',
    bridgeFailureClass: 'preflight-rejected'
  });
});

test('Bridge client accepts only explicit transaction completion results', async () => {
  const cleanClient = createBridgeSessionClient({
    baseUrl: 'http://bridge.test',
    fetchImpl: successfulFetch(
      { applied: false, reason: 'clean' },
      { sessionRevision: 11 }
    )
  });
  await expect(cleanClient.applyConfig({})).resolves.toMatchObject({
    result: { applied: false, reason: 'clean' }
  });

  for (const malformedResult of [null, {}, { applied: false, reason: 'unknown' }]) {
    const client = createBridgeSessionClient({
      baseUrl: 'http://bridge.test',
      fetchImpl: successfulFetch(malformedResult, { sessionRevision: 12 })
    });
    await expect(client.applyConfig({})).rejects.toMatchObject({
      code: 'invalid_bridge_apply_response',
      bridgeFailureClass: 'transmission-unknown',
      bridgeSession: { sessionRevision: 12 }
    });
  }
});
