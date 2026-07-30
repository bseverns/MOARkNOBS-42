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
    this.drainCalls = 0;
    this.parser = null;
    this.device = FakeSerialPort.device;
    FakeSerialPort.lastInstance = this;
    this.unsubscribe = this.device.on('line', (line) => {
      if (this.parser) this.parser.emit('data', line);
    });
    setImmediate(() => this.emit('open'));
  }

  pipe(parser) {
    this.parser = parser;
    return parser;
  }

  write(data, callback) {
    const line = String(data);
    this.writes.push(line);
    if (line.startsWith('SET_ALL ')) {
      FakeSerialPort.setAllWrites += 1;
      if (FakeSerialPort.failSetAllWriteAt === FakeSerialPort.setAllWrites) {
        throw new Error('simulated partial SET_ALL write failure');
      }
      if (FakeSerialPort.asyncFailSetAllWriteAt === FakeSerialPort.setAllWrites) {
        setImmediate(() => callback?.(new Error('simulated delayed SET_ALL write failure')));
        return false;
      }
    }
    this.device.receiveLine(line);
    setImmediate(() => callback?.());
    return true;
  }

  drain(callback) {
    this.drainCalls += 1;
    setImmediate(() => callback?.());
  }

  close(cb) {
    this.unsubscribe?.();
    if (typeof cb === 'function') cb();
    this.emit('close');
  }
}
FakeSerialPort.failSetAllWriteAt = null;
FakeSerialPort.asyncFailSetAllWriteAt = null;
FakeSerialPort.setAllWrites = 0;
FakeSerialPort.lastInstance = null;

class FakeUdpPort extends EventEmitter {
  constructor() {
    super();
    FakeUdpPort.instances.push(this);
  }
  open() {
    setImmediate(() => this.emit('ready'));
  }
  send() {}
  close() {}
}
FakeUdpPort.instances = [];

