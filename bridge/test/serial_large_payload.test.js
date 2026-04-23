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

async function run() {
  FakeSerialPort.instances = [];
  FakeUdpPort.instances = [];
  const service = createBridgeService(
    {
      serialName: '/dev/fake',
      oscPort: 57122,
      oscListen: 0,
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
  await new Promise((resolve) => setTimeout(resolve, 50));

  const serial = FakeSerialPort.instances[0];
  const udp = FakeUdpPort.instances[0];
  assert.ok(serial && serial.parser, 'serial parser should be connected');
  assert.ok(udp, 'udp endpoint should be connected');

  serial.parser.emit('data', '{"hello":"mn42"}');
  serial.parser.emit(
    'data',
    '{"slots":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,' +
      '16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,123],' +
      '"envelopes":[127,126,125,124,123,87]}',
  );
  await new Promise((resolve) => setTimeout(resolve, 50));

  const slotsEntry = [...udp.sent]
    .reverse()
    .find((entry) => entry.packet?.address === '/mn42/slots');
  const envelopesEntry = [...udp.sent]
    .reverse()
    .find((entry) => entry.packet?.address === '/mn42/envelopes');

  assert.ok(slotsEntry, 'bridge should emit slots telemetry');
  assert.ok(envelopesEntry, 'bridge should emit envelope telemetry');
  const slots = slotsEntry.packet.args;
  const envelopes = envelopesEntry.packet.args;
  assert.equal(
    slots.length,
    42,
    'slots array should survive large serial frame',
  );
  assert.equal(
    envelopes.length,
    6,
    'envelopes array should survive large serial frame',
  );
  assert.equal(slots[0], 0);
  assert.equal(slots[41], 123);
  assert.equal(envelopes[0], 127);
  assert.equal(envelopes[5], 87);

  console.log('large serial telemetry payload still forwards to OSC');
  await service.stop();
}

run().catch((err) => {
  console.error(err);
  process.exit(1);
});
