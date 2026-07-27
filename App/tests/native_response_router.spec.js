import { test, expect } from '@playwright/test';
import {
  createChunkedReadAssembler,
  handleNativePendingResponse
} from '../runtime/native_response_router.js';

function createHarness(kind) {
  const responses = [];
  const rpcKernel = {
    handleRpcResponse: (response) => responses.push(response)
  };
  return {
    activePending: { id: 42, protocolMode: 'native', nativeRequest: { kind } },
    rpcKernel,
    responses
  };
}

function handle(kind, msg, overrides = {}) {
  const harness = createHarness(kind);
  const handled = handleNativePendingResponse({
    msg,
    activePending: harness.activePending,
    activePendingId: harness.activePending.id,
    rpcKernel: harness.rpcKernel,
    isManifestPayload: (payload) => payload?.device_name === 'MN42',
    isConfigPayload: (payload) => Array.isArray(payload?.slots) && Array.isArray(payload?.pots),
    ...overrides
  });
  return { handled, responses: harness.responses };
}

test('native response router resolves profile get payloads', () => {
  const msg = { profile: 2, arp: { enabled: true }, lfos: [], routes: [] };
  const { handled, responses } = handle('profile_get', msg);

  expect(handled).toBe(true);
  expect(responses).toEqual([{ id: 42, result: msg }]);
});

test('native response router maps profile command failures to RPC errors', () => {
  const { handled, responses } = handle('profile_load', {
    profile_loaded: false,
    error: 'Profile slot empty'
  });

  expect(handled).toBe(true);
  expect(responses).toEqual([{ id: 42, error: { message: 'Profile slot empty' } }]);
});

test('native response router resolves modulation matrix reports', () => {
  const msg = {
    command: 'GET_MOD_MATRIX',
    sources: { lfo: [0], ef: [0, 1] },
    routes: [{ source: 'lfo0', destination: 'slot7.value' }]
  };
  const { handled, responses } = handle('mod_matrix', msg);

  expect(handled).toBe(true);
  expect(responses).toEqual([{ id: 42, result: msg }]);
});

test('native response router rejects failed profile set responses', () => {
  const { handled, responses } = handle('profile_set', {
    type: 'response',
    status: 'error',
    message: 'Schema rejected profile'
  });

  expect(handled).toBe(true);
  expect(responses).toEqual([{ id: 42, error: { message: 'Schema rejected profile' } }]);
});

test('native response router ignores unrelated payloads', () => {
  const { handled, responses } = handle('profile_get', { profile: 2 });

  expect(handled).toBe(false);
  expect(responses).toEqual([]);
});

test('native response router maps generic native errors with code and message', () => {
  const { handled, responses } = handle('clock_get', {
    type: 'error',
    code: 'bad_state',
    message: 'Clock unavailable'
  });

  expect(handled).toBe(true);
  expect(responses).toEqual([
    { id: 42, error: { code: 'bad_state', message: 'Clock unavailable' } }
  ]);
});

test('sequence-correlated native reads ignore shape-matching unsolicited telemetry', () => {
  const harness = createHarness('jitter_get');
  harness.activePending.nativeRequest.expectedSequence = 42;
  const common = {
    activePending: harness.activePending,
    activePendingId: 42,
    rpcKernel: harness.rpcKernel,
    isManifestPayload: () => false,
    isConfigPayload: () => false
  };
  expect(handleNativePendingResponse({
    ...common,
    msg: { command: 'GET_JITTER', seq: 41, depth: 0.2, smoothness: 0.5 }
  })).toBe(false);
  expect(handleNativePendingResponse({
    ...common,
    msg: { command: 'GET_JITTER', seq: 42, depth: 0.2, smoothness: 0.5 }
  })).toBe(true);
  expect(harness.responses).toEqual([{ id: 42, result: expect.objectContaining({ seq: 42 }) }]);
});

function fnv1a(value) {
  let hash = 2166136261;
  for (const byte of new TextEncoder().encode(value)) {
    hash ^= byte;
    hash = Math.imul(hash, 16777619);
  }
  return hash >>> 0;
}

test('native response router reassembles checksum-verified config chunks', () => {
  const config = { pots: [], slots: [], led: {} };
  const payload = JSON.stringify(config);
  const harness = createHarness('config_chunked');
  const common = {
    activePending: harness.activePending,
    activePendingId: harness.activePending.id,
    rpcKernel: harness.rpcKernel,
    isManifestPayload: () => false,
    isConfigPayload: (value) => Array.isArray(value?.pots) && Array.isArray(value?.slots),
    chunkedReadAssembler: createChunkedReadAssembler()
  };

  const first = handleNativePendingResponse({
    ...common,
    msg: { type: 'read_chunk', command: 'GET_CONFIG', index: 1, total: 2, checksum: fnv1a(payload), data: payload.slice(10) }
  });
  const second = handleNativePendingResponse({
    ...common,
    msg: { type: 'read_chunk', command: 'GET_CONFIG', index: 0, total: 2, checksum: fnv1a(payload), data: payload.slice(0, 10) }
  });

  expect(first).toBe(true);
  expect(second).toBe(true);
  expect(harness.responses).toEqual([{ id: 42, result: { config } }]);
});

test('native response router rejects corrupted chunked reads', () => {
  const harness = createHarness('config_chunked');
  const handled = handleNativePendingResponse({
    msg: { type: 'read_chunk', command: 'GET_CONFIG', index: 0, total: 1, checksum: 1, data: '{"pots":[],"slots":[],"led":{}}' },
    activePending: harness.activePending,
    activePendingId: harness.activePending.id,
    rpcKernel: harness.rpcKernel,
    isManifestPayload: () => false,
    isConfigPayload: () => true,
    chunkedReadAssembler: createChunkedReadAssembler()
  });

  expect(handled).toBe(true);
  expect(harness.responses).toEqual([
    { id: 42, error: { message: 'Chunked device read checksum mismatch' } }
  ]);
});
