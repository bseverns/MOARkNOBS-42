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

// Hurl a bunch of OSC commands at the bridge and make sure it only forwards
// the ones that play by the rules. Anything sketchy should get tossed before
// it ever hits the wire.
async function run() {
  FakeSerialPort.instances = [];
  FakeUdpPort.instances = [];
  const service = createBridgeService(
    {
      serialName: '/dev/fake',
      oscPort: 9711,
      oscListen: 9710,
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
      jzz: () => ({
        openMidiOut: () => ({
          or() {
            return this;
          },
          send() {},
          on() {},
        }),
        openMidiIn: () => ({
          or() {
            return this;
          },
          connect() {
            return this;
          },
          on() {},
        }),
      }),
    },
  );

  await service.start();
  await new Promise((r) => setTimeout(r, 100));

  const serial = FakeSerialPort.instances[0];
  const udp = FakeUdpPort.instances[0];
  const writes = serial.writes;
  assert.ok(serial && serial.parser, 'serial parser should be connected');
  assert.ok(udp, 'udp endpoint should be connected');

  // these commands are squeaky clean and should forward without complaint
  const good = [
    { cmd: 'SET_SLOT_VALUE', slot: 0, value: 0 },
    { cmd: 'SET_SLOT_VALUE', slot: 41, value: 127 },
  ];
  good.forEach((c) =>
    udp.emit('message', { address: '/mn42/cmd', args: [JSON.stringify(c)] }),
  );

  // these are mangled in one way or another and should get dropped cold
  const bad = [
    { cmd: 'SET_POT', slot: 0, value: 0 }, // deprecated live-control name
    { cmd: 'SET_SLOT_VALUE', slot: -1, value: 0 }, // slot too small
    { cmd: 'SET_SLOT_VALUE', slot: 42, value: 0 }, // slot too big
    { cmd: 'SET_SLOT_VALUE', slot: 0, value: 128 }, // value out of range
    { cmd: 'SET_SLOT_VALUE', slot: 0, value: 0, junk: 'x'.repeat(200) }, // payload over 128B
  ];
  bad.forEach((c) =>
    udp.emit('message', { address: '/mn42/cmd', args: [JSON.stringify(c)] }),
  );
  udp.emit('message', { address: '/mn42/cmd', args: ['notjson'] }); // and one that's not even JSON

  await new Promise((r) => setTimeout(r, 100));

  // Pluck out the live-value writes and make sure only the good ones made it through.
  const forwards = writes.filter((w) => w.startsWith('SET_SLOT_VALUE,'));
  assert.deepEqual(
    forwards,
    good.map((c) => `SET_SLOT_VALUE,${c.slot},${c.value}\n`),
    'only in-range cmds should forward',
  );
  await service.stop();
  console.log('validation clamps size and range before forwarding');
}

run().catch((err) => {
  console.error(err);
  process.exit(1);
});
