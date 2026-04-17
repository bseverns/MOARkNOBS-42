const { strict: assert } = require('node:assert');
const { EventEmitter } = require('node:events');

const {
  createBridgeService,
  PENDING_COMMAND_TTL_MS,
} = require('../lib/bridge_service');

class FakeReadlineParser extends EventEmitter {}

class FakeSerialPort extends EventEmitter {
  constructor(options = {}) {
    super();
    this.options = options;
    this.writes = [];
    this.parser = null;
    FakeSerialPort.instances.push(this);
    setImmediate(() => this.emit('open'));
  }

  pipe(parser) {
    this.parser = parser;
    return parser;
  }

  write(data) {
    this.writes.push(String(data));
  }

  close(cb) {
    if (typeof cb === 'function') cb();
    this.emit('close');
  }
}
FakeSerialPort.instances = [];

class FakeUdpPort extends EventEmitter {
  constructor(options = {}) {
    super();
    this.options = options;
    this.sent = [];
    FakeUdpPort.instances.push(this);
  }

  open() {
    setImmediate(() => this.emit('ready'));
  }

  send(packet, host, port) {
    this.sent.push({ packet, host, port });
  }

  close() {}
}
FakeUdpPort.instances = [];

function createFakeJzzFactory() {
  const context = {
    midiIn: null,
  };

  function jzzFactory() {
    const midiOut = new EventEmitter();
    midiOut.or = function or() {
      return this;
    };
    midiOut.send = function send() {};
    midiOut.close = function close() {};

    const midiIn = new EventEmitter();
    midiIn.handler = null;
    midiIn.or = function or() {
      return this;
    };
    midiIn.connect = function connect(handler) {
      this.handler = handler;
      return this;
    };
    midiIn.close = function close() {};

    context.midiIn = midiIn;
    return {
      openMidiOut: () => midiOut,
      openMidiIn: () => midiIn,
    };
  }

  return { jzzFactory, context };
}

function makeService() {
  FakeSerialPort.instances = [];
  FakeUdpPort.instances = [];
  const { jzzFactory, context } = createFakeJzzFactory();
  const service = createBridgeService(
    {
      serialName: '/dev/fake',
      oscPort: 9150,
      oscListen: 9151,
      oscHost: '127.0.0.1',
      oscBind: '127.0.0.1',
      midiLabel: 'MN42 RT Metrics',
    },
    {
      serialport: {
        SerialPort: FakeSerialPort,
        ReadlineParser: FakeReadlineParser,
      },
      osc: { UDPPort: FakeUdpPort },
      jzz: jzzFactory,
    },
  );
  return { service, context };
}

