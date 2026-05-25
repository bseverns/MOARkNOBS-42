const { strict: assert } = require('node:assert');

const { createSerialLineHandler } = require('../lib/transports/serial_ingress');

function createHarness() {
  const state = {
    ready: false,
    serialConnected: false,
    lastError: 'stale',
  };
  const lines = [];
  const counters = {};
  const malformed = [];
  const handled = [];

  return {
    state,
    lines,
    counters,
    malformed,
    handled,
    handler: createSerialLineHandler({
      maxSerialLineLen: 4096,
      setState(patch) {
        Object.assign(state, patch);
      },
      broadcastLine(line) {
        lines.push(line);
      },
      bumpCounter(name) {
        counters[name] = (counters[name] || 0) + 1;
      },
      pushLog() {},
      inspectManifest() {},
      deviceSession: {
        handleMessage(data) {
          handled.push(data);
        },
        handleMalformedMessage(line, error) {
          malformed.push({ line, error });
        },
      },
      extractTimestampMs() {
        return null;
      },
      extractTraceId() {
        return null;
      },
      nextTraceId() {
        return 'serial-test';
      },
      matchPendingRoundTrips() {},
      sendOscTelemetry() {},
      sendMidiTelemetry() {},
      now() {
        return 1234;
      },
    }),
  };
}

function run() {
  {
    const harness = createHarness();
    harness.handler('{"hello":"mn42"}');
    assert.equal(
      harness.state.serialConnected,
      true,
      'raw HELLO should mark the serial lane connected',
    );
    assert.equal(
      harness.state.lastError,
      null,
      'raw HELLO should clear stale serial errors',
    );
    assert.equal(
      harness.state.ready,
      false,
      'raw HELLO alone must not promote the bridge to ready',
    );
  }

  {
    const harness = createHarness();
    harness.handler('{"hello":"mn42","type":"status"}');
    assert.equal(
      harness.handled.length,
      1,
      'valid JSON frames should still reach the device session',
    );
  }

  console.log(
    'serial ingress keeps HELLO-seen separate from full session-ready state',
  );
}

run();
