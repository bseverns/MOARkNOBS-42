import { test, expect } from '@playwright/test';
import { createConfigSession } from '../runtime/config_session.js';
import { clone, shallowDiff } from '../runtime/runtime_utils.js';

const baseConfig = () => ({ slots: [], efSlots: [], filter: { freq: 100 }, arg: {}, led: {} });

function createSession(sendRpc, events) {
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
    getRemoteManifest: () => ({ capabilities: {} }),
    getSchema: () => ({ schema_version: 6 }),
    getSchemaSource: () => 'device',
    getValidator: () => Object.assign(() => true, { errors: [] })
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

  await expect(session.apply()).rejects.toThrow(/firmware ACK/i);
  const state = session.getState();
  expect(state.transactionState).toBe('verified');
  expect(state.dirty).toBe(false);
  expect(state.live.filter.freq).toBe(321);
  expect(events).toContainEqual(expect.objectContaining({ type: 'resynchronized' }));
});