function createFakeJzzFactory() {
  const context = {
    midiIn: null,
    midiOut: null,
  };

  function jzzFactory() {
    const midiOut = {
      sent: [],
      or() {
        return this;
      },
      send(message) {
        this.sent.push(Array.isArray(message) ? [...message] : message);
      },
      on() {},
    };

    const midiIn = {
      handler: null,
      or() {
        return this;
      },
      connect(handler) {
        this.handler = handler;
        return this;
      },
      on() {},
    };

    context.midiIn = midiIn;
    context.midiOut = midiOut;
    return {
      openMidiOut: () => midiOut,
      openMidiIn: () => midiIn,
    };
  }

  return { jzzFactory, context };
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
  const [
    { createRpcKernel },
    { handleNativePendingResponse },
    { chunkString },
  ] = await Promise.all([
    import('../../App/runtime/rpc_kernel.js'),
    import('../../App/runtime/native_response_router.js'),
    import('../../App/runtime/runtime_utils.js'),
  ]);
  FakeSerialPort.device = createSimulatedMn42Device({ ackDelayMs: 30 });
  FakeUdpPort.instances = [];
  const { jzzFactory, context } = createFakeJzzFactory();
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
      jzz: jzzFactory,
    },
  );

  const events = [];
  service.on('structured-event', (event) => events.push(event));

  await service.start();
  await waitFor(() => service.getState().deviceSession?.ready);
  const udp = FakeUdpPort.instances[0];
  assert.ok(udp, 'OSC ingress transport should be connected');
  assert.ok(
    context.midiIn?.handler,
    'MIDI ingress handler should be connected',
  );

  const baselineStaged = JSON.parse(
    JSON.stringify(service.getState().deviceSession.stagedConfig),
  );

  udp.emit('message', {
    address: '/mn42/cmd',
    args: [{ cmd: 'SET_SLOT_VALUE', slot: 7, value: 88 }],
  });
  await wait(25);
  assert.equal(
    service.getState().deviceSession?.dirty,
    false,
    'OSC live command lane must not dirty staged config',
  );
  assert.deepEqual(
    service.getState().deviceSession?.stagedConfig,
    baselineStaged,
    'OSC live command lane must not rewrite staged config',
  );

  context.midiIn.handler({ toArray: () => [0xb0, 5, 64] });
  await wait(25);
  assert.equal(
    service.getState().deviceSession?.dirty,
    false,
    'MIDI CC live lane must not dirty staged config',
  );
  assert.deepEqual(
    service.getState().deviceSession?.stagedConfig,
    baselineStaged,
    'MIDI CC live lane must not rewrite staged config',
  );

  const staged = JSON.parse(
    JSON.stringify(service.getState().deviceSession.stagedConfig),
  );
  staged.slots[0].midiChannel = 12;
  await service.stageDeviceConfig(staged);
  assert.equal(
    service.getState().deviceSession?.dirty,
    true,
    'stageDeviceConfig should dirty the cached staged config',
  );
  FakeSerialPort.device.emitTelemetry({
    type: 'telemetry',
    slots: [1, 2, 3],
    envelopes: [4, 5, 6],
  });
  const applyPromise = service.applyDeviceConfig();
  await wait(5);
  await assert.rejects(
    () => service.applyDeviceConfig(),
    (error) => error?.code === 'apply_in_progress' && error?.statusCode === 409,
    'a competing Apply is a definitive preflight rejection',
  );
  assert.equal(
    service.getState().deviceSession?.dirty,
    true,
    'dirty state must stay set until the device ACK arrives',
  );
  await applyPromise;
  await wait(25);
  assert.equal(
    service.getState().deviceSession?.dirty,
    false,
    'device ACK should clear dirty state after apply',
  );

  const partiallyWrittenDraft = JSON.parse(
    JSON.stringify(service.getState().deviceSession.stagedConfig),
  );
  partiallyWrittenDraft.slots[1].data1 = 101;
  await service.stageDeviceConfig(partiallyWrittenDraft);
  const serial = FakeSerialPort.lastInstance;
  FakeSerialPort.setAllWrites = 0;
  const drainsBeforeFailedApply = serial.drainCalls;
  FakeSerialPort.asyncFailSetAllWriteAt = 2;
  await assert.rejects(
    () => service.applyDeviceConfig(),
    (error) => error?.code === 'apply_transport_error',
    'a partial Bridge SET_ALL write should reject as a transport error',
  );
  FakeSerialPort.asyncFailSetAllWriteAt = null;
  const abortIndex = serial.writes.lastIndexOf('ABORT_SET_ALL\n');
  const failedChunkIndex = serial.writes.findLastIndex((line) => line.startsWith('SET_ALL '));
  assert.equal(
    abortIndex > failedChunkIndex,
    true,
    'the owning Apply must write ABORT_SET_ALL immediately instead of deferring it',
  );
  assert.equal(
    FakeSerialPort.setAllWrites,
    2,
    'a delayed serial write failure must stop later Apply chunks',
  );
  assert.equal(
    serial.drainCalls > drainsBeforeFailedApply,
    true,
    'Apply writes and the owner abort must await serial drain completion',
  );
  await waitFor(() => !service.getState().deviceSession?.lastApplyResult ||
    service.getState().deviceSession.lastApplyResult.status !== 'pending');

  const nativeTransport = {
    protocol: 'native',
    writeLine: async (line) => service.sendLine(line),
  };
  const rpcKernel = createRpcKernel({
    getTransport: () => nativeTransport,
    isJsonRpcTransport: () => false,
    chunkString,
    nativeSetAllChunkSize: 24,
    nativeSetAllLinePaceMs: 0,
    rpcTimeoutMs: 1000,
    rpcThrottleIntervalMs: 0,
  });
  const unsubscribeRpc = service.on('line', (line) => {
    let msg;
    try {
      msg = JSON.parse(line);
    } catch (_) {
      return;
    }
    const activePending = rpcKernel.getActivePendingRpc();
    handleNativePendingResponse({
      msg,
      activePending,
      activePendingId: activePending?.id,
      rpcKernel,
      isManifestPayload: (payload) =>
        typeof payload?.device_name === 'string' &&
        Number.isFinite(Number(payload?.slot_count)),
      isConfigPayload: (payload) =>
        Array.isArray(payload?.pots) && Array.isArray(payload?.slots),
    });
  });

  const profileReceipt = await rpcKernel.sendRpc({
    rpc: 'set_profile',
    slot: 0,
    profile: {
      arp: { pattern_length: 9 },
      lfos: [{ index: 0, shape: 5, frequency_hz: 1.25 }],
    },
  });
  assert.equal(profileReceipt.command, 'SET_PROFILE');
  assert.equal(profileReceipt.status, 'ok');
  assert.equal(
    FakeSerialPort.device.getManifest().capabilities.arp_live,
    true,
    'simulator should advertise the implemented live arp protocol',
  );
  const storedProfile = await rpcKernel.sendRpc({
    rpc: 'get_profile',
    slot: 0,
  });
  assert.equal(
    storedProfile.arp.pattern_length,
    9,
    'chunked native profile writes should round trip through the bridge and simulator',
  );

  const extendedArp = await rpcKernel.sendRpc({
    rpc: 'set_arp',
    lengthTicks: 6,
    shape: 4,
    swingPercent: 30,
    gatePercent: 75,
    octaveRange: 2,
    patternLength: 14,
  });
  assert.equal(extendedArp.pattern_length, 14);
  const legacyArp = await rpcKernel.sendRpc({
    rpc: 'set_arp',
    lengthTicks: 12,
    shape: 1,
    swingPercent: 10,
    gatePercent: 50,
    octaveRange: 1,
  });
  assert.equal(
    legacyArp.pattern_length,
    14,
    'legacy five-field SET_ARP should retain the live pattern length',
  );
  const liveArp = await rpcKernel.sendRpc({ rpc: 'get_arp' });
  assert.equal(liveArp.pattern_length, 14);
  unsubscribeRpc();

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
