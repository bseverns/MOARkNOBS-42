const { strict: assert } = require('node:assert');
const { EventEmitter } = require('node:events');
const { Readable } = require('node:stream');

const { createBrowserBridgeServer } = require('../lib/http_bridge_server');

function makeFakeService() {
  const events = new EventEmitter();
  const state = {
    running: false,
    serialConnected: false,
    ready: false,
    manifest: {
      power_profile: 'POWER_CHOKED_V1',
      led_brightness_cap: 26,
      rail_topology_verified: false,
    },
    lastError: null,
    lastTelemetryAt: null,
    lastRouteAt: null,
    lastRouteTraceId: null,
    logs: [],
    routes: [],
    timing: {
      lastSerialSourceTimestampMs: null,
      lastSerialHostTimestampMs: null,
      lastSerialSkewMs: null,
    },
    performance: {
      roundTrip: {
        windowSize: 200,
        pending: 0,
        sampleCount: 0,
        minMs: null,
        maxMs: null,
        meanMs: null,
        p50Ms: null,
        p95Ms: null,
        lastMs: null,
        jitterMeanMs: null,
        jitterP95Ms: null,
        lastUpdatedAt: null,
      },
      counters: {
        completed: 0,
        expired: 0,
        matchedByTrace: 0,
        matchedBySlotValue: 0,
      },
      health: {
        status: 'no_data',
        reasons: ['No round-trip samples yet'],
        thresholds: {
          p95Ms: 10,
          jitterP95Ms: 5,
        },
        lastEvaluatedAt: null,
      },
    },
    alerts: {
      active: [],
      recent: [],
    },
    counters: {
      serialParseErrors: 0,
      serialOversizeDrops: 0,
      badOscCmdDrops: 0,
      badMidiCmdDrops: 0,
      feedbackSuppressed: 0,
    },
    config: {
      serialName: '/dev/ttyACM0',
      oscPort: 9000,
      oscListen: 9000,
      oscHost: '127.0.0.1',
      oscBind: '127.0.0.1',
      midiLabel: 'MN42 Bridge',
      allowFeedbackLoops: false,
      feedbackWindowMs: 120,
      rtP95TargetMs: 10,
      rtJitterP95TargetMs: 5,
      alertSuppressionMs: 3000,
    },
    deviceSession: {
      connected: false,
      helloSeen: false,
      ready: false,
      schemaSource: 'bundled',
      manifest: null,
      schema: { schema_version: 6, type: 'object', properties: {} },
      liveConfig: null,
      stagedConfig: null,
      dirty: false,
      lastApplyResult: null,
      powerSafety: {
        power_profile: 'POWER_CHOKED_V1',
        led_brightness_cap: 26,
        rail_topology_verified: false,
      },
      firmwareIdentity: {
        device_name: 'MOARkNOBS-42',
        fw_version: 'mock-fw',
        schema_version: 6,
      },
      lastError: null,
    },
  };
  const sentLines = [];
  return {
    sentLines,
    async listSerialPorts() {
      return [
        {
          path: '/dev/fake',
          manufacturer: 'Mock Hardware',
        },
      ];
    },
    async listMidiPorts() {
      return {
        inputs: [
          {
            id: 'Mock MIDI In',
            name: 'Mock MIDI In',
            manufacturer: 'Mock MIDI',
            engine: 'node',
          },
        ],
        outputs: [
          {
            id: 'Mock MIDI Out',
            name: 'Mock MIDI Out',
            manufacturer: 'Mock MIDI',
            engine: 'node',
          },
        ],
      };
    },
    async configure(nextConfig = {}) {
      state.config = { ...state.config, ...nextConfig };
      return this.getState();
    },
    async start() {
      state.running = true;
      state.serialConnected = true;
      state.deviceSession.connected = true;
      return this.getState();
    },
    async stop() {
      state.running = false;
      state.serialConnected = false;
      state.ready = false;
      state.deviceSession.connected = false;
      state.deviceSession.ready = false;
      return this.getState();
    },
    async stageDeviceConfig(config) {
      state.deviceSession.stagedConfig = JSON.parse(JSON.stringify(config));
      state.deviceSession.dirty = true;
      events.emit('structured-event', {
        version: 1,
        event: 'device.config.staged',
        at: new Date().toISOString(),
        payload: { config: state.deviceSession.stagedConfig },
      });
      return { staged: state.deviceSession.stagedConfig, dirty: true };
    },
    async applyDeviceConfig() {
      state.deviceSession.liveConfig = JSON.parse(
        JSON.stringify(state.deviceSession.stagedConfig),
      );
      state.deviceSession.dirty = false;
      state.deviceSession.lastApplyResult = {
        status: 'ack',
        checksum: 'mock-checksum',
      };
      events.emit('structured-event', {
        version: 1,
        event: 'device.apply.ack',
        at: new Date().toISOString(),
        payload: { checksum: 'mock-checksum', seq: 1 },
      });
      return { applied: true, checksum: 'mock-checksum' };
    },
    async rollbackDeviceConfig(reason = 'operator_request') {
      state.deviceSession.stagedConfig = JSON.parse(
        JSON.stringify(state.deviceSession.liveConfig),
      );
      state.deviceSession.dirty = false;
      events.emit('structured-event', {
        version: 1,
        event: 'device.apply.rollback',
        at: new Date().toISOString(),
        payload: { reason },
      });
      return { rolledBack: true, reason };
    },
    getDeviceSessionState() {
      return JSON.parse(JSON.stringify(state.deviceSession));
    },
    async resetPerformance() {
      state.performance.roundTrip = {
        ...state.performance.roundTrip,
        pending: 0,
        sampleCount: 0,
        minMs: null,
        maxMs: null,
        meanMs: null,
        p50Ms: null,
        p95Ms: null,
        lastMs: null,
        jitterMeanMs: null,
        jitterP95Ms: null,
      };
      state.performance.counters = {
        ...state.performance.counters,
        completed: 0,
        expired: 0,
        matchedByTrace: 0,
        matchedBySlotValue: 0,
      };
      state.performance.health = {
        ...state.performance.health,
        status: 'no_data',
        reasons: ['No round-trip samples yet'],
      };
      return this.getState();
    },
    async clearAlerts() {
      state.alerts.active = [];
      return this.getState();
    },
    seedAlert(alert) {
      state.alerts.active.push(JSON.parse(JSON.stringify(alert)));
    },
    sendLine(line) {
      sentLines.push(line);
      return `${line}\n`;
    },
    getState() {
      return JSON.parse(JSON.stringify(state));
    },
    on(eventName, handler) {
      events.on(eventName, handler);
      return () => events.off(eventName, handler);
    },
    emitLine(line) {
      events.emit('line', line);
    },
  };
}