function wait(ms = 20) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function run() {
  const originalNow = Date.now;
  let fakeNow = 1000;
  Date.now = () => fakeNow;

  try {
    const { service, context } = makeService();
    await service.start();
    await wait();

    const serial = FakeSerialPort.instances[0];
    const udp = FakeUdpPort.instances[0];
    assert.ok(serial && serial.parser, 'serial parser should be connected');
    assert.ok(udp, 'udp input should be connected');

    fakeNow = 1000;
    let state = service.getState();
    assert.equal(
      state.performance.health.status,
      'no_data',
      'health should start in no_data state before any round-trip samples',
    );
    assert.equal(state.alerts.active.length, 0);

    fakeNow = 1000;
    udp.emit('message', {
      address: '/mn42/cmd',
      args: [
        {
          cmd: 'SET_SLOT_VALUE',
          slot: 2,
          value: 55,
          traceId: 'cmd-trace-1',
        },
      ],
    });
    await wait();
    state = service.getState();
    assert.equal(state.performance.roundTrip.pending, 1);

    fakeNow = 1016;
    serial.parser.emit(
      'data',
      JSON.stringify({
        type: 'telemetry',
        traceId: 'cmd-trace-1',
        slots: [0, 0, 55, 0, 0],
      }),
    );
    await wait();

    state = service.getState();
    assert.equal(state.performance.roundTrip.sampleCount, 1);
    assert.equal(state.performance.roundTrip.lastMs, 16);
    assert.equal(state.performance.roundTrip.p95Ms, 16);
    assert.equal(state.performance.roundTrip.pending, 0);
    assert.equal(state.performance.counters.completed, 1);
    assert.equal(state.performance.counters.matchedByTrace, 1);
    assert.equal(
      state.performance.health.status,
      'warn',
      'default p95/jitter targets should flag slow round-trip samples',
    );
    assert.equal(
      state.alerts.active.some((alert) => alert.code === 'performance_warn'),
      true,
      'warn health state should raise a performance alert',
    );
    assert.equal(
      state.alerts.recent.filter((alert) => alert.code === 'performance_warn')
        .length,
      1,
      'warn health should not spam duplicate performance alerts inside suppression window',
    );

    fakeNow = 2000;
    context.midiIn.handler({ toArray: () => [0xb0, 4, 77] });
    await wait();
    state = service.getState();
    assert.equal(state.performance.roundTrip.pending, 1);

    fakeNow = 2038;
    serial.parser.emit(
      'data',
      JSON.stringify({
        type: 'telemetry',
        slots: [0, 0, 55, 0, 77],
      }),
    );
    await wait();
    state = service.getState();
    assert.equal(state.performance.roundTrip.sampleCount, 2);
    assert.equal(state.performance.roundTrip.minMs, 16);
    assert.equal(state.performance.roundTrip.maxMs, 38);
    assert.equal(state.performance.roundTrip.meanMs, 27);
    assert.equal(state.performance.roundTrip.p50Ms, 16);
    assert.equal(state.performance.roundTrip.p95Ms, 38);
    assert.equal(state.performance.roundTrip.jitterP95Ms, 22);
    assert.equal(state.performance.counters.matchedBySlotValue, 1);
    assert.equal(
      state.alerts.recent.filter((alert) => alert.code === 'performance_warn')
        .length,
      1,
      'repeated warn samples inside suppression window should stay deduplicated',
    );

    const roundTripRoutes = state.routes.filter(
      (route) => route.flow === 'roundtrip' && route.kind === 'latency_sample',
    );
    assert.equal(roundTripRoutes.length, 2);
    assert.deepEqual(
      roundTripRoutes.map((route) => route.latencyMs),
      [16, 38],
      'roundtrip route events should preserve measured latency samples',
    );

    fakeNow = 3000;
    udp.emit('message', {
      address: '/mn42/cmd',
      args: [{ cmd: 'SET_SLOT_VALUE', slot: 8, value: 99 }],
    });
    await wait();
    state = service.getState();
    assert.equal(state.performance.roundTrip.pending, 1);

    fakeNow = 3000 + PENDING_COMMAND_TTL_MS + 1;
    serial.parser.emit(
      'data',
      JSON.stringify({
        type: 'telemetry',
        slots: [0, 0, 55, 0, 77],
      }),
    );
    await wait();
    state = service.getState();
    assert.equal(state.performance.roundTrip.pending, 0);
    assert.equal(state.performance.counters.expired, 1);

    await service.configure(
      {
        rtP95TargetMs: 100,
        rtJitterP95TargetMs: 50,
      },
      { restart: false },
    );
    state = service.getState();
    assert.equal(
      state.performance.health.status,
      'ok',
      'raising thresholds should clear warn status when samples are under target',
    );
    assert.equal(
      state.alerts.active.some((alert) => alert.code === 'performance_warn'),
      false,
      'ok health state should clear performance alerts',
    );
    await service.clearAlerts();
    state = service.getState();
    assert.equal(state.alerts.active.length, 0);

    await service.resetPerformance();
    state = service.getState();
    assert.equal(state.performance.roundTrip.sampleCount, 0);
    assert.equal(state.performance.roundTrip.pending, 0);
    assert.equal(state.performance.health.status, 'no_data');
    assert.equal(state.performance.counters.completed, 0);
    assert.equal(
      state.alerts.active.some((alert) => alert.code === 'performance_warn'),
      false,
      'reset should keep performance alerts cleared',
    );

    await service.stop();
    console.log('roundtrip latency metrics track p50/p95, jitter, and expiry');
  } finally {
    Date.now = originalNow;
  }
}

run().catch((err) => {
  console.error(err);
  process.exit(1);
});
