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
      oscPort: 57121,
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
  const slots = [];
  assert.ok(serial && serial.parser, 'serial parser should be connected');
  assert.ok(udp, 'udp endpoint should be connected');
  assert.ok(
    serial.writes.includes('HELLO\n'),
    'bridge should send HELLO on serial open',
  );
  assert.ok(
    serial.writes.includes('GET_MANIFEST\n'),
    'bridge should request manifest on serial open',
  );

  serial.parser.emit('data', '{"hello":"mn42"}');
  serial.parser.emit(
    'data',
    '{"device_name":"MOARkNOBS-42","schema_version":6,"slot_count":42,"pot_count":42,"envelope_count":6,"led_count":52,"power_profile":"POWER_CHOKED_V1","led_brightness_cap":26,"rail_topology_verified":false}',
  );
  serial.parser.emit(
    'data',
    '{"slots":[{"index":0,"type":"CC","midiChannel":1,"data1":74}]}',
  );
  serial.parser.emit('data', '{"slots":[1,2,3]}');
  await new Promise((resolve) => setTimeout(resolve, 50));

  const slotsEntry = [...udp.sent]
    .reverse()
    .find((entry) => entry.packet?.address === '/mn42/slots');
  if (slotsEntry) {
    slots.splice(0, slots.length, ...slotsEntry.packet.args);
  }

  assert.deepEqual(slots, [1, 2, 3], 'bridge should echo slots via OSC');
  assert.equal(
    udp.sent.some(
      (entry) =>
        entry.packet?.address === '/mn42/slots' &&
        Array.isArray(entry.packet?.args) &&
        entry.packet.args.some((arg) => typeof arg === 'object'),
    ),
    false,
    'bridge should not treat config-shaped slot objects as OSC telemetry',
  );
  assert.equal(
    service.getState().manifest.power_profile,
    'POWER_CHOKED_V1',
    'bridge should retain direct manifest replies for state snapshots',
  );

  await service.stop();
  console.log('serial JSON turns into OSC, as foretold');
}

run().catch((err) => {
  console.error(err);
  process.exit(1);
});