function makeFakeHttpApi() {
  const server = new EventEmitter();
  server.addressValue = { port: 12345 };
  server.listen = (port, host, cb) => {
    server.addressValue = {
      port: port === 0 ? 12345 : port,
      family: 'IPv4',
      address: host,
    };
    if (typeof cb === 'function') cb();
  };
  server.address = () => server.addressValue;
  server.close = (cb) => {
    if (typeof cb === 'function') cb();
  };
  const httpApi = {
    createServer(listener) {
      server.requestHandler = listener;
      return server;
    },
  };
  return { httpApi, server };
}

function makeReq({ method = 'GET', url = '/', headers = {}, body = '' } = {}) {
  const req = Readable.from(body ? [Buffer.from(body)] : []);
  req.method = method;
  req.url = url;
  req.headers = headers;
  return req;
}

function makeRes() {
  const res = new EventEmitter();
  res.headers = {};
  res.statusCode = 0;
  res.body = Buffer.alloc(0);
  res.writeHead = (statusCode, headers = {}) => {
    res.statusCode = statusCode;
    res.headers = { ...res.headers, ...headers };
  };
  res.end = (body = '') => {
    res.body = Buffer.isBuffer(body) ? body : Buffer.from(String(body));
    res.emit('finish');
  };
  return res;
}

function encodeClientTextFrame(payload) {
  const body = Buffer.from(payload, 'utf8');
  const mask = Buffer.from([0x11, 0x22, 0x33, 0x44]);
  const header =
    body.length < 126
      ? Buffer.from([0x81, 0x80 | body.length])
      : Buffer.from([
          0x81,
          0x80 | 126,
          (body.length >> 8) & 0xff,
          body.length & 0xff,
        ]);
  const masked = Buffer.from(body);
  for (let i = 0; i < masked.length; i += 1) {
    masked[i] ^= mask[i % 4];
  }
  return Buffer.concat([header, mask, masked]);
}

