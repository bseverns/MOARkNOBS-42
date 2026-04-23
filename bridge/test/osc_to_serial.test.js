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
      oscPort: 9701,
      oscListen: 9700,
      oscHost: '127.0.0.1',
      oscBind: '127.0.0.1',
      midiLabel: 'MN42 Bridge Test',
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
  const { service } = makeService();
  await service.start();
  await wait();

  const serial = FakeSerialPort.instances[0];
  const udp = FakeUdpPort.instances[0];
  assert.ok(serial && serial.parser, 'serial parser should be connected');
  assert.ok(udp, 'udp endpoint should be connected');

  udp.emit('message', {
    address: '/mn42/cmd',
    args: [
      {
        cmd: 'SET_SLOT_VALUE',
        slot: 2,
        value: 42,
      },
    ],
  });
  await wait();
  assert.ok(
    serial.writes.includes('SET_SLOT_VALUE,2,42\n'),
    'OSC cmd should hit serial as a firmware command',
  );

  await service.stop();
  console.log('OSC commands drive the firmware live-value command lane');
}

run().catch((err) => {
  console.error(err);
  process.exit(1);
});
