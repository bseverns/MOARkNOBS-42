const { strict: assert } = require('node:assert');
const { EventEmitter } = require('node:events');
const net = require('node:net');

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
    logs: [],
    config: {
      serialName: '/dev/ttyACM0',
      oscPort: 9000,
      oscListen: 9000,
      oscHost: '127.0.0.1',
      oscBind: '127.0.0.1',
      midiLabel: 'MN42 Bridge',
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

async function connectWebSocket(port) {
  const socket = net.createConnection({ host: '127.0.0.1', port });
  await new Promise((resolve, reject) => {
    socket.once('connect', resolve);
    socket.once('error', reject);
  });

  let buffer = Buffer.alloc(0);
  socket.on('data', (chunk) => {
    buffer = Buffer.concat([buffer, chunk]);
  });

  socket.write(
    [
      'GET /ws HTTP/1.1',
      'Host: 127.0.0.1',
      'Upgrade: websocket',
      'Connection: Upgrade',
      'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==',
      'Sec-WebSocket-Version: 13',
      '',
      '',
    ].join('\r\n'),
  );

  await new Promise((resolve, reject) => {
    const timer = setTimeout(
      () => reject(new Error('websocket handshake timed out')),
      1000,
    );
    const poll = () => {
      const text = buffer.toString('utf8');
      if (text.includes('\r\n\r\n')) {
        clearTimeout(timer);
        buffer = Buffer.from(
          text.split('\r\n\r\n').slice(1).join('\r\n\r\n'),
          'binary',
        );
        resolve();
        return;
      }
      setTimeout(poll, 10);
    };
    poll();
  });

  async function nextFrameText() {
    await new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error('frame timeout')), 1000);
      const poll = () => {
        if (buffer.length >= 2) {
          clearTimeout(timer);
          resolve();
          return;
        }
        setTimeout(poll, 10);
      };
      poll();
    });

    const first = buffer[0];
    const second = buffer[1];
    assert.equal(first & 0x0f, 0x1, 'expected text frame');
    let offset = 2;
    let length = second & 0x7f;
    if (length === 126) {
      length = buffer.readUInt16BE(2);
      offset = 4;
    }
    const payload = buffer.subarray(offset, offset + length);
    buffer = buffer.subarray(offset + length);
    return payload.toString('utf8');
  }

  return {
    socket,
    sendText(payload) {
      socket.write(encodeClientTextFrame(payload));
    },
    nextFrameText,
    close() {
      socket.end();
    },
  };
}

async function run() {
  const service = makeFakeService();
  const server = createBrowserBridgeServer({
    service,
    host: '127.0.0.1',
    port: 0,
  });
  const address = await server.start();

  const stateResponse = await fetch(
    `http://127.0.0.1:${address.port}/api/state`,
  );
  const statePayload = await stateResponse.json();
  assert.equal(
    statePayload.state.running,
    false,
    'state endpoint should expose idle bridge state',
  );

  const portsResponse = await fetch(
    `http://127.0.0.1:${address.port}/api/ports`,
  );
  const portsPayload = await portsResponse.json();
  assert.equal(
    portsPayload.ports[0].path,
    '/dev/fake',
    'port listing should be proxied',
  );

  const connectResponse = await fetch(
    `http://127.0.0.1:${address.port}/api/connect`,
    {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({
        serialName: '/dev/fake',
        midiLabel: 'Browser Bridge',
        oscPort: 9100,
        oscListen: 9101,
      }),
    },
  );
  const connectPayload = await connectResponse.json();
  assert.equal(
    connectPayload.state.running,
    true,
    'connect endpoint should start the bridge service',
  );
  assert.equal(connectPayload.state.config.midiLabel, 'Browser Bridge');

  const appResponse = await fetch(`http://127.0.0.1:${address.port}/app/`);
  const appHtml = await appResponse.text();
  assert.match(
    appHtml,
    /MOARkNOBZ Control Deck/,
    'server should expose the bundled configurator',
  );

  const client = await connectWebSocket(address.port);
  service.emitLine('{"hello":"mn42"}');
  const inbound = await client.nextFrameText();
  assert.equal(
    inbound,
    '{"hello":"mn42"}\n',
    'websocket should relay serial lines to the browser',
  );

  client.sendText('{"cmd":"PING"}\n');
  await new Promise((resolve) => setTimeout(resolve, 50));
  assert.deepEqual(
    service.sentLines,
    ['{"cmd":"PING"}'],
    'websocket should forward browser lines to the service',
  );

  client.close();
  await server.stop();
  console.log(
    'browser bridge server exposes API, app, and websocket transport',
  );
}

run().catch((err) => {
  console.error(err);
  process.exit(1);
});
