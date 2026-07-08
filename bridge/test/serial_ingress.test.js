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
  const osc = [];
  const midi = [];

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
      sendOscTelemetry(address, args, routeMeta) {
        osc.push({ address, args, routeMeta });
      },
      sendMidiTelemetry(channelBase, values, routeMeta) {
        midi.push({ channelBase, values, routeMeta });
      },
      now() {
        return 1234;
      },
    }),
    osc,
    midi,
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

  {
    const harness = createHarness();
    harness.handler(
      JSON.stringify({
        type: 'telemetry',
        scope: 'state_slots',
        timestampMs: 42,
        traceId: 'fw-42-1',
        slots: [1, 2, 3],
        currentSlot: 2,
      }),
    );
    assert.equal(
      harness.osc.some((entry) => entry.address === '/mn42/slots'),
      true,
      'state_slots should route slot telemetry to OSC',
    );
    assert.equal(
      harness.osc.some((entry) => entry.address === '/mn42/current-slot'),
      true,
      'state_slots should route active slot to OSC',
    );
    assert.equal(
      harness.midi.some((entry) => entry.channelBase === 0xb0),
      true,
      'state_slots should route slot telemetry to MIDI CCs',
    );
  }

  {
    const harness = createHarness();
    harness.handler(
      JSON.stringify({
        type: 'telemetry',
        scope: 'state_envelopes',
        envelopes: [4, 5, 6],
        lfos: [0.25, 0.75],
        efStatus: [1, 0, 1],
        lfo_config: [{ index: 0, shape_name: 'sine' }],
      }),
    );
    for (const address of [
      '/mn42/envelopes',
      '/mn42/lfos',
      '/mn42/ef/status',
      '/mn42/lfo/config',
    ]) {
      assert.equal(
        harness.osc.some((entry) => entry.address === address),
        true,
        `state_envelopes should route ${address}`,
      );
    }
  }

  {
    const harness = createHarness();
    harness.handler(
      JSON.stringify({
        type: 'telemetry',
        scope: 'state_diagnostics',
        argMethod: 'PLUS',
        argEnabled: true,
        argPair: [0, 1],
        active_profile: 2,
        diagnostics: { display_ok: false, loop_overruns: 3 },
        clock: { source: 'external', running: true },
        note_dynamics: { velocity_shift: 2 },
        jitter: { depth: 0.5 },
      }),
    );
    for (const address of [
      '/mn42/diagnostics',
      '/mn42/clock',
      '/mn42/arg/pair',
      '/mn42/arg/enabled',
      '/mn42/arg/method',
      '/mn42/profile/active',
      '/mn42/note-dynamics',
      '/mn42/jitter',
    ]) {
      assert.equal(
        harness.osc.some((entry) => entry.address === address),
        true,
        `state_diagnostics should route ${address}`,
      );
    }
  }

  {
    const harness = createHarness();
    harness.handler(
      JSON.stringify({
        type: 'telemetry',
        scope: 'state_args_0_13',
        slotArgs: [{ index: 0, enabled: true, method: 0 }],
      }),
    );
    assert.equal(
      harness.osc.some((entry) => entry.address === '/mn42/arg/slots'),
      true,
      'state_args chunks should route ARG slot config to OSC as JSON',
    );
  }

  console.log(
    'serial ingress keeps session readiness separate and routes rich firmware telemetry',
  );
}

run();
