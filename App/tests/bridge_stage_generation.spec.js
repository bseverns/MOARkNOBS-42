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
