import { test, expect } from '@playwright/test';
import { createConfigSession } from '../runtime/config_session.js';
import { createStateSnapshotStore } from '../runtime/state_snapshot.js';

function createMemoryStorage(initial = {}) {
  const values = new Map(Object.entries(initial));
  return {
    getItem: (key) => (values.has(key) ? values.get(key) : null),
    removeItem: (key) => values.delete(key),
    setItem: (key, value) => values.set(key, String(value)),
    snapshot: () => Object.fromEntries(values)
  };
}

test('state snapshot store persists and restores staged config for matching schema', () => {
  const storage = createMemoryStorage();
  const store = createStateSnapshotStore({
    storage,
    storageKey: 'state',
    getSchemaVersion: () => 6,
    now: () => 1000
  });

  store.persist({ slots: [{ value: 64 }] });

  expect(JSON.parse(storage.snapshot().state)).toEqual({
    schema_version: 6,
    staged: { slots: [{ value: 64 }] },
    timestamp: 1000
  });
  expect(store.readStagedConfig()).toEqual({ slots: [{ value: 64 }] });
});

test('state snapshot store ignores corrupt snapshots', () => {
  const storage = createMemoryStorage({ state: '{broken-json' });
  const store = createStateSnapshotStore({
    storage,
    storageKey: 'state',
    getSchemaVersion: () => 6
  });
  const originalDebug = console.debug;

  try {
    console.debug = () => {};
    expect(store.read()).toBeNull();
    expect(store.readStagedConfig()).toBeNull();
  } finally {
    console.debug = originalDebug;
  }
});

test('state snapshot store rejects schema-mismatched snapshots', () => {
  const storage = createMemoryStorage({
    state: JSON.stringify({
      schema_version: 5,
      staged: { slots: [{ value: 64 }] },
      timestamp: 1000
    })
  });
  const store = createStateSnapshotStore({
    storage,
    storageKey: 'state',
    getSchemaVersion: () => 6
  });

  expect(store.read()).toEqual({
    schema_version: 5,
    staged: { slots: [{ value: 64 }] },
    timestamp: 1000
  });
  expect(store.readStagedConfig()).toBeNull();
});

test('state snapshot store rejects stale snapshots when maxAgeMs is configured', () => {
  const storage = createMemoryStorage({
    state: JSON.stringify({
      schema_version: 6,
      staged: { slots: [{ value: 64 }] },
      timestamp: 1000
    })
  });
  const store = createStateSnapshotStore({
    storage,
    storageKey: 'state',
    getSchemaVersion: () => 6,
    now: () => 2500,
    maxAgeMs: 1000
  });

  expect(store.readStagedConfig()).toBeNull();
});

test('state snapshot store clears snapshots when persisting null staged config', () => {
  const storage = createMemoryStorage({ state: '{}' });
  const store = createStateSnapshotStore({
    storage,
    storageKey: 'state',
    getSchemaVersion: () => 6
  });

  store.persist(null);

  expect(storage.snapshot()).toEqual({});
});

test('config session restores only validated staged snapshots', () => {
  const emitted = [];
  const liveConfig = { slots: [{ value: 12 }], pots: [], led: {} };
  const restoredConfig = { slots: [{ value: 64 }], pots: [], led: {} };
  const session = createConfigSession({
    normalizeConfig: (config) => config,
    clone: (value) => JSON.parse(JSON.stringify(value)),
    shallowDiff: (before, after) =>
      JSON.stringify(before) === JSON.stringify(after) ? [] : [{ path: 'slots.0.value' }],
    digest: async () => 'checksum',
    emit: (event, payload) => emitted.push({ event, payload }),
    sendRpc: async () => ({ checksum: 'checksum' }),
    nextSeq: () => 1,
    applyRpcTimeoutMs: 1000,
    slotTypeNames: [],
    localSlotMetaManager: {
      extractFromConfig: () => {},
      mergeIntoConfig: (config) => config,
      updateEntry: () => false
    },
    stateSnapshotStore: {
      persist: () => {},
      readStagedConfig: () => restoredConfig
    },
    getManifest: () => ({}),
    getRemoteManifest: () => ({}),
    getSchema: () => ({}),
    getSchemaSource: () => 'test',
    getValidator: () => () => true
  });

  session.syncFromDevice(liveConfig);

  const originalDebug = console.debug;
  try {
    console.debug = () => {};
    expect(session.restoreLocalState()).toBe(true);
    expect(session.getState().staged).toEqual(restoredConfig);
    expect(session.getState().dirty).toBe(true);
    expect(emitted.at(-1)).toMatchObject({
      event: 'config',
      payload: { dirty: true }
    });
  } finally {
    console.debug = originalDebug;
  }
});
