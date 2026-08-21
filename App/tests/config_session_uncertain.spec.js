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

test('a verified Apply can stage its previous confirmed state without writing immediately', async () => {
  const events = [];
  const session = createSession(async () => ({ checksum: 'candidate-checksum' }), events);
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

  await expect(session.apply()).resolves.toMatchObject({ applied: true });
  expect(session.getState()).toMatchObject({
    dirty: false,
    canStagePreviousApply: true,
    live: { filter: { freq: 321 } }
  });

  expect(session.stagePreviousApply()).toBe(true);
  expect(session.getState()).toMatchObject({
    dirty: true,
    canStagePreviousApply: false,
    live: { filter: { freq: 321 } },
    staged: { filter: { freq: 100 } }
  });
  expect(events).toContainEqual(expect.objectContaining({ type: 'previous-apply-staged' }));
});

test('a later device patch invalidates the pre-Apply return snapshot', async () => {
  const events = [];
  const session = createSession(async () => ({ checksum: 'candidate-checksum' }), events);
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));
  await session.apply();
  expect(session.getState().canStagePreviousApply).toBe(true);

  const deviceState = { ...baseConfig(), filter: { freq: 444 } };
  session.reconcileDevicePatch(deviceState, deviceState);

  expect(session.getState().canStagePreviousApply).toBe(false);
  expect(session.stagePreviousApply()).toBe(false);
});

test('pre-Apply return never overwrites a newer unsent draft', async () => {
  const events = [];
  const session = createSession(async () => ({ checksum: 'candidate-checksum' }), events);
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));
  await session.apply();

  session.stage((draft) => ({ ...draft, filter: { freq: 654 } }));

  expect(session.getState().canStagePreviousApply).toBe(false);
  expect(session.stagePreviousApply()).toBe(false);
  expect(session.getState().staged.filter.freq).toBe(654);
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
    remoteManifest: {
      capabilities: {
        verified_apply: true,
        apply_integrity_receipt: true,
        authoritative_readback: true
      }
    }
  });
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

  await expect(session.apply()).rejects.toThrow(/readback differs/i);
  expect(session.getState().transactionState).toBe('verified-device-different');
  expect(session.getState().live.filter.freq).toBe(100);
  expect(session.getState().staged.filter.freq).toBe(321);
});

