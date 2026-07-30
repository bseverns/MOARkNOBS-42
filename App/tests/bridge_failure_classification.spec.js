import { test, expect } from '@playwright/test';
import { createBridgeSessionClient } from '../runtime/bridge_session_client.js';

function failingFetch(code, deviceSession = null) {
  return async () => ({
    ok: false,
    status: 409,
    text: async () => JSON.stringify({
      error: { code, message: code },
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
