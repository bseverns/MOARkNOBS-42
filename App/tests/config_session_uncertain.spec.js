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
    applyBridgeConfig: overrides.applyBridgeConfig,
    refreshBridgeSession: overrides.refreshBridgeSession
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
  expect(state.deviceAuthority).toBe('uncertain');
  expect(state.draftState).toBe('dirty');
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

test('malformed ACK with successful readback returns a classified recovery result', async () => {
  const events = [];
  let calls = 0;
  const deviceTruth = { ...baseConfig(), filter: { freq: 100 } };
  const session = createSession(async () => {
    calls += 1;
    if (calls === 1) return { checksum: 'wrong-checksum' };
    return { config: deviceTruth };
  }, events);
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

  await expect(session.apply()).resolves.toEqual({
    applied: false,
    verifiedDeviceState: true,
    verifiedBy: 'readback',
    receiptValid: false,
    checksum: 'candidate-checksum'
  });
  expect(session.getState().live.filter.freq).toBe(100);
  expect(session.getState().staged.filter.freq).toBe(321);
  expect(session.getState().dirty).toBe(true);
  expect(session.getState().transactionState).toBe('verified-device-different');
  expect(session.getState().deviceAuthority).toBe('verified-device-different');
  expect(session.getState().draftState).toBe('dirty');
});

test('direct readback mismatch reports verified-device-different', async () => {
  const events = [];
  let calls = 0;
  const session = createSession(async () => {
    calls += 1;
    if (calls === 1) {
      return {
        checksum: 'candidate-checksum',
        applied_checksum: 'device-state-checksum',
        storage_generation: 7
      };
    }
    return { config: baseConfig() };
  }, events, {
    remoteManifest: { persistence: { backend: 'littlefs' } }
  });
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

  await expect(session.apply()).rejects.toThrow(/readback differs/i);
  expect(session.getState().transactionState).toBe('verified-device-different');
  expect(session.getState().live.filter.freq).toBe(100);
  expect(session.getState().staged.filter.freq).toBe(321);
});

test('editing after failed resynchronization preserves uncertainty and the next draft', async () => {
  const events = [];
  let deviceReadFails = true;
  const deviceApplied = { ...baseConfig(), filter: { freq: 321 } };
  const session = createSession(async (payload) => {
    if (payload.rpc === 'set_config') throw new Error('RPC timeout');
    if (deviceReadFails) throw new Error('readback unavailable');
    return { config: deviceApplied };
  }, events);
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

  await expect(session.apply()).rejects.toThrow(/firmware ACK/i);
  expect(session.getState().transactionState).toBe('uncertain');

  session.stage((draft) => ({ ...draft, filter: { freq: 654 } }));
  expect(session.getState().transactionState).toBe('uncertain');
  expect(session.getState().deviceAuthority).toBe('uncertain');
  expect(session.getState().draftState).toBe('dirty');
  expect(session.getState().staged.filter.freq).toBe(654);
  await expect(session.apply()).rejects.toThrow(/already in progress|resynchronized/i);

  deviceReadFails = false;
  await expect(session.resynchronize()).resolves.toEqual(deviceApplied);
  const state = session.getState();
  expect(state.live.filter.freq).toBe(321);
  expect(state.staged.filter.freq).toBe(654);
  expect(state.dirty).toBe(true);
  expect(state.deviceAuthority).toBe('verified');
  expect(state.draftState).toBe('dirty');
});

for (const ordering of ['event-before-rejection', 'rejection-before-event']) {
  test(`Bridge uncertainty survives ${ordering}`, async () => {
    const events = [];
    let session;
    const uncertainSnapshot = {
      liveConfig: baseConfig(),
      stagedConfig: { ...baseConfig(), filter: { freq: 321 } },
      dirty: true,
      lastApplyResult: { status: 'uncertain', seq: 17, checksum: 'bridge-checksum' }
    };
    session = createSession(async () => ({}), events, {
      isBridgeSessionActive: () => true,
      applyBridgeConfig: async () => {
        if (ordering === 'event-before-rejection') session.syncFromSession(uncertainSnapshot);
        throw Object.assign(new Error('Bridge apply timed out'), { code: 'apply_timeout' });
      },
      refreshBridgeSession: async () => uncertainSnapshot
    });
    session.syncFromDevice(baseConfig());
    session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

    await expect(session.apply()).rejects.toThrow(/timed out/i);
    if (ordering === 'rejection-before-event') session.syncFromSession(uncertainSnapshot);
    expect(session.getState().transactionState).toBe('uncertain');
    await expect(session.apply()).rejects.toThrow(/already in progress|resynchronized/i);
  });
}

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
