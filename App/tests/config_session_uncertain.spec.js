import { test, expect } from '@playwright/test';
import { createConfigSession } from '../runtime/config_session.js';
import { clone, shallowDiff } from '../runtime/runtime_utils.js';

const baseConfig = () => ({ slots: [], efSlots: [], filter: { freq: 100 }, arg: {}, led: {} });

function createSession(sendRpc, events, overrides = {}) {
  return createConfigSession({
    normalizeConfig: (value) => clone(value),
    clone,
    shallowDiff,
    digest: async () => 'candidate-checksum',
    emit: (type, payload) => events.push({ type, payload }),
    sendRpc,
    nextSeq: () => 17,
    applyRpcTimeoutMs: 10,
    slotTypeNames: ['OFF'],
    localSlotMetaManager: {
      extractFromConfig() {},
      mergeIntoConfig: (value) => value,
      updateEntry: () => true
    },
    stateSnapshotStore: { persist() {} },
    getManifest: () => ({}),
    getRemoteManifest: () => ({ capabilities: {}, ...(overrides.remoteManifest ?? {}) }),
    getSchema: () => ({ schema_version: 6 }),
    getSchemaSource: () => 'device',
    getValidator: () => Object.assign(() => true, { errors: [] }),
    isBridgeSessionActive: overrides.isBridgeSessionActive,
    applyBridgeConfig: overrides.applyBridgeConfig
  });
}

test('a transmitted Apply timeout remains dirty and uncertain when readback cannot establish device truth', async () => {
  const events = [];
  const session = createSession(async () => {
    throw new Error('RPC timeout');
  }, events);
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

  await expect(session.apply()).rejects.toThrow(/firmware ACK/i);
  const state = session.getState();
  expect(state.transactionState).toBe('uncertain');
  expect(state.dirty).toBe(true);
  expect(state.staged.filter.freq).toBe(321);
  expect(events).toContainEqual(expect.objectContaining({ type: 'apply-uncertain' }));
});

test('a successful resynchronization adopts device truth after an ambiguous Apply', async () => {
  const events = [];
  let calls = 0;
  const deviceTruth = { ...baseConfig(), filter: { freq: 321 } };
  const session = createSession(async () => {
    calls += 1;
    if (calls === 1) throw new Error('RPC timeout');
    return { config: deviceTruth };
  }, events);
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

  await expect(session.apply()).resolves.toEqual({
    applied: true,
    verifiedBy: 'readback',
    ackReceived: false,
    checksum: 'candidate-checksum'
  });
  const state = session.getState();
  expect(state.transactionState).toBe('verified');
  expect(state.dirty).toBe(false);
  expect(state.live.filter.freq).toBe(321);
  expect(events).toContainEqual(expect.objectContaining({ type: 'resynchronized' }));
});

test('Bridge Apply promotes its immutable candidate while retaining an edit made in flight', async () => {
  const events = [];
  let resolveApply;
  const bridgeApply = new Promise((resolve) => {
    resolveApply = resolve;
  });
  const session = createSession(async () => ({}), events, {
    isBridgeSessionActive: () => true,
    applyBridgeConfig: () => bridgeApply
  });
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

  const applyPromise = session.apply();
  expect(session.getState().transactionState).toBe('applying');
  session.stage((draft) => ({ ...draft, filter: { freq: 654 } }));
  resolveApply({
    applied: true,
    checksum: 'bridge-checksum',
    authoritativeConfig: { ...baseConfig(), filter: { freq: 321 } }
  });

  await expect(applyPromise).resolves.toEqual({
    applied: true,
    checksum: 'bridge-checksum'
  });
  const state = session.getState();
  expect(state.live.filter.freq).toBe(321);
  expect(state.staged.filter.freq).toBe(654);
  expect(state.dirty).toBe(true);
  expect(state.transactionState).toBe('dirty');
});

test('direct Apply verifies its immutable candidate while retaining an edit made in flight', async () => {
  const events = [];
  let transmitted;
  let resolveAck;
  const session = createSession(async (payload) => {
    if (payload.rpc === 'set_config') {
      transmitted = clone(payload.config);
      return new Promise((resolve) => {
        resolveAck = () => resolve({
          checksum: 'candidate-checksum',
          applied_checksum: 'device-state-checksum',
          storage_generation: 7
        });
      });
    }
    return { config: transmitted };
  }, events, {
    remoteManifest: { persistence: { backend: 'littlefs' } }
  });
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

  const applyPromise = session.apply();
  await expect.poll(() => transmitted?.filter?.freq).toBe(321);
  session.stage((draft) => ({ ...draft, filter: { freq: 654 } }));
  resolveAck();

  await expect(applyPromise).resolves.toEqual({
    applied: true,
    checksum: 'candidate-checksum',
    appliedChecksum: 'device-state-checksum',
    storageGeneration: 7
  });
  const state = session.getState();
  expect(state.live.filter.freq).toBe(321);
  expect(state.staged.filter.freq).toBe(654);
  expect(state.dirty).toBe(true);
  expect(state.transactionState).toBe('dirty');
});
