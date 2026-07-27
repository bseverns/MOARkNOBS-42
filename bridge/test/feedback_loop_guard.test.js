const { strict: assert } = require('node:assert');
const { EventEmitter } = require('node:events');

const {
  createBridgeService,
  MAX_SERIAL_LINE_LEN,
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

  open(cb) {
    if (typeof cb === 'function') cb();
    this.emit('open');
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
    midiOut.sent = [];
    midiOut.or = function or() {
      return this;
    };
    midiOut.send = function send(msg) {
      this.sent.push(msg);
    };
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

function makeService(config = {}) {
  FakeSerialPort.instances = [];
  FakeUdpPort.instances = [];
  const { jzzFactory, context } = createFakeJzzFactory();
  const service = createBridgeService(
    {
      serialName: '/dev/fake',
      oscPort: 9120,
      oscListen: 9121,
      oscHost: '127.0.0.1',
      oscBind: '127.0.0.1',
      midiLabel: 'MN42 Bridge Test',
      ...config,
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

async function testFeedbackSuppressionDefault() {
  const { service, context } = makeService();
  await service.start();
  await wait();

  const serial = FakeSerialPort.instances[0];
  assert.ok(serial && serial.parser, 'serial parser should be wired');
  serial.parser.emit('data', '{"hello":"mn42"}');
  serial.parser.emit(
    'data',
    '{"type":"telemetry","slots":[10],"envelopes":[]}',
  );
  await wait();

  assert.ok(
    context.midiIn && typeof context.midiIn.handler === 'function',
    'midi input handler should be connected',
  );

  // This mirrors the just-sent slot telemetry CC and should be dropped.
  context.midiIn.handler({ toArray: () => [0xb0, 0, 10] });
  // Different CC/value should still pass.
  context.midiIn.handler({ toArray: () => [0xb0, 1, 11] });
  await wait();

  assert.deepEqual(
    serial.writes.filter((line) => line.startsWith('SET_SLOT_VALUE,')),
    ['SET_SLOT_VALUE,1,11\n'],
    'default mode should suppress telemetry echo but keep fresh MIDI input',
  );

  const counters = service.getState().counters;
  assert.equal(
    counters.feedbackSuppressed,
    1,
    'suppressed MIDI echoes should be counted',
  );

  await service.stop();
}

async function testFeedbackAllowed() {
  const { service, context } = makeService({ allowFeedbackLoops: true });
  await service.start();
  await wait();

  const serial = FakeSerialPort.instances[0];
  serial.parser.emit(
    'data',
    '{"type":"telemetry","slots":[27],"envelopes":[]}',
  );
  await wait();
  context.midiIn.handler({ toArray: () => [0xb0, 0, 27] });
  await wait();

  assert.ok(
    serial.writes.includes('SET_SLOT_VALUE,0,27\n'),
    'allow-feedback-loops should disable echo suppression',
  );
  assert.equal(
    service.getState().counters.feedbackSuppressed,
    0,
    'feedback suppression counter should stay at zero when loops are allowed',
  );

  await service.stop();
}

async function testCounterIncrementPaths() {
  const { service, context } = makeService();
  await service.start();
  await wait();

  const serial = FakeSerialPort.instances[0];
  const udp = FakeUdpPort.instances[0];
  serial.parser.emit('data', 'x'.repeat(MAX_SERIAL_LINE_LEN + 1));
  serial.parser.emit('data', '{"broken":');
  udp.emit('message', { address: '/mn42/cmd', args: ['not-json'] });
  // CC 42 is valid MIDI but is outside the bridge's 0..41 live-slot lane.
  // Do not use CC 99 here: it is an NRPN selector and is intentionally ignored.
  context.midiIn.handler({ toArray: () => [0xb0, 42, 64] });
  await wait();

  const counters = service.getState().counters;
  assert.equal(counters.serialOversizeDrops, 1);
  assert.equal(counters.serialParseErrors, 1);
  assert.equal(counters.badOscCmdDrops, 1);
  assert.equal(counters.badMidiCmdDrops, 1);

  await service.stop();
}

async function run() {
  await testFeedbackSuppressionDefault();
  await testFeedbackAllowed();
  await testCounterIncrementPaths();
  console.log('feedback guard and drop counters behave as expected');
}

run().catch((err) => {
  console.error(err);
  process.exit(1);
});
