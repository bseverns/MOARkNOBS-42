const { strict: assert } = require('node:assert');

const { createDeviceSession } = require('../lib/device/session');
const { createSimulatedMn42Device } = require('../lib/device/simulator');

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
  let session = null;

  function deliverLine(line) {
    try {
      session.handleMessage(JSON.parse(line), {});
    } catch (error) {
      session.handleMalformedMessage(line, error);
    }
  }

  session = createDeviceSession({
    sendLine(line) {
      writtenLines.push(String(line).trim());
      simulator.receiveLine(line);
    },
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
  });

  simulator.on('line', deliverLine);

  return {
    simulator,
    session,
    structuredEvents,
    writtenLines,
  };
}

async function run() {
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
    assert.equal(state.schema?.schema_version, 6);
    assert.equal(Array.isArray(state.liveConfig?.slots), true);
    assert.equal(Array.isArray(state.stagedConfig?.slots), true);
    assert.deepEqual(harness.writtenLines.slice(0, 4), [
      'HELLO',
      'GET_MANIFEST',
      'GET_SCHEMA',
      'GET_CONFIG',
    ]);
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
    const applyPromise = harness.session.applyStagedConfig({ timeoutMs: 500 });
    await wait(5);
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
      false,
      'timeout rollback should clear the dirty flag',
    );
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
        (entry) => entry.event === 'device.apply.rollback',
      ),
      true,
      'bad ACK should emit a rollback event',
    );
  }

  {
    const harness = createHarness({ simulator: { ackDelayMs: 30 } });
    await harness.session.handleOpen();
    await waitFor(() => harness.session.getState().ready);
    const staged = clone(harness.session.getState().stagedConfig);
    staged.slots[3].data1 = 88;
    await harness.session.stageConfig(staged);
    const applyPromise = harness.session.applyStagedConfig({ timeoutMs: 500 });
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
