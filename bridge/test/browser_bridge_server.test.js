const { strict: assert } = require('node:assert');
const { EventEmitter } = require('node:events');
const { Readable } = require('node:stream');

const { createBrowserBridgeServer } = require('../lib/http_bridge_server');
const { createBridgeContract } = require('../lib/bridge_contract');

function makeFakeService() {
  const events = new EventEmitter();
  const configureCalls = [];
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
      midiDestinationName: 'Ableton',
      oscDestinationName: 'TouchDesigner',
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
      configValidation: { status: 'verified', errors: [] },
      deviceAuthority: 'verified-device-different',
      draftState: 'dirty',
      clientApplyId: 'bootstrap-apply',
      stagedRevision: 23,
      stagedDigest: 'bootstrap-digest',
      lastApplyResult: null,
      powerSafety: {
        power_profile: 'POWER_CHOKED_V1',
        led_brightness_cap: 26,
        rail_topology_verified: false,
      },
      hardwareHealth: {
        display_present: true,
        display_ok: true,
        display_init_failures: 0,
        display_status: 'ok',
        brownout_count: 0,
        eeprom_primary_valid: true,
        eeprom_backup_valid: true,
        eeprom_last_load: 'primary',
        free_ram: 32768,
        free_flash: 1048576,
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
    configureCalls,
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
    async configure(nextConfig = {}, options = {}) {
      configureCalls.push({ nextConfig, options });
      state.config = { ...state.config, ...nextConfig };
      events.emit('state', this.getState());
      return this.getState();
    },
    async start() {
      state.running = true;
      state.serialConnected = true;
      state.deviceSession.connected = true;
      events.emit('state', this.getState());
      return this.getState();
    },
    async stop() {
      state.running = false;
      state.serialConnected = false;
      state.ready = false;
      state.deviceSession.connected = false;
      state.deviceSession.ready = false;
      events.emit('state', this.getState());
      return this.getState();
    },
    async stageDeviceConfig(config) {
      if (config?.slots === 'invalid') {
        const error = new Error('Staged config failed schema validation');
        error.code = 'schema_validation_failed';
        error.statusCode = 422;
        error.details = {
          errors: [{ instancePath: '/slots', message: 'must be array' }],
        };
        throw error;
      }
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
    async prewarmDeviceSession() {
      state.deviceSession.schema = {
        schema_version: 6,
        type: 'object',
        properties: { slots: { type: 'array' } },
      };
      state.deviceSession.schemaSource = 'bundled';
      return this.getDeviceSessionState();
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
    setAppDisplayMetadata(metadata) {
      state.appDisplayMetadata = {
        authority: 'advisory-browser-metadata',
        ...JSON.parse(JSON.stringify(metadata)),
      };
      return state.appDisplayMetadata;
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
    emitStructuredEvent(message) {
      events.emit('structured-event', JSON.parse(JSON.stringify(message)));
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

function makeSocket() {
  const socket = new EventEmitter();
  socket.writes = [];
  socket.ended = false;
  socket.destroyed = false;
  socket.write = (chunk) => {
    socket.writes.push(
      Buffer.isBuffer(chunk) ? chunk : Buffer.from(String(chunk)),
    );
  };
  socket.end = (chunk = '') => {
    if (chunk) socket.write(chunk);
    socket.ended = true;
    socket.emit('end');
  };
  socket.destroy = () => {
    socket.destroyed = true;
    socket.emit('close');
  };
  socket.on = socket.addListener.bind(socket);
  return socket;
}

function makeUpgradeReq(url = '/ws') {
  return {
    url,
    headers: {
      host: '127.0.0.1',
      'sec-websocket-key': 'dGhlIHNhbXBsZSBub25jZQ==',
      connection: 'Upgrade',
      upgrade: 'websocket',
      'sec-websocket-version': '13',
    },
  };
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

function decodeServerFrame(frame) {
  let offset = 2;
  let length = frame[1] & 0x7f;
  if (length === 126) {
    length = frame.readUInt16BE(2);
    offset = 4;
  } else if (length === 127) {
    length = Number(frame.readBigUInt64BE(2));
    offset = 10;
  }
  const payload = frame.subarray(offset, offset + length);
  return {
    opcode: frame[0] & 0x0f,
    length,
    payload: payload.toString('utf8'),
    payloadBuffer: payload,
  };
}

async function startBrowserServer(options = {}) {
  const service = makeFakeService();
  const { httpApi, server } = makeFakeHttpApi();
  const browserServer = createBrowserBridgeServer({
    service,
    host: '127.0.0.1',
    port: 0,
    httpApi,
    requireControlToken: false,
    ...options,
  });
  await browserServer.start();
  return { browserServer, server, service };
}

async function assertJsonApiBodyLimit() {
  const { browserServer, server, service } = await startBrowserServer({
    maxJsonApiBodyBytes: 32,
  });
  const response = makeRes();
  await server.requestHandler(
    makeReq({
      method: 'POST',
      url: '/api/connect',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ serialName: 'x'.repeat(64) }),
    }),
    response,
  );
  const payload = JSON.parse(response.body.toString('utf8'));
  assert.equal(response.statusCode, 413);
  assert.equal(payload.error?.code, 'request_body_too_large');
  assert.equal(payload.error?.limitBytes, 32);
  assert.match(payload.error?.message || '', /32 byte limit/);
  assert.equal(
    service.getState().running,
    false,
    'oversized API body should not start the bridge service',
  );
  await browserServer.stop();
}

async function assertRawWebSocketMessageLimit() {
  const { browserServer, server, service } = await startBrowserServer({
    maxRawWebSocketMessageBytes: 8,
  });
  const socket = makeSocket();
  server.emit('upgrade', makeUpgradeReq('/ws'), socket);
  assert.match(
    Buffer.concat(socket.writes).toString('utf8'),
    /HTTP\/1\.1 101 Switching Protocols/,
    'raw websocket upgrade should succeed before message limit enforcement',
  );

  socket.emit('data', encodeClientTextFrame('123456789\n'));
  const close = decodeServerFrame(socket.writes.at(-1));
  assert.equal(close.opcode, 8, 'oversized raw websocket message should close');
  assert.equal(
    close.payloadBuffer.readUInt16BE(0),
    1009,
    'oversized raw websocket message should use message-too-big close code',
  );
  assert.deepEqual(
    service.sentLines,
    [],
    'oversized raw websocket message should not reach the bridge service',
  );
  await browserServer.stop();
}

async function assertRawWebSocketProtocolGuard() {
  const { browserServer, server, service } = await startBrowserServer();
  const socket = makeSocket();
  server.emit('upgrade', makeUpgradeReq('/ws'), socket);
  // Client-to-server WebSocket frames are required to be masked and final.
  socket.emit('data', Buffer.from([0x81, 0x03, 0x50, 0x49, 0x4e]));
  const close = decodeServerFrame(socket.writes.at(-1));
  assert.equal(close.opcode, 8);
  assert.equal(close.payloadBuffer.readUInt16BE(0), 1002);
  assert.deepEqual(service.sentLines, []);
  await browserServer.stop();
}

async function assertWebSocketClientLimit() {
  const { browserServer, server } = await startBrowserServer({
    maxBrowserWebSocketClients: 1,
  });
  const firstSocket = makeSocket();
  server.emit('upgrade', makeUpgradeReq('/ws'), firstSocket);
  assert.match(
    Buffer.concat(firstSocket.writes).toString('utf8'),
    /HTTP\/1\.1 101 Switching Protocols/,
    'first websocket client should be accepted',
  );

  const secondSocket = makeSocket();
  server.emit('upgrade', makeUpgradeReq('/ws/events'), secondSocket);
  const secondText = Buffer.concat(secondSocket.writes).toString('utf8');
  assert.match(secondText, /HTTP\/1\.1 503 Service Unavailable/);
  assert.match(secondText, /browser websocket client limit reached/);
  assert.equal(secondSocket.ended, true);
  await browserServer.stop();
}

async function assertControlTokenBoundary() {
  const service = makeFakeService();
  const { httpApi, server } = makeFakeHttpApi();
  const browserServer = createBrowserBridgeServer({
    service,
    host: '127.0.0.1',
    port: 0,
    httpApi,
    controlToken: 'test-control-token',
  });
  const address = await browserServer.start();
  assert.match(address.url, /token=test-control-token/);

  const denied = makeRes();
  await server.requestHandler(
    makeReq({ method: 'POST', url: '/api/connect', headers: { host: '127.0.0.1:12345' }, body: '{}' }),
    denied,
  );
  assert.equal(denied.statusCode, 401, 'Bridge mutations require the launch token');

  const allowed = makeRes();
  await server.requestHandler(
    makeReq({
      method: 'POST',
      url: '/api/connect',
      headers: { host: '127.0.0.1:12345', authorization: 'Bearer test-control-token' },
      body: '{}',
    }),
    allowed,
  );
  assert.equal(allowed.statusCode, 200, 'the launch token authorizes Bridge control');

  const socket = makeSocket();
  server.emit('upgrade', makeUpgradeReq('/ws'), socket);
  assert.match(Buffer.concat(socket.writes).toString('utf8'), /401 Unauthorized/);
  await browserServer.stop();
}

async function run() {
  const service = makeFakeService();
  const { httpApi, server } = makeFakeHttpApi();
  const bridgeContract = createBridgeContract({
    sourceSha: 'browser-server-test-sha',
  });
  const browserServer = createBrowserBridgeServer({
    service,
    host: '127.0.0.1',
    port: 0,
    httpApi,
    requireControlToken: false,
    bridgeContract,
  });
  const address = await browserServer.start();
  assert.equal(address.port, 12345);

  const contractResponse = makeRes();
  await server.requestHandler(
    makeReq({ method: 'GET', url: '/api/contract' }),
    contractResponse,
  );
  const contractPayload = JSON.parse(contractResponse.body.toString('utf8'));
  assert.deepEqual(contractPayload.contract, {
    bridge_api_version: 1,
    event_contract_version: 1,
    bridge_version: '1.0.0',
    bridge_source_sha: 'browser-server-test-sha',
    supported_schema_versions: [9],
    verified_apply: true,
    structured_session: true,
  });

  const stateResponse = makeRes();
  await server.requestHandler(
    makeReq({ method: 'GET', url: '/api/state' }),
    stateResponse,
  );
  const statePayload = JSON.parse(stateResponse.body.toString('utf8'));
  assert.deepEqual(
    statePayload.contract,
    contractPayload.contract,
    'state endpoint should carry the same negotiated Bridge contract',
  );
  assert.equal(
    statePayload.state.running,
    false,
    'state endpoint should expose idle bridge state',
  );

  const crossOriginApply = makeRes();
  await server.requestHandler(
    makeReq({
      method: 'POST',
      url: '/api/device/apply',
      headers: {
        host: '127.0.0.1:12345',
        origin: 'https://untrusted.example',
        'content-type': 'application/json',
      },
      body: '{}',
    }),
    crossOriginApply,
  );
  assert.equal(
    crossOriginApply.statusCode,
    403,
    'cross-origin browser control requests must not reach Bridge mutation endpoints',
  );

  const sessionResponse = makeRes();
  await server.requestHandler(
    makeReq({ method: 'GET', url: '/api/device/session' }),
    sessionResponse,
  );
  const sessionPayload = JSON.parse(sessionResponse.body.toString('utf8'));
  assert.deepEqual(
    sessionPayload.contract,
    contractPayload.contract,
    'session endpoint should carry the same negotiated Bridge contract',
  );
  assert.equal(
    sessionPayload.session?.firmwareIdentity?.device_name,
    'MOARkNOBS-42',
    'device session endpoint should expose cached firmware identity',
  );

  const warmedSessionResponse = makeRes();
  await server.requestHandler(
    makeReq({ method: 'GET', url: '/api/device/session?warm=1' }),
    warmedSessionResponse,
  );
  const warmedSessionPayload = JSON.parse(
    warmedSessionResponse.body.toString('utf8'),
  );
  assert.equal(
    warmedSessionPayload.session?.schema?.type,
    'object',
    'warmed device session endpoint should expose bundled schema metadata',
  );
  assert.equal(
    warmedSessionPayload.session?.schemaSource,
    'bundled',
    'warmed device session endpoint should report bundled schema source',
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

  const eventSocket = new EventEmitter();
  eventSocket.writes = [];
  eventSocket.write = (chunk) => {
    eventSocket.writes.push(Buffer.from(chunk));
  };
  eventSocket.end = () => {};
  eventSocket.destroy = () => {};
  eventSocket.on = eventSocket.addListener.bind(eventSocket);
  const eventUpgradeReq = {
    url: '/ws',
    headers: {
      host: '127.0.0.1',
      'sec-websocket-key': 'dGhlIHNhbXBsZSBub25jZQ==',
      connection: 'Upgrade',
      upgrade: 'websocket',
      'sec-websocket-version': '13',
    },
  };
  server.emit(
    'upgrade',
    {
      ...eventUpgradeReq,
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
  const bootstrapSnapshot = JSON.parse(
    decodeServerFrame(eventSocket.writes[1]).payload,
  );
  assert.equal(bootstrapSnapshot.event, 'device.session.snapshot');
  assert.equal(bootstrapSnapshot.payload.deviceAuthority, 'verified-device-different');
  assert.equal(bootstrapSnapshot.payload.draftState, 'dirty');
  assert.equal(bootstrapSnapshot.payload.clientApplyId, 'bootstrap-apply');
  assert.equal(bootstrapSnapshot.payload.stagedRevision, 23);
  assert.equal(bootstrapSnapshot.payload.stagedDigest, 'bootstrap-digest');

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
  service.emitStructuredEvent({
    version: 1,
    event: 'device.ready',
    at: new Date().toISOString(),
    payload: { manifest: { device_name: 'MOARkNOBS-42' } },
  });
  const eventFrameTextAfterConnect = Buffer.concat(eventSocket.writes).toString(
    'utf8',
  );
  assert.match(
    eventFrameTextAfterConnect,
    /device\.ready/,
    'structured websocket should survive connect and receive later events',
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

  const invalidStageResponse = makeRes();
  await server.requestHandler(
    makeReq({
      method: 'POST',
      url: '/api/device/stage',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({
        config: {
          slots: 'invalid',
        },
      }),
    }),
    invalidStageResponse,
  );
  const invalidStagePayload = JSON.parse(
    invalidStageResponse.body.toString('utf8'),
  );
  assert.equal(
    invalidStageResponse.statusCode,
    422,
    'device stage endpoint should preserve structured validation status codes',
  );
  assert.equal(
    invalidStagePayload.error?.code,
    'schema_validation_failed',
    'device stage endpoint should expose machine-readable validation errors',
  );
  assert.equal(
    invalidStagePayload.error?.failureClass,
    'preflight-rejected',
    'device stage endpoint should classify failures before serial transmission',
  );
  assert.equal(
    Array.isArray(invalidStagePayload.error?.details?.errors),
    true,
    'device stage endpoint should preserve validation error details',
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

  const originalApplyDeviceConfig = service.applyDeviceConfig;
  for (const [code, expectedFailureClass] of [
    ['stale_session_revision', 'preflight-rejected'],
    ['device_checksum', 'device-rejected-before-commit'],
    ['apply_transport_error', 'transmission-unknown'],
  ]) {
    service.applyDeviceConfig = async () => {
      const error = new Error(code);
      error.code = code;
      error.statusCode = 409;
      throw error;
    };
    const rejectedApplyResponse = makeRes();
    await server.requestHandler(
      makeReq({
        method: 'POST',
        url: '/api/device/apply',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({}),
      }),
      rejectedApplyResponse,
    );
    const rejectedApplyPayload = JSON.parse(
      rejectedApplyResponse.body.toString('utf8'),
    );
    assert.equal(
      rejectedApplyPayload.error?.failureClass,
      expectedFailureClass,
      `device apply endpoint should classify ${code}`,
    );
  }
  service.applyDeviceConfig = originalApplyDeviceConfig;

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

  const consoleResponse = makeRes();
  await server.requestHandler(
    makeReq({ method: 'GET', url: '/' }),
    consoleResponse,
  );
  const consoleHtml = consoleResponse.body.toString('utf8');
  assert.match(
    consoleHtml,
    /Bridge command center/,
    'server should expose the simplified first-run console copy',
  );
  assert.match(
    consoleHtml,
    /Choose device[\s\S]*Choose recipe[\s\S]*Open App[\s\S]*Snapshot/,
    'console should explain the default five-step bridge path',
  );
  assert.match(
    consoleHtml,
    /Advanced setup: guard and timing controls/,
    'console should keep guard and timing controls available behind advanced setup',
  );
  assert.match(
    consoleHtml,
    /Config export[\s\S]*Device truth[\s\S]*Draft state/,
    'Stage should expose validation and authority state in operator language',
  );
  assert.match(
    consoleHtml,
    /operator-state\.js[\s\S]*bridge-ui\.js/,
    'console should load tested operator-state classification before its UI runtime',
  );
  assert.match(
    consoleHtml,
    /Routing heartbeat[\s\S]*Performance setup:[\s\S]*OSC destination · OSC[\s\S]*MIDI destination · MIDI/,
    'Stage should expose named passive routing destinations',
  );
  assert.match(
    consoleHtml,
    /Start passive soundcheck/,
    'Stage should expose a write-free guided soundcheck',
  );
  assert.match(
    consoleHtml,
    /Learn a MIDI → OSC mapping[\s\S]*Listen for MIDI CC[\s\S]*Recently observed custom address[\s\S]*Confirm and add mapping/,
    'Mappings should expose a guided passive learn and explicit review flow',
  );
  assert.match(
    consoleHtml,
    /My Performance Setups[\s\S]*separate from firmware[\s\S]*Suggested device profile[\s\S]*Save current[\s\S]*Load selected[\s\S]*Export JSON[\s\S]*Import JSON/,
    'Setup should expose browser-local Performance Setups with portable JSON',
  );
  assert.match(
    consoleHtml,
    /id="stop-bridge" class="action-danger" data-console-modes="setup mappings stage advanced"/,
    'Stop should remain available in every mode while looking destructive',
  );
  assert.match(
    consoleHtml,
    /id="reset-metrics" data-console-modes="advanced"[\s\S]*id="clear-alerts" data-console-modes="advanced"/,
    'diagnostic reset and acknowledgement actions should stay in Advanced',
  );

  const mappingResponse = makeRes();
  await server.requestHandler(
    makeReq({
      method: 'POST',
      url: '/api/mappings',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({
        midiToOscMappings: [
          {
            id: 'learned-filter',
            controller: 21,
            channel: 3,
            address: '/show/filter',
            valueMode: 'normalized',
          },
        ],
      }),
    }),
    mappingResponse,
  );
  assert.equal(mappingResponse.statusCode, 200);
  assert.deepEqual(service.configureCalls.at(-1)?.options, { restart: false });
  assert.deepEqual(
    service.getState().config.midiToOscMappings,
    [
      {
        id: 'learned-filter',
        controller: 21,
        channel: 3,
        address: '/show/filter',
        valueMode: 'normalized',
      },
    ],
    'mapping edits should become live Bridge config without restarting transports',
  );
  const invalidMappingResponse = makeRes();
  await server.requestHandler(
    makeReq({
      method: 'POST',
      url: '/api/mappings',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ midiToOscMappings: 'not-an-array' }),
    }),
    invalidMappingResponse,
  );
  assert.equal(invalidMappingResponse.statusCode, 400);

  const displayMetadataResponse = makeRes();
  await server.requestHandler(
    makeReq({
      method: 'POST',
      url: '/api/display-metadata',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({
        profileLabels: ['Rehearsal'],
        activeProfile: 0,
        slots: [{ index: 3, label: 'Lighting wash' }],
      }),
    }),
    displayMetadataResponse,
  );
  assert.equal(displayMetadataResponse.statusCode, 200);
  assert.equal(
    JSON.parse(displayMetadataResponse.body.toString('utf8')).metadata.authority,
    'advisory-browser-metadata',
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
  assert.equal(
    snapshotPayload.hardwareHealth?.display_ok,
    true,
    'snapshot endpoint should include manifest-backed display health',
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
  const largePayload = 'x'.repeat(70000);
  service.emitLine(largePayload);
  const largeFrame = decodeServerFrame(socket.writes.at(-1));
  assert.equal(
    largeFrame.opcode,
    1,
    'large server frame should be a text frame',
  );
  assert.equal(
    largeFrame.length,
    largePayload.length + 1,
    'large server frame should use the full 64-bit payload length',
  );
  assert.equal(
    largeFrame.payload,
    `${largePayload}\n`,
    'large server frame should preserve the full payload',
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
  await assertJsonApiBodyLimit();
  await assertRawWebSocketMessageLimit();
  await assertRawWebSocketProtocolGuard();
  await assertWebSocketClientLimit();
  await assertControlTokenBoundary();
  console.log(
    'browser bridge server exposes API, app, and websocket transport',
  );
}

run().catch((err) => {
  console.error(err);
  process.exit(1);
});
