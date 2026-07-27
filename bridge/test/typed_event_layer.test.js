const { strict: assert } = require('node:assert');
const { EventEmitter } = require('node:events');

const { createBridgeService } = require('../lib/bridge_service');

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
    midiOut.sent = [];
    midiOut.or = function or() {
      return this;
    };
    midiOut.send = function send(message) {
      this.sent.push(Array.isArray(message) ? [...message] : message);
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

function makeService() {
  FakeSerialPort.instances = [];
  FakeUdpPort.instances = [];
  const { jzzFactory, context } = createFakeJzzFactory();
  const service = createBridgeService(
    {
      serialName: '/dev/fake',
      oscPort: 9160,
      oscListen: 9161,
      oscHost: '127.0.0.1',
      oscBind: '127.0.0.1',
      midiLabel: 'MN42 Typed Events',
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

function parseOscPayload(entry) {
  const arg = entry?.packet?.args?.[0];
  if (typeof arg !== 'string') return null;
  try {
    return JSON.parse(arg);
  } catch (_) {
    return null;
  }
}

async function run() {
  const { service, context } = makeService();
  await service.start();
  await wait();

  const serial = FakeSerialPort.instances[0];
  const udp = FakeUdpPort.instances[0];
  assert.ok(serial && serial.parser, 'serial parser should be connected');
  assert.ok(udp, 'udp endpoint should be connected');

  // OSC typed note event should bridge to virtual MIDI output.
  udp.emit('message', {
    address: '/mn42/event/note_on',
    args: [
      {
        channel: 2,
        note: 60,
        velocity: 99,
        traceId: 'osc-note-1',
        timestamp: 1710000010000,
      },
    ],
  });
  await wait();
  assert.deepEqual(
    context.midiOut.sent[0],
    [0x91, 60, 99],
    'typed OSC note_on should emit MIDI note_on on the requested channel',
  );

  // Typed OSC CC should go to both MIDI and firmware slot-control lane.
  udp.emit('message', {
    address: '/mn42/event/cc',
    args: [
      JSON.stringify({
        channel: 3,
        controller: 7,
        value: 88,
        traceId: 'osc-cc-7',
        timestampMs: 1710000010012,
      }),
    ],
  });
  await wait();
  assert.ok(
    context.midiOut.sent.some(
      (message) =>
        Array.isArray(message) &&
        message[0] === 0xb2 &&
        message[1] === 7 &&
        message[2] === 88,
    ),
    'typed OSC cc should emit MIDI CC with channel preserved',
  );
  assert.ok(
    serial.writes.includes('SET_SLOT_VALUE,7,88\n'),
    'typed OSC cc in slot range should still hit firmware live slot command',
  );

  // NRPN events should become the standard 4-message CC sequence.
  const sentBeforeNrpn = context.midiOut.sent.length;
  udp.emit('message', {
    address: '/mn42/event/nrpn',
    args: [
      {
        channel: 1,
        parameter: 513,
        value: 2047,
      },
    ],
  });
  await wait();
  const nrpnPackets = context.midiOut.sent.slice(sentBeforeNrpn);
  assert.deepEqual(nrpnPackets, [
    [0xb0, 99, 4],
    [0xb0, 98, 1],
    [0xb0, 6, 15],
    [0xb0, 38, 127],
  ]);

  // Inbound RPN/NRPN data-entry CCs must not double as implicit slot writes.
  const serialBeforeParameter = serial.writes.length;
  context.midiIn.handler({ toArray: () => [0xb0, 99, 4] });
  context.midiIn.handler({ toArray: () => [0xb0, 98, 1] });
  context.midiIn.handler({ toArray: () => [0xb0, 6, 15] });
  context.midiIn.handler({ toArray: () => [0xb0, 38, 127] });
  await wait();
  assert.equal(
    serial.writes.slice(serialBeforeParameter).some((line) => line.startsWith('SET_SLOT_VALUE,6,') || line.startsWith('SET_SLOT_VALUE,38,')),
    false,
    'RPN/NRPN selectors and data-entry controllers must not mutate live slots',
  );

  // MIDI inbound should rebroadcast into typed OSC namespaces.
  context.midiIn.handler({ toArray: () => [0x90, 64, 100] });
  await wait();
  const noteOsc = [...udp.sent]
    .reverse()
    .find((entry) => entry.packet?.address === '/mn42/event/note_on');
  assert.ok(noteOsc, 'midi note_on should emit typed OSC note_on event');
  const notePayload = parseOscPayload(noteOsc);
  assert.equal(notePayload.note, 64);
  assert.equal(notePayload.velocity, 100);

  context.midiIn.handler({ toArray: () => [0xe1, 0x00, 0x40] });
  await wait();
  const bendOsc = [...udp.sent]
    .reverse()
    .find((entry) => entry.packet?.address === '/mn42/event/pitch_bend');
  assert.ok(bendOsc, 'midi pitch bend should emit typed OSC pitch_bend event');
  const bendPayload = parseOscPayload(bendOsc);
  assert.equal(bendPayload.channel, 2);
  assert.equal(bendPayload.value, 8192);

  const state = service.getState();
  const oscToMidiEvent = state.routes.find(
    (route) => route.flow === 'osc->midi' && route.kind === 'event',
  );
  assert.ok(oscToMidiEvent, 'typed OSC->MIDI event route should be tracked');

  await service.stop();
  console.log('typed OSC/MIDI event layer routes note/cc/nrpn/pitch messages');
}

run().catch((err) => {
  console.error(err);
  process.exit(1);
});
