import { test, expect } from '@playwright/test';
import { createBridgeSessionClient } from '../runtime/bridge_session_client.js';

function failingFetch(code, deviceSession = null, failureClass = undefined) {
  return async () => ({
    ok: false,
    status: 409,
    text: async () => JSON.stringify({
      error: { code, message: code, failureClass },
      state: { deviceSession }
    })
  });
}

function successfulFetch(result, deviceSession = null) {
  return async () => ({
    ok: true,
    status: 200,
    text: async () => JSON.stringify({
      result,
      state: { deviceSession }
    })
  });
}

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
