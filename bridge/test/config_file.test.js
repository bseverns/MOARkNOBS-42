const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { strict: assert } = require('node:assert');
const { EventEmitter } = require('node:events');

const {
  parseConfigFromArgv,
  createBridgeService,
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

function wait(ms = 20) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function writeTempConfig(payload) {
  const filePath = path.join(
    os.tmpdir(),
    `mn42-bridge-config-${process.pid}-${Date.now()}.json`,
  );
  fs.writeFileSync(filePath, JSON.stringify(payload, null, 2));
  return filePath;
}

async function run() {
  const configPath = writeTempConfig({
    serialName: '/dev/from-config',
    oscHost: '192.168.1.50',
    oscPort: 9010,
    oscListen: 9011,
    midiLabel: 'Config Bus',
    midiToOscMappings: [
      {
        kind: 'cc',
        channel: 1,
        controller: 20,
        address: '/layer1/opacity',
        valueMode: 'normalized',
      },
    ],
  });

  const parsed = parseConfigFromArgv([
    'node',
    'mn42_bridge.js',
    '--config',
    configPath,
    '--osc',
    '9100',
    '--midi',
    'CLI Override',
  ]);
  assert.equal(parsed.configPath, configPath);
  assert.equal(parsed.serialName, '/dev/from-config');
  assert.equal(parsed.oscHost, '192.168.1.50');
  assert.equal(parsed.oscPort, 9100, 'CLI should override file oscPort');
  assert.equal(parsed.oscListen, 9011, 'file oscListen should persist');
  assert.equal(parsed.midiLabel, 'CLI Override');
  assert.equal(parsed.midiToOscMappings.length, 1);
  assert.deepEqual(parsed.midiToOscMappings[0], {
    id: 'mapping-1',
    kind: 'cc',
    controller: 20,
    channel: 1,
    address: '/layer1/opacity',
    valueMode: 'normalized',
    scale: 1,
    offset: 0,
    argType: 'float',
  });

  FakeSerialPort.instances = [];
  FakeUdpPort.instances = [];
  const { jzzFactory, context } = createFakeJzzFactory();
  const service = createBridgeService(parsed, {
    serialport: {
      SerialPort: FakeSerialPort,
      ReadlineParser: FakeReadlineParser,
    },
    osc: { UDPPort: FakeUdpPort },
    jzz: jzzFactory,
  });

  await service.start();
  await wait();
  context.midiIn.handler({ toArray: () => [0xb0, 20, 64] });
  await wait();

  const mappedOsc = FakeUdpPort.instances[0].sent.find(
    (entry) => entry.packet?.address === '/layer1/opacity',
  );
  assert.ok(mappedOsc, 'configured CC mapping should emit custom OSC');
  assert.equal(mappedOsc.host, '192.168.1.50');
  assert.equal(mappedOsc.port, 9100);
  assert.equal(mappedOsc.packet.args[0], 64 / 127);

  await service.stop();
  fs.unlinkSync(configPath);
  console.log(
    'bridge config file loads transport settings and CC->OSC mappings',
  );
}

run().catch((err) => {
  console.error(err);
  process.exit(1);
});
