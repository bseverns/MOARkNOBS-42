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
    manifest: null,
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
    async configure(nextConfig = {}) {
      state.config = { ...state.config, ...nextConfig };
      return this.getState();
    },
    async start() {
      state.running = true;
      state.serialConnected = true;
      return this.getState();
    },
    async stop() {
      state.running = false;
      state.serialConnected = false;
      state.ready = false;
      return this.getState();
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

  await browserServer.stop();
  console.log(
    'browser bridge server exposes API, app, and websocket transport',
  );
}

run().catch((err) => {
  console.error(err);
  process.exit(1);
});
