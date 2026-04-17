const { strict: assert } = require('node:assert');
const { EventEmitter } = require('node:events');

const {
  createBridgeService,
  ROUTE_LOG_LIMIT,
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
    midiOut: null,
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
    context.midiOut = midiOut;
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
      oscPort: 9140,
      oscListen: 9141,
      oscHost: '127.0.0.1',
      oscBind: '127.0.0.1',
      midiLabel: 'MN42 Trace Test',
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
  const { service, context } = makeService();
  await service.start();
  await wait();

  const serial = FakeSerialPort.instances[0];
  const udp = FakeUdpPort.instances[0];
  assert.ok(serial && serial.parser, 'serial parser should be connected');
  assert.ok(udp, 'udp port should be created');

  serial.parser.emit(
    'data',
    JSON.stringify({
      type: 'telemetry',
      timestamp: 1710000000123,
      traceId: 'fw-trace-1',
      slots: [10, 64],
      envelopes: [127, 63],
    }),
  );
  await wait();

  let state = service.getState();
  assert.equal(state.timing.lastSerialSourceTimestampMs, 1710000000123);
  assert.ok(
    Number.isFinite(state.timing.lastSerialHostTimestampMs),
    'host timestamp should be captured for telemetry ingest',
  );
  assert.ok(
    Number.isFinite(state.timing.lastSerialSkewMs),
    'source-vs-host skew should be computed when source timestamp exists',
  );

  const serialToOscRoutes = state.routes.filter(
    (route) => route.flow === 'serial->osc' && route.kind === 'telemetry',
  );
  assert.equal(serialToOscRoutes.length, 4);
  const serialToOscAddresses = serialToOscRoutes.map((route) => route.address);
  assert.deepEqual(serialToOscAddresses.sort(), [
    '/mn42/envelopes',
    '/mn42/slots',
    '/mn42/telemetry/envelopes',
    '/mn42/telemetry/slots',
  ]);
  serialToOscRoutes.forEach((route) => {
    assert.equal(route.traceId, 'fw-trace-1');
    assert.equal(route.sourceTimestampMs, 1710000000123);
  });

  const serialToMidiRoutes = state.routes.filter(
    (route) => route.flow === 'serial->midi' && route.kind === 'telemetry',
  );
  assert.equal(serialToMidiRoutes.length, 2);

  udp.emit('message', {
    address: '/mn42/cmd',
    args: [
      {
        cmd: 'SET_SLOT_VALUE',
        slot: 8,
        value: 96,
        traceId: 'osc-cmd-7',
        timestamp: 1710000000200,
      },
    ],
  });
  await wait();

  assert.ok(
    serial.writes.includes('SET_SLOT_VALUE,8,96\n'),
    'OSC command should still forward to serial',
  );

  state = service.getState();
  const oscRoute = [...state.routes]
    .reverse()
    .find((route) => route.flow === 'osc->serial' && route.kind === 'command');
  assert.ok(oscRoute, 'OSC->serial route event should be recorded');
  assert.equal(oscRoute.traceId, 'osc-cmd-7');
  assert.equal(oscRoute.sourceTimestampMs, 1710000000200);
  assert.equal(state.lastRouteTraceId, 'osc-cmd-7');

  context.midiIn.handler({ toArray: () => [0xb0, 9, 65] });
  await wait();
  assert.ok(
    serial.writes.includes('SET_SLOT_VALUE,9,65\n'),
    'MIDI command should still forward to serial',
  );

  state = service.getState();
  const midiRoute = [...state.routes]
    .reverse()
    .find((route) => route.flow === 'midi->serial' && route.kind === 'command');
  assert.ok(midiRoute, 'MIDI->serial route event should be recorded');
  assert.match(midiRoute.traceId || '', /^midi-/);
  assert.equal(midiRoute.slot, 9);
  assert.equal(midiRoute.value, 65);

  for (let index = 0; index < ROUTE_LOG_LIMIT + 25; index += 1) {
    context.midiIn.handler({ toArray: () => [0xb0, index % 42, index % 128] });
  }
  await wait();

  state = service.getState();
  assert.equal(
    state.routes.length,
    ROUTE_LOG_LIMIT,
    'route log should stay bounded to avoid unbounded state growth',
  );

  await service.stop();
  console.log(
    'translation trace metadata preserves timestamps and stays bounded',
  );
}

run().catch((err) => {
  console.error(err);
  process.exit(1);
});