async function run() {
  const service = makeFakeService();
  const { httpApi, server } = makeFakeHttpApi();
  const browserServer = createBrowserBridgeServer({
    service,
    host: '127.0.0.1',
    port: 0,
    httpApi,
  });
  const address = await browserServer.start();
  assert.equal(address.port, 12345);

  const stateResponse = makeRes();
  await server.requestHandler(
    makeReq({ method: 'GET', url: '/api/state' }),
    stateResponse,
  );
  const statePayload = JSON.parse(stateResponse.body.toString('utf8'));
  assert.equal(
    statePayload.state.running,
    false,
    'state endpoint should expose idle bridge state',
  );

  const sessionResponse = makeRes();
  await server.requestHandler(
    makeReq({ method: 'GET', url: '/api/device/session' }),
    sessionResponse,
  );
  const sessionPayload = JSON.parse(sessionResponse.body.toString('utf8'));
  assert.equal(
    sessionPayload.session?.firmwareIdentity?.device_name,
    'MOARkNOBS-42',
    'device session endpoint should expose cached firmware identity',
  );

  const portsResponse = makeRes();
  await server.requestHandler(
    makeReq({ method: 'GET', url: '/api/ports' }),
    portsResponse,
  );
  const portsPayload = JSON.parse(portsResponse.body.toString('utf8'));
  assert.equal(
    portsPayload.ports[0].path,
    '/dev/fake',
    'port listing should be proxied',
  );

  const midiPortsResponse = makeRes();
  await server.requestHandler(
    makeReq({ method: 'GET', url: '/api/midi-ports' }),
    midiPortsResponse,
  );
  const midiPortsPayload = JSON.parse(midiPortsResponse.body.toString('utf8'));
  assert.equal(
    midiPortsPayload.inputs[0].name,
    'Mock MIDI In',
    'MIDI input listing should be proxied',
  );
  assert.equal(
    midiPortsPayload.outputs[0].name,
    'Mock MIDI Out',
    'MIDI output listing should be proxied',
  );

  const presetsResponse = makeRes();
  await server.requestHandler(
    makeReq({ method: 'GET', url: '/api/presets' }),
    presetsResponse,
  );
  const presetsPayload = JSON.parse(presetsResponse.body.toString('utf8'));
  assert.equal(
    Array.isArray(presetsPayload.presets) && presetsPayload.presets.length > 0,
    true,
    'preset endpoint should expose known-good host recipes',
  );

  const connectResponse = makeRes();
  await server.requestHandler(
    makeReq({
      method: 'POST',
      url: '/api/connect',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({
        serialName: '/dev/fake',
        midiLabel: 'Browser Bridge',
        oscPort: 9100,
        oscListen: 9101,
        alertSuppressionMs: 2222,
      }),
    }),
    connectResponse,
  );
  const connectPayload = JSON.parse(connectResponse.body.toString('utf8'));
  assert.equal(
    connectPayload.state.running,
    true,
    'connect endpoint should start the bridge service',
  );
  assert.equal(connectPayload.state.config.midiLabel, 'Browser Bridge');
  assert.equal(
    connectPayload.state.config.alertSuppressionMs,
    2222,
    'connect endpoint should apply alert suppression config',
  );

  const stageResponse = makeRes();
  await server.requestHandler(
    makeReq({
      method: 'POST',
      url: '/api/device/stage',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({
        config: {
          slots: [{ midiChannel: 2 }],
        },
      }),
    }),
    stageResponse,
  );
  const stagePayload = JSON.parse(stageResponse.body.toString('utf8'));
  assert.equal(
    stagePayload.result?.dirty,
    true,
    'device stage endpoint should route config drafts into the session cache',
  );

  const applyResponse = makeRes();
  await server.requestHandler(
    makeReq({
      method: 'POST',
      url: '/api/device/apply',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({}),
    }),
    applyResponse,
  );
  const applyPayload = JSON.parse(applyResponse.body.toString('utf8'));
  assert.equal(
    applyPayload.result?.checksum,
    'mock-checksum',
    'device apply endpoint should expose structured apply results',
  );

  const rollbackResponse = makeRes();
  await server.requestHandler(
    makeReq({
      method: 'POST',
      url: '/api/device/rollback',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ reason: 'browser_test' }),
    }),
    rollbackResponse,
  );
  const rollbackPayload = JSON.parse(rollbackResponse.body.toString('utf8'));
  assert.equal(
    rollbackPayload.result?.reason,
    'browser_test',
    'device rollback endpoint should pass through operator reason strings',
  );

  const resetResponse = makeRes();
  await server.requestHandler(
    makeReq({
      method: 'POST',
      url: '/api/performance/reset',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({}),
    }),
    resetResponse,
  );
  const resetPayload = JSON.parse(resetResponse.body.toString('utf8'));
  assert.equal(
    resetPayload.state.performance.roundTrip.sampleCount,
    0,
    'performance reset endpoint should clear round-trip samples',
  );

  service.seedAlert({
    id: 1,
    at: new Date().toISOString(),
    code: 'performance_warn',
    severity: 'warn',
    message: 'Round-trip p95 too high',
    details: null,
  });
  const clearAlertsResponse = makeRes();
  await server.requestHandler(
    makeReq({
      method: 'POST',
      url: '/api/alerts/clear',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({}),
    }),
    clearAlertsResponse,
  );
  const clearAlertsPayload = JSON.parse(
    clearAlertsResponse.body.toString('utf8'),
  );
  assert.equal(
    clearAlertsPayload.state.alerts.active.length,
    0,
    'clear alerts endpoint should clear active alerts',
  );

  const appResponse = makeRes();
  await server.requestHandler(
    makeReq({ method: 'GET', url: '/app/' }),
    appResponse,
  );
  const appHtml = appResponse.body.toString('utf8');
  assert.match(
    appHtml,
    /MOARkNOBS-42 Browser Configurator[\s\S]*Control Deck/,
    'server should expose the bundled configurator',
  );

  const snapshotResponse = makeRes();
  await server.requestHandler(
    makeReq({ method: 'GET', url: '/api/state/snapshot' }),
    snapshotResponse,
  );
  assert.equal(snapshotResponse.statusCode, 200);
  assert.match(
    snapshotResponse.headers['content-disposition'] || '',
    /attachment; filename="mn42-bridge-state-/,
    'snapshot endpoint should provide a download filename',
  );
  const snapshotPayload = JSON.parse(snapshotResponse.body.toString('utf8'));
  assert.equal(
    snapshotPayload.state.config.midiLabel,
    'Browser Bridge',
    'snapshot endpoint should include current bridge state',
  );
  assert.equal(
    typeof snapshotPayload.runtime?.pid,
    'number',
    'snapshot endpoint should include runtime metadata',
  );
  assert.equal(
    snapshotPayload.powerSafety?.power_profile,
    'POWER_CHOKED_V1',
    'snapshot endpoint should include manifest power-safety state',
  );
  assert.equal(
    snapshotPayload.powerSafety?.led_brightness_cap,
    26,
    'snapshot endpoint should preserve manifest LED cap',
  );
  assert.equal(
    snapshotPayload.powerSafety?.rail_topology_verified,
    false,
    'snapshot endpoint should preserve rail-topology verification state',
  );

  const socket = new EventEmitter();
  socket.writes = [];
  socket.write = (chunk) => {
    socket.writes.push(Buffer.from(chunk));
  };
  socket.end = () => {};
  socket.destroy = () => {};
  socket.on = socket.addListener.bind(socket);
  const upgradeReq = {
    url: '/ws',
    headers: {
      host: '127.0.0.1',
      'sec-websocket-key': 'dGhlIHNhbXBsZSBub25jZQ==',
      connection: 'Upgrade',
      upgrade: 'websocket',
      'sec-websocket-version': '13',
    },
  };
  server.emit('upgrade', upgradeReq, socket);
  service.emitLine('{"hello":"mn42"}');
  const handshakeText = Buffer.concat(socket.writes).toString('utf8');
  assert.match(
    handshakeText,
    /HTTP\/1\.1 101 Switching Protocols/,
    'websocket upgrade should succeed',
  );

  socket.emit('data', encodeClientTextFrame('{"cmd":"PING"}\n'));
  await new Promise((resolve) => setTimeout(resolve, 20));
  assert.deepEqual(
    service.sentLines,
    ['{"cmd":"PING"}'],
    'websocket should forward browser lines to the service',
  );

  const eventSocket = new EventEmitter();
  eventSocket.writes = [];
  eventSocket.write = (chunk) => {
    eventSocket.writes.push(Buffer.from(chunk));
  };
  eventSocket.end = () => {};
  eventSocket.destroy = () => {};
  eventSocket.on = eventSocket.addListener.bind(eventSocket);
  server.emit(
    'upgrade',
    {
      ...upgradeReq,
      url: '/ws/events',
    },
    eventSocket,
  );
  const eventHandshakeText = Buffer.concat(eventSocket.writes).toString('utf8');
  assert.match(
    eventHandshakeText,
    /HTTP\/1\.1 101 Switching Protocols/,
    'structured websocket upgrade should succeed',
  );
  service.emitLine('{"hello":"mn42"}');
  service.on('structured-event', () => {});
  const structuredFrameText = Buffer.concat(eventSocket.writes).toString(
    'utf8',
  );
  assert.match(
    structuredFrameText,
    /device\.config\.live[\s\S]*bridge\.performance/,
    'structured websocket should send session/bootstrap events',
  );

  await browserServer.stop();
  console.log(
    'browser bridge server exposes API, app, and websocket transport',
  );
}

run().catch((err) => {
  console.error(err);
  process.exit(1);
});
