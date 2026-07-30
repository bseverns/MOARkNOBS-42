import { test, expect } from '@playwright/test';
import { createBridgeSessionRuntime } from '../runtime/bridge_session_runtime.js';

const clone = (value) => JSON.parse(JSON.stringify(value));

test('Bridge staging submits the newest draft when an edit arrives during an in-flight stage', async () => {
  let staged = { slots: [{ value: 1 }] };
  const submissions = [];
  const resolvers = [];
  const syncedSessions = [];
  const client = {
    stageConfig(config) {
      submissions.push(clone(config));
      return new Promise((resolve) => resolvers.push(resolve));
    }
  };
  const configSession = {
    getStagedConfig: () => staged,
    syncFromSession(session) { syncedSessions.push(clone(session)); },
    broadcastConfig() {}
  };
  const runtime = createBridgeSessionRuntime({
    baseUrl: 'http://bridge.test',
    clone,
    emit() {},
    createClient: () => client,
    compileSchema() {},
    configSession,
    localManifest: {},
    currentSlotCount: () => 1,
    localSlotMetaManager: { ensureCount() {} },
    getConnectedPayload: () => ({}),
    setRemoteManifest() {},
    setSchema() {},
    setSchemaSource() {},
    onTelemetry() {}
  });

  runtime.scheduleStageSync({ active: true });
  const flush = runtime.flushStageSync({ active: true });
  expect(submissions).toEqual([{ slots: [{ value: 1 }] }]);

  staged = { slots: [{ value: 2 }] };
  runtime.scheduleStageSync({ active: true });
  runtime.applyAuthoritativeSession({
    liveConfig: { slots: [{ value: 0 }] },
    stagedConfig: { slots: [{ value: 1 }] },
    dirty: true
  });
  expect(syncedSessions.at(-1).stagedConfig).toEqual({ slots: [{ value: 2 }] });
  resolvers.shift()({ sessionRevision: 1 });
  await expect.poll(() => submissions.length).toBe(2);
  expect(submissions[1]).toEqual({ slots: [{ value: 2 }] });
  resolvers.shift()({ sessionRevision: 2 });

  await expect(flush).resolves.toEqual({ sessionRevision: 2 });
});

test('Bridge reconnect preserves an unsent local draft over the first remote snapshot', async () => {
  let staged = { slots: [{ value: 2 }] };
  const submissions = [];
  const syncedSessions = [];
  const client = {
    closeEvents() {},
    stageConfig(config) {
      submissions.push(clone(config));
      return Promise.resolve({ sessionRevision: 3 });
    }
  };
  const configSession = {
    getStagedConfig: () => staged,
    syncFromSession(session) {
      syncedSessions.push(clone(session));
      staged = clone(session.stagedConfig);
    },
    broadcastConfig() {}
  };
  const runtime = createBridgeSessionRuntime({
    baseUrl: 'http://bridge.test',
    clone,
    emit() {},
    createClient: () => client,
    compileSchema() {},
    configSession,
    localManifest: {},
    currentSlotCount: () => 1,
    localSlotMetaManager: { ensureCount() {} },
    getConnectedPayload: () => ({}),
    setRemoteManifest() {},
    setSchema() {},
    setSchemaSource() {},
    onTelemetry() {}
  });

  runtime.scheduleStageSync({ active: true });
  runtime.reset({ preserveLocalDraft: true });
  runtime.applyAuthoritativeSession({
    liveConfig: { slots: [{ value: 0 }] },
    stagedConfig: { slots: [{ value: 1 }] },
    dirty: true,
    sessionRevision: 2
  });

  expect(syncedSessions.at(-1).liveConfig).toEqual({ slots: [{ value: 0 }] });
  expect(syncedSessions.at(-1).stagedConfig).toEqual({ slots: [{ value: 2 }] });
  await expect(runtime.flushStageSync({ active: true })).resolves.toEqual({
    sessionRevision: 3
  });
  expect(submissions).toEqual([{ slots: [{ value: 2 }] }]);
});