test('explicit Apply capability values override the legacy LittleFS inference', async () => {
  const events = [];
  let calls = 0;
  const session = createSession(async () => {
    calls += 1;
    return { checksum: 'candidate-checksum' };
  }, events, {
    remoteManifest: {
      persistence: { backend: 'littlefs' },
      capabilities: {
        verified_apply: false,
        apply_integrity_receipt: false,
        authoritative_readback: false
      }
    }
  });
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

  await expect(session.apply()).resolves.toMatchObject({ applied: true });
  expect(calls).toBe(1);
  expect(session.getState().live.filter.freq).toBe(321);
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

test('explicit Bridge authority is preferred over a stale apply receipt projection', () => {
  const events = [];
  const session = createSession(async () => ({}), events);
  session.syncFromDevice(baseConfig());

  session.syncFromSession({
    liveConfig: baseConfig(),
    stagedConfig: { ...baseConfig(), filter: { freq: 321 } },
    dirty: true,
    deviceAuthority: 'uncertain',
    draftState: 'dirty',
    // This can occur when individual structured events arrive around a
    // snapshot; the explicit dimensions remain the session source of truth.
    lastApplyResult: { status: 'ack', seq: 17 }
  });

  const state = session.getState();
  expect(state.deviceAuthority).toBe('uncertain');
  expect(state.draftState).toBe('dirty');
  expect(state.transactionState).toBe('uncertain');
});

test('a device patch cannot erase unresolved Apply authority', () => {
  const events = [];
  const session = createSession(async () => ({}), events);
  session.syncFromDevice(baseConfig());
  session.syncFromSession({
    liveConfig: baseConfig(),
    stagedConfig: { ...baseConfig(), filter: { freq: 321 } },
    dirty: true,
    deviceAuthority: 'uncertain',
    draftState: 'dirty',
    lastApplyResult: { status: 'uncertain', seq: 17 }
  });

  session.reconcileDevicePatch(
    { ...baseConfig(), filter: { freq: 200 } },
    { ...baseConfig(), filter: { freq: 654 } }
  );

  expect(session.getState().deviceAuthority).toBe('uncertain');
  expect(session.getState().transactionState).toBe('uncertain');
  expect(session.getState().draftState).toBe('dirty');
  expect(session.getState().live.filter.freq).toBe(200);
  expect(session.getState().staged.filter.freq).toBe(654);
});

test('an old Bridge success receipt cannot resolve a newer failed Apply', async () => {
  const events = [];
  const oldReceipt = {
    status: 'ack',
    clientApplyId: 'old-attempt',
    stagedRevision: 1,
    stagedDigest: 'old-digest'
  };
  const session = createSession(async () => ({}), events, {
    isBridgeSessionActive: () => true,
    applyBridgeConfig: async () => {
      throw new Error('Bridge request failed');
    },
    refreshBridgeSession: async () => ({
      liveConfig: baseConfig(),
      stagedConfig: baseConfig(),
      dirty: false,
      deviceAuthority: 'verified',
      draftState: 'clean',
      lastApplyResult: oldReceipt
    })
  });
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

  await expect(session.apply()).rejects.toThrow(/Bridge request failed/);
  expect(session.getState().deviceAuthority).toBe('uncertain');
  expect(session.getState().draftState).toBe('dirty');
  expect(session.getState().staged.filter.freq).toBe(321);
});

test('a correlated refreshed Bridge receipt can resolve its failed HTTP request', async () => {
  const events = [];
  let capturedIdentity;
  const session = createSession(async () => ({}), events, {
    isBridgeSessionActive: () => true,
    applyBridgeConfig: async ({ identity }) => {
      capturedIdentity = identity;
      throw new Error('Bridge response was lost');
    },
    refreshBridgeSession: async () => ({
      liveConfig: { ...baseConfig(), filter: { freq: 321 } },
      stagedConfig: { ...baseConfig(), filter: { freq: 321 } },
      dirty: false,
      deviceAuthority: 'verified',
      draftState: 'clean',
      lastApplyResult: {
        status: 'ack',
        ...capturedIdentity
      }
    })
  });
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

  await expect(session.apply()).rejects.toThrow(/response was lost/);
  expect(session.getState().deviceAuthority).toBe('verified');
  expect(session.getState().dirty).toBe(false);
  expect(session.getState().live.filter.freq).toBe(321);
});

test('a Bridge preflight rejection keeps verified authority and the local draft', async () => {
  const events = [];
  let refreshCalls = 0;
  const error = Object.assign(new Error('stale staged revision'), {
    code: 'stale_session_revision',
    bridgeFailureClass: 'preflight-rejected'
  });
  const session = createSession(async () => ({}), events, {
    isBridgeSessionActive: () => true,
    applyBridgeConfig: async () => {
      throw error;
    },
    refreshBridgeSession: async () => {
      refreshCalls += 1;
      return null;
    }
  });
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

  await expect(session.apply()).rejects.toThrow(/stale staged revision/);
  expect(refreshCalls).toBe(0);
  expect(session.getState().deviceAuthority).toBe('verified');
  expect(session.getState().draftState).toBe('dirty');
  expect(session.getState().transactionState).toBe('dirty');
  expect(session.getState().staged.filter.freq).toBe(321);
  await expect(session.apply()).rejects.toThrow(/stale staged revision/);
});

test('a normalized-clean Bridge Apply releases preflighting authority', async () => {
  const events = [];
  const session = createSession(async () => ({}), events, {
    isBridgeSessionActive: () => true,
    applyBridgeConfig: async () => ({
      applied: false,
      reason: 'clean'
    })
  });
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

  await expect(session.apply()).resolves.toEqual({
    applied: false,
    reason: 'clean'
  });

  const state = session.getState();
  expect(state.deviceAuthority).toBe('verified');
  expect(state.transactionState).not.toBe('preflighting');
  expect(state.transactionState).toBe('dirty');
  expect(state.staged.filter.freq).toBe(321);
});

test('a malformed successful Bridge response cannot prove a no-op', async () => {
  const events = [];
  const session = createSession(async () => ({}), events, {
    isBridgeSessionActive: () => true,
    applyBridgeConfig: async () => ({})
  });
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

  await expect(session.apply()).rejects.toMatchObject({
    code: 'invalid_bridge_apply_response',
    bridgeFailureClass: 'transmission-unknown'
  });

  const state = session.getState();
  expect(state.deviceAuthority).toBe('uncertain');
  expect(state.transactionState).toBe('uncertain');
  expect(state.dirty).toBe(true);
  expect(state.staged.filter.freq).toBe(321);
});

for (const ordering of ['snapshot-before-rejection', 'rejection-before-snapshot']) {
  test(`a stale-revision preflight rejection preserves candidate A with ${ordering}`, async () => {
    const events = [];
    let session;
    const remoteSnapshotC = {
      liveConfig: baseConfig(),
      stagedConfig: { ...baseConfig(), filter: { freq: 777 } },
      dirty: true,
      deviceAuthority: 'verified',
      draftState: 'dirty',
      sessionRevision: 12
    };
    const error = Object.assign(new Error('stale staged revision'), {
      code: 'stale_session_revision',
      bridgeFailureClass: 'preflight-rejected'
    });
    session = createSession(async () => ({}), events, {
      isBridgeSessionActive: () => true,
      applyBridgeConfig: async () => {
        if (ordering === 'snapshot-before-rejection') {
          session.syncFromSession(remoteSnapshotC);
        }
        throw error;
      }
    });
    session.syncFromDevice(baseConfig());
    session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

    await expect(session.apply()).rejects.toThrow(/stale staged revision/);
    if (ordering === 'rejection-before-snapshot') {
      session.syncFromSession(remoteSnapshotC);
    }

    const state = session.getState();
    expect(state.live.filter.freq).toBe(100);
    expect(state.staged.filter.freq).toBe(321);
    expect(state.dirty).toBe(true);
    expect(state.deviceAuthority).toBe('verified');
    expect(state.draftState).toBe('dirty');
  });
}

test('a correlated firmware rejection restores verified truth and retains the candidate', async () => {
  const events = [];
  let capturedIdentity;
  const error = Object.assign(new Error('firmware rejected config'), {
    code: 'device_checksum',
    bridgeFailureClass: 'device-rejected-before-commit'
  });
  const session = createSession(async () => ({}), events, {
    isBridgeSessionActive: () => true,
    applyBridgeConfig: async ({ identity }) => {
      capturedIdentity = identity;
      error.bridgeSession = {
        liveConfig: baseConfig(),
        stagedConfig: baseConfig(),
        dirty: false,
        deviceAuthority: 'verified',
        draftState: 'clean',
        lastApplyResult: {
          status: 'rollback',
          reason: 'device_error',
          ...capturedIdentity
        }
      };
      throw error;
    },
    refreshBridgeSession: async () => {
      throw new Error('refresh unavailable');
    }
  });
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

  await expect(session.apply()).rejects.toThrow(/firmware rejected config/);
  expect(session.getState().deviceAuthority).toBe('verified');
  expect(session.getState().draftState).toBe('dirty');
  expect(session.getState().transactionState).toBe('dirty');
  expect(session.getState().live.filter.freq).toBe(100);
  expect(session.getState().staged.filter.freq).toBe(321);
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
    applyBridgeConfig: () => {
      session.syncFromSession({
        liveConfig: baseConfig(),
        stagedConfig: { ...baseConfig(), filter: { freq: 321 } },
        dirty: true,
        deviceAuthority: 'applying',
        draftState: 'dirty',
        lastApplyResult: { status: 'pending' }
      });
      return bridgeApply;
    }
  });
  session.syncFromDevice(baseConfig());
  session.stage((draft) => ({ ...draft, filter: { freq: 321 } }));

  const applyPromise = session.apply();
  expect(session.getState().transactionState).toBe('preflighting');
  await expect.poll(() => session.getState().transactionState).toBe('applying');
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
