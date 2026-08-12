const { strict: assert } = require('node:assert');

const { createDeviceSession } = require('../lib/device/session');
const { createSimulatedMn42Device } = require('../lib/device/simulator');
const { loadSchemaAuthority } = require('../lib/device/schema_authority');
const { MN42_MANIFEST_CONTRACT } = require('../lib/manifest_contract');

function clone(value) {
  return value == null ? value : JSON.parse(JSON.stringify(value));
}

function wait(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function waitFor(predicate, timeoutMs = 1500) {
  const startedAt = Date.now();
  while (Date.now() - startedAt < timeoutMs) {
    if (predicate()) return;
    await wait(10);
  }
  throw new Error('timed out waiting for condition');
}

function createHarness(options = {}) {
  const simulator = createSimulatedMn42Device(options.simulator);
  const structuredEvents = [];
  const writtenLines = [];
  let setAllWriteCount = 0;
  let resolveDelayedApplyWrite = null;
  let session = null;

  function deliverLine(line) {
    try {
      session.handleMessage(JSON.parse(line), {});
    } catch (error) {
      session.handleMalformedMessage(line, error);
    }
  }

  function sendLine(line) {
    const normalized = String(line).trim();
    writtenLines.push(normalized);
    if (normalized.startsWith('SET_ALL ')) {
      setAllWriteCount += 1;
      if (setAllWriteCount === options.failSetAllWriteAt) {
        throw new Error('serial write failed');
      }
    }
    simulator.receiveLine(line);
  }

  session = createDeviceSession({
    sendLine,
    writeApplyLine: options.delayFirstApplyWrite
      ? (line) => {
          setAllWriteCount += 1;
          writtenLines.push(String(line).trim());
          if (setAllWriteCount !== 1) {
            simulator.receiveLine(line);
            return undefined;
          }
          return new Promise((resolve) => {
            resolveDelayedApplyWrite = () => {
              simulator.receiveLine(line);
              resolve();
            };
          });
        }
      : sendLine,
    onStructuredEvent(event) {
      structuredEvents.push(event);
    },
    nextSeq: (() => {
      let seq = 0;
      return () => {
        seq += 1;
        return seq;
      };
    })(),
    loadAuthority: options.loadAuthority,
  });

  simulator.on('line', deliverLine);

  return {
    simulator,
    session,
    structuredEvents,
    writtenLines,
    resolveDelayedApplyWrite: () => resolveDelayedApplyWrite?.(),
  };
}

async function run() {
  {
    const harness = createHarness();
    const state = await harness.session.prewarmAuthority();
    assert.equal(state.schema?.type, 'object');
    assert.equal(state.schemaSource, 'bundled');
    assert.equal(state.manifest, null);
    assert.equal(state.liveConfig, null);
    assert.equal(state.ready, false);
  }

  {
    const harness = createHarness();
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    const state = harness.session.getState();
    assert.equal(
      state.ready,
      true,
      'session should be ready after startup handshake',
    );
    assert.equal(state.helloSeen, true, 'session should cache HELLO state');
    assert.equal(state.manifest?.device_name, 'MOARkNOBS-42');
    assert.equal(
      state.schema?.schema_version,
      MN42_MANIFEST_CONTRACT.schema_version,
    );
    assert.equal(
      state.hardwareHealth?.display_ok,
      true,
      'session should expose manifest-backed display health',
    );
    assert.equal(
      state.hardwareHealth?.eeprom_primary_valid,
      true,
      'session should expose manifest-backed EEPROM health',
    );
    assert.equal(Array.isArray(state.liveConfig?.slots), true);
    assert.equal(Array.isArray(state.stagedConfig?.slots), true);
    assert.equal(state.deviceAuthority, 'verified');
    assert.equal(state.configValidation.status, 'verified');
    assert.equal(state.draftState, 'clean');
    assert.deepEqual(harness.writtenLines.slice(0, 4), [
      'HELLO',
      'GET_MANIFEST',
      'GET_SCHEMA',
      'GET_CONFIG',
    ]);
  }

  {
    const authority = await loadSchemaAuthority();
    const harness = createHarness({
      loadAuthority: async () => ({
        ...authority,
        validateConfig: () => ({
          valid: false,
          errors: [{ instancePath: '/slots', message: 'forced contract failure' }],
        }),
      }),
    });
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().configValidation.status === 'invalid');
    const state = harness.session.getState();
    assert.equal(state.ready, false);
    assert.equal(state.handshakeState, 'degraded');
    assert.equal(state.liveConfig, null, 'an invalid device export must never become live truth');
    assert.equal(
      harness.structuredEvents.some(
        (entry) =>
          entry.event === 'bridge.alert' &&
          entry.payload?.code === 'device_config_schema_invalid',
      ),
      true,
    );
  }

  {
    const harness = createHarness();
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    const initialRevision = harness.session.getState().sessionRevision;
    const firstDraft = clone(harness.session.getState().stagedConfig);
    firstDraft.slots[0].data1 = 71;
    const first = await harness.session.stageConfig(firstDraft, {
      expectedSessionRevision: initialRevision,
    });
    assert.equal(first.sessionRevision > initialRevision, true);
    assert.equal(harness.session.getState().deviceAuthority, 'verified');
    assert.equal(harness.session.getState().draftState, 'dirty');
    const staleDraft = clone(firstDraft);
    staleDraft.slots[1].data1 = 72;
    await assert.rejects(
      () => harness.session.stageConfig(staleDraft, { expectedSessionRevision: initialRevision }),
      (error) => error?.code === 'stale_session_revision',
      'a second browser cannot overwrite a newer staged revision',
    );
    const snapshot = harness.structuredEvents.findLast(
      (entry) => entry.event === 'device.session.snapshot',
    );
    assert.equal(snapshot?.payload?.deviceAuthority, 'verified');
    assert.equal(snapshot?.payload?.draftState, 'dirty');
  }

  {
    const harness = createHarness();
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    const staged = clone(harness.session.getState().stagedConfig);
    staged.slots[0].data1 = 73;
    await harness.session.stageConfig(staged);
    const writesBeforeRollback = [...harness.writtenLines];

    const result = await harness.session.rollback('operator_request');

    assert.equal(result.rolledBack, true);
    assert.deepEqual(
      harness.writtenLines,
      writesBeforeRollback,
      'rolling back an untransmitted draft must not write to the device',
    );
    assert.deepEqual(
      harness.session.getState().stagedConfig,
      harness.session.getState().liveConfig,
    );
    assert.equal(harness.session.getState().dirty, false);
  }

  {
    const lines = [];
    const alerts = [];
    const session = createDeviceSession({
      sendLine: (line) => lines.push(line),
      onStructuredEvent: (event) => alerts.push(event),
      handshakeTimeoutMs: 10,
    });
    await session.handleOpen();
    await wait(45);
    assert.equal(
      lines.filter((line) => line === 'HELLO').length,
      3,
      'handshake retries only the missing HELLO request before timing out',
    );
    assert.equal(session.getState().handshakeState, 'timeout');
    assert.equal(
      alerts.some((entry) => entry.event === 'bridge.alert' && entry.payload?.code === 'handshake_timeout'),
      true,
    );
  }

  {
    const harness = createHarness({
      simulator: { schema: { required: ['slots', 'efSlots', 'filter', 'arg'] } },
    });
    await harness.session.handleOpen();
    await wait(30);
    const state = harness.session.getState();
    assert.equal(state.ready, false, 'incompatible reported schemas must not make the session write-ready');
    assert.equal(state.compatibility.status, 'incompatible');
    const staged = clone(state.stagedConfig);
    staged.slots[0].data1 = 17;
    await harness.session.stageConfig(staged);
    await assert.rejects(
      () => harness.session.applyStagedConfig(),
      (error) => error?.code === 'device_schema_incompatible',
      'Apply must be blocked when device and bundled schema contracts disagree',
    );
  }

  {
    const harness = createHarness();
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    const staged = clone(harness.session.getState().stagedConfig);
    staged.slots[0].midiChannel = 9;
    await harness.session.stageConfig(staged);
    assert.equal(
      harness.session.getState().dirty,
      true,
      'staging should mark the cached config dirty',
    );
    const result = await harness.session.applyStagedConfig();
    assert.equal(result.applied, true, 'good ACK should complete staged apply');
    assert.equal(
      harness.session.getState().liveConfig.slots[0].midiChannel,
      9,
      'verified ACK should promote staged config to live',
    );
    assert.equal(harness.session.getState().dirty, false);
    assert.equal(
      harness.structuredEvents.some(
        (entry) => entry.event === 'device.apply.pending',
      ),
      true,
      'structured event stream should expose the serial writer ownership boundary',
    );
    assert.equal(
      harness.structuredEvents.some(
        (entry) => entry.event === 'device.apply.ack',
      ),
      true,
      'structured event stream should expose apply ACKs',
    );
  }

  {
    const harness = createHarness({ simulator: { ackDelayMs: 30 } });
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    const staged = clone(harness.session.getState().stagedConfig);
    staged.slots[4].data1 = 91;
    await harness.session.stageConfig(staged);
    const applyPromise = harness.session.applyStagedConfig({ timeoutMs: 5000 });
    await wait(5);
    assert.equal(harness.session.getState().deviceAuthority, 'applying');
    assert.equal(harness.session.getState().draftState, 'dirty');
    assert.equal(
      harness.session.getState().dirty,
      true,
      'dirty should remain set until the ACK is observed',
    );
    await applyPromise;
    assert.equal(
      harness.session.getState().dirty,
      false,
      'dirty should clear after the ACK promotes staged config to live',
    );
    assert.equal(harness.session.getState().deviceAuthority, 'verified');
    assert.equal(harness.session.getState().draftState, 'clean');
  }

  {
    const harness = createHarness({ simulator: { ackDelayMs: 30 } });
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    const staged = clone(harness.session.getState().stagedConfig);
    staged.slots[4].data1 = 99;
    await harness.session.stageConfig(staged);
    const applyPromise = harness.session.applyStagedConfig({ timeoutMs: 5000 });
    await wait(1);
    await assert.rejects(
      () => harness.session.rollback('operator_request'),
      (error) => error?.code === 'apply_outcome_unresolved',
      'rollback must reject while a transmitted apply outcome is unresolved',
    );
    await applyPromise;
    assert.equal(
      harness.session.getState().liveConfig.slots[4].data1,
      99,
      'ACK must promote the immutable transmitted staged snapshot',
    );
  }

  {
    const harness = createHarness({ simulator: { ackDelayMs: 30 } });
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    const staged = clone(harness.session.getState().stagedConfig);
    staged.slots[5].data1 = 92;
    await harness.session.stageConfig(staged);
    const applyPromise = harness.session.applyStagedConfig({ timeoutMs: 5000 });
    await assert.rejects(
      () => harness.session.applyStagedConfig({ timeoutMs: 5000 }),
      (error) => error?.code === 'apply_in_progress',
      'overlapping staged applies should fail without replacing the pending ACK',
    );
    await applyPromise;
    assert.equal(
      harness.session.getState().dirty,
      false,
      'first apply should still complete after a rejected overlapping apply',
    );
  }

  {
    const harness = createHarness({ simulator: { ackMode: 'timeout' } });
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    const staged = clone(harness.session.getState().stagedConfig);
    staged.slots[1].data1 = 77;
    await harness.session.stageConfig(staged);
    await assert.rejects(
      () => harness.session.applyStagedConfig({ timeoutMs: 20 }),
      (error) => error?.code === 'apply_timeout',
      'missing ACK should time out the staged apply',
    );
    assert.equal(
      harness.session.getState().dirty,
      true,
      'readback that differs after a timeout must retain the attempted candidate',
    );
    assert.equal(harness.session.getState().stagedConfig.slots[1].data1, 77);
    assert.equal(
      harness.session.getState().lastApplyResult?.status,
      'verified_device_different',
    );
  }

  {
    const harness = createHarness({ delayFirstApplyWrite: true });
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    const staged = clone(harness.session.getState().stagedConfig);
    staged.slots[1].data1 = 78;
    await harness.session.stageConfig(staged);

    await assert.rejects(
      () => harness.session.applyStagedConfig({
        timeoutMs: 20,
        writeTimeoutMs: 5000,
      }),
      (error) => error?.code === 'apply_timeout',
      'the complete Apply timeout should expire a stalled serial writer',
    );
    await waitFor(
      () => harness.session.getState().lastApplyResult?.status === 'verified_device_different',
    );
    assert.equal(
      harness.session.getState().stagedConfig.slots[1].data1,
      78,
      'readback mismatch must preserve the exact attempted candidate as the dirty draft',
    );
    assert.equal(harness.session.getState().dirty, true);
    assert.equal(
      harness.session.isApplyTransactionActive(),
      false,
      'successful readback should release public Apply exclusivity',
    );

    harness.resolveDelayedApplyWrite();
    await wait(20);
    assert.equal(
      harness.writtenLines.filter((line) => line.startsWith('SET_ALL ')).length,
      1,
      'an expired writer must not transmit remaining frames after readback releases exclusivity',
    );
  }

  {
    const harness = createHarness();
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    const staged = clone(harness.session.getState().stagedConfig);
    staged.slots[0].data1 = 101;
    const stagedDigest = require('node:crypto')
      .createHash('sha256')
      .update(JSON.stringify(staged))
      .digest('hex');
    const receipt = await harness.session.stageConfig(staged, {
      clientApplyId: 'client-apply-current',
      stagedRevision: 41,
      stagedDigest,
    });
    assert.equal(receipt.clientApplyId, 'client-apply-current');
    assert.equal(receipt.stagedRevision, 41);
    await assert.rejects(
      () => harness.session.applyStagedConfig({
        clientApplyId: 'client-apply-old',
        stagedRevision: 40,
        stagedDigest,
      }),
      (error) => error?.code === 'staged_apply_identity_mismatch',
      'Apply cannot inherit a receipt from an older staged attempt',
    );
    const apply = harness.session.applyStagedConfig({
      clientApplyId: receipt.clientApplyId,
      stagedRevision: receipt.stagedRevision,
      stagedDigest: receipt.stagedDigest,
    });
    await apply;
    const result = harness.session.getState().lastApplyResult;
    assert.equal(result.clientApplyId, 'client-apply-current');
    assert.equal(result.stagedRevision, 41);
    assert.equal(result.stagedDigest, stagedDigest);
  }

  {
    const harness = createHarness({ simulator: { ackMode: 'bad-ack' } });
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    const staged = clone(harness.session.getState().stagedConfig);
    staged.slots[2].data1 = 55;
    await harness.session.stageConfig(staged);
    await assert.rejects(
      () => harness.session.applyStagedConfig(),
      (error) => error?.code === 'apply_checksum_mismatch',
      'bad ACK checksum should reject the staged apply',
    );
    assert.equal(
      harness.structuredEvents.some(
        (entry) => entry.event === 'device.apply.uncertain',
      ),
      true,
      'bad ACK should enter uncertain state and read device truth back',
    );
  }

  {
    const harness = createHarness({ simulator: { ackMode: 'error' } });
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    const staged = clone(harness.session.getState().stagedConfig);
    staged.slots[2].data1 = 54;
    const stagedDigest = require('node:crypto')
      .createHash('sha256')
      .update(JSON.stringify(staged))
      .digest('hex');
    await harness.session.stageConfig(staged, {
      clientApplyId: 'firmware-rejection',
      stagedRevision: 42,
      stagedDigest,
    });
    await assert.rejects(
      () => harness.session.applyStagedConfig({
        clientApplyId: 'firmware-rejection',
        stagedRevision: 42,
        stagedDigest,
      }),
      (error) => error?.code === 'device_checksum',
    );
    const receipt = harness.session.getState().lastApplyResult;
    assert.equal(receipt.status, 'rollback');
    assert.equal(receipt.clientApplyId, 'firmware-rejection');
    assert.equal(receipt.stagedRevision, 42);
    assert.equal(receipt.stagedDigest, stagedDigest);
  }

  {
    const harness = createHarness({ failSetAllWriteAt: 2 });
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    const staged = clone(harness.session.getState().stagedConfig);
    staged.slots[2].data1 = 56;
    await harness.session.stageConfig(staged);
    await assert.rejects(
      () => harness.session.applyStagedConfig(),
      (error) => error?.code === 'apply_transport_error',
      'partial SET_ALL write should reject as a transport error',
    );
    const abortIndex = harness.writtenLines.indexOf('ABORT_SET_ALL');
    const failedChunkIndex = harness.writtenLines.findLastIndex((line) =>
      line.startsWith('SET_ALL '),
    );
    assert.equal(
      abortIndex > failedChunkIndex,
      true,
      'partial SET_ALL write should attempt to release firmware assembler state before readback',
    );
  }

  {
    const harness = createHarness({ simulator: { ackDelayMs: 100 } });
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    const staged = clone(harness.session.getState().stagedConfig);
    staged.slots[2].data1 = 116;
    await harness.session.stageConfig(staged);
    const applyPromise = harness.session.applyStagedConfig({ timeoutMs: 1000 });
    await waitFor(
      () => harness.session.getState().lastApplyResult?.status === 'pending',
    );
    await assert.rejects(
      () => harness.session.stageConfig(staged),
      (error) => error?.code === 'apply_outcome_unresolved',
      'staging must not replace the draft while Apply owns it',
    );
    await applyPromise;
  }

  {
    const harness = createHarness({ simulator: { ackDelayMs: 30 } });
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    const staged = clone(harness.session.getState().stagedConfig);
    staged.slots[3].data1 = 88;
    await harness.session.stageConfig(staged);
    const applyPromise = harness.session.applyStagedConfig({ timeoutMs: 5000 });
    await wait(1);
    harness.simulator.disconnect();
    harness.session.handleDisconnect('serial_close');
    await assert.rejects(
      () => applyPromise,
      (error) => error?.code === 'apply_interrupted',
      'disconnect during apply should reject and roll back',
    );
    harness.simulator.connect();
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    assert.equal(
      harness.session.getState().ready,
      true,
      'session should recover after reconnecting from a failed apply',
    );
  }

  {
    const harness = createHarness();
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    harness.simulator.emitMalformed('{"oops"');
    await waitFor(() =>
      harness.structuredEvents.some(
        (entry) =>
          entry.event === 'bridge.alert' &&
          entry.payload?.code === 'malformed_device_response',
      ),
    );
  }

  console.log(
    'device session caches startup state and enforces staged apply discipline',
  );
}

run().catch((error) => {
  console.error(error);
  process.exit(1);
});