test('Apply suspension queues newer edits until the identity-stage handoff finishes', async () => {
  let staged = { slots: [{ value: 1 }] };
  const submissions = [];
  const client = {
    stageConfig(config) {
      submissions.push(clone(config));
      return Promise.resolve({ sessionRevision: submissions.length });
    }
  };
  const runtime = createBridgeSessionRuntime({
    baseUrl: 'http://bridge.test',
    clone,
    emit() {},
    createClient: () => client,
    compileSchema() {},
    configSession: {
      getStagedConfig: () => staged,
      syncFromSession() {},
      broadcastConfig() {}
    },
    localManifest: {},
    currentSlotCount: () => 1,
    localSlotMetaManager: { ensureCount() {} },
    getConnectedPayload: () => ({}),
    setRemoteManifest() {},
    setSchema() {},
    setSchemaSource() {},
    onTelemetry() {}
  });

  runtime.scheduleStageSync({ active: true });
  await runtime.flushStageSync({ active: true });
  expect(submissions).toEqual([{ slots: [{ value: 1 }] }]);

  runtime.suspendStageSync();
  staged = { slots: [{ value: 2 }] };
  runtime.scheduleStageSync({ active: true });
  await expect(runtime.flushStageSync({ active: true })).resolves.toBeNull();
  expect(submissions).toHaveLength(1);

  runtime.resumeStageSync({ active: true });
  await expect.poll(() => submissions.length).toBe(2);
  expect(submissions[1]).toEqual({ slots: [{ value: 2 }] });
});

for (const event of [
  'device.apply.ack',
  'device.apply.rollback',
  'device.apply.resynchronized',
  'device.apply.device_different'
]) {
  for (const ordering of ['rejection-before-event', 'event-before-rejection']) {
    test(`${event} retries an unresolved stage with ${ordering}`, async () => {
      let eventHandler;
      let stageCalls = 0;
      let rejectFirstStage;
      const staged = { slots: [{ value: 2 }] };
      const client = {
        stageConfig() {
          stageCalls += 1;
          if (stageCalls === 1) {
            return new Promise((_, reject) => {
              rejectFirstStage = () => reject(
                Object.assign(new Error('Apply outcome unresolved'), {
                  code: 'apply_outcome_unresolved'
                })
              );
            });
          }
          return Promise.resolve({ sessionRevision: 4 });
        },
        openEvents({ onEvent }) {
          eventHandler = onEvent;
          return Promise.resolve();
        },
        closeEvents() {}
      };
      const runtime = createBridgeSessionRuntime({
        baseUrl: 'http://bridge.test',
        eventUrl: 'ws://bridge.test/events',
        clone,
        emit() {},
        createClient: () => client,
        compileSchema() {},
        configSession: {
          getStagedConfig: () => staged,
          syncFromSession() {},
          broadcastConfig() {}
        },
        localManifest: {},
        currentSlotCount: () => 1,
        localSlotMetaManager: { ensureCount() {} },
        getConnectedPayload: () => ({}),
        setRemoteManifest() {},
        setSchema() {},
        setSchemaSource() {},
        onTelemetry() {}
      });
      runtime.applyAuthoritativeSession({
        liveConfig: { slots: [{ value: 1 }] },
        stagedConfig: { slots: [{ value: 1 }] },
        dirty: false,
        sessionRevision: 2
      });
      await runtime.openStructuredEvents();
      runtime.scheduleStageSync({ active: true });
      const firstFlush = runtime.flushStageSync({ active: true });
      const firstRejection = expect(firstFlush).rejects.toThrow(
        /outcome unresolved/i
      );

      if (ordering === 'event-before-rejection') {
        eventHandler({ event, payload: { sessionRevision: 3 } });
      }
      rejectFirstStage();
      await firstRejection;
      if (ordering === 'rejection-before-event') {
        eventHandler({ event, payload: { sessionRevision: 3 } });
      }
      await expect.poll(() => stageCalls).toBe(2);
    });
  }
}
