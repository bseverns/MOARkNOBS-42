const { strict: assert } = require('node:assert');
const { EventEmitter } = require('node:events');

const { createBridgeService } = require('../lib/bridge_service');
const { createSimulatedMn42Device } = require('../lib/device/simulator');
const {
  validateStructuredEventShape,
} = require('../lib/device/transport_contract');

class FakeReadlineParser extends EventEmitter {}

class FakeSerialPort extends EventEmitter {
  constructor(options = {}) {
    super();
    this.options = options;
    this.writes = [];
    this.parser = null;
    this.device = FakeSerialPort.device;
    this.unsubscribe = this.device.on('line', (line) => {
      if (this.parser) this.parser.emit('data', line);
    });
    setImmediate(() => this.emit('open'));
  }

  pipe(parser) {
    this.parser = parser;
    return parser;
  }

  write(data) {
    this.writes.push(String(data));
    this.device.receiveLine(String(data));
  }

  close(cb) {
    this.unsubscribe?.();
    if (typeof cb === 'function') cb();
    this.emit('close');
  }
}

class FakeUdpPort extends EventEmitter {
  open() {
    setImmediate(() => this.emit('ready'));
  }
  send() {}
  close() {}
}

async function wait(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function waitFor(predicate, timeoutMs = 1500) {
  const startedAt = Date.now();
  while (Date.now() - startedAt < timeoutMs) {
    if (predicate()) return;
    await wait(10);
  }
  throw new Error('timed out waiting for condition');
}

async function run() {
  FakeSerialPort.device = createSimulatedMn42Device();
  const service = createBridgeService(
    {
      serialName: '/dev/fake',
      oscPort: 57121,
      oscListen: 0,
      oscHost: '127.0.0.1',
      oscBind: '127.0.0.1',
      midiLabel: 'MN42 Bridge Contract Test',
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

  const events = [];
  service.on('structured-event', (event) => events.push(event));

  await service.start();
  await waitFor(() => service.getState().deviceSession?.ready);

  const staged = JSON.parse(
    JSON.stringify(service.getState().deviceSession.stagedConfig),
  );
  staged.slots[0].midiChannel = 12;
  await service.stageDeviceConfig(staged);
  FakeSerialPort.device.emitTelemetry({
    type: 'telemetry',
    slots: [1, 2, 3],
    envelopes: [4, 5, 6],
  });
  await service.applyDeviceConfig();
  await wait(25);

  const names = new Set(events.map((entry) => entry.event));
  for (const requiredName of [
    'device.ready',
    'device.telemetry',
    'device.config.live',
    'device.config.staged',
    'device.config.dirty',
    'device.apply.ack',
    'bridge.performance',
  ]) {
    assert.equal(
      names.has(requiredName),
      true,
      `structured bridge contract should emit ${requiredName}`,
    );
  }

  events.forEach((event) => {
    const shapeErrors = validateStructuredEventShape(event);
    assert.deepEqual(
      shapeErrors,
      [],
      `structured event ${event.event} should keep the documented shape`,
    );
  });

  await service.stop();
  console.log(
    'structured bridge contract stays stable for App-facing consumers',
  );
}

run().catch((error) => {
  console.error(error);
  process.exit(1);
});
