const crypto = require('node:crypto');
const fs = require('node:fs/promises');
const http = require('node:http');
const path = require('node:path');

const { DEFAULT_HTTP_PORT } = require('./bridge_service');

const MIME_TYPES = {
  '.css': 'text/css; charset=utf-8',
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.map': 'application/json; charset=utf-8',
  '.png': 'image/png',
  '.svg': 'image/svg+xml',
  '.txt': 'text/plain; charset=utf-8',
};

const PUBLIC_CONFIG_KEYS = [
  'serialName',
  'oscPort',
  'oscListen',
  'oscHost',
  'oscBind',
  'midiLabel',
  'allowFeedbackLoops',
  'feedbackWindowMs',
  'rtP95TargetMs',
  'rtJitterP95TargetMs',
  'alertSuppressionMs',
];

// Parse user-provided port values from the browser console without crashing on junk input.
function normalizePositiveInt(value, fallback) {
  const parsed = parseInt(String(value), 10);
  if (!Number.isInteger(parsed) || parsed <= 0) {
    return fallback;
  }
  return parsed;
}

// Minimal websocket text-frame encoder for the one-way bridge log stream.
function frameText(payload) {
  const body = Buffer.from(String(payload), 'utf8');
  const header =
    body.length < 126
      ? Buffer.from([0x81, body.length])
      : Buffer.from([0x81, 126, (body.length >> 8) & 0xff, body.length & 0xff]);
  return Buffer.concat([header, body]);
}

// Close frame used when the server intentionally tears down a websocket client.
function closeFrame() {
  return Buffer.from([0x88, 0x00]);
}

// Join request paths safely so the browser console cannot escape the intended static roots.
function safeJoin(rootDir, requestPath) {
  const root = path.resolve(rootDir);
  const candidate = path.resolve(root, `.${requestPath}`);
  if (candidate === root) return candidate;
  if (!candidate.startsWith(`${root}${path.sep}`)) return null;
  return candidate;
}

// Read a JSON body for the tiny `/api/*` control surface.
async function readJson(req) {
  const chunks = [];
  for await (const chunk of req) {
    chunks.push(chunk);
  }
  if (!chunks.length) return {};
  return JSON.parse(Buffer.concat(chunks).toString('utf8'));
}

// Reduce SerialPort objects to the fields the browser UI actually needs.
function simplifyPorts(ports) {
  return (ports || []).map((portInfo) => ({
    path: portInfo.path,
    manufacturer: portInfo.manufacturer ?? null,
    serialNumber: portInfo.serialNumber ?? null,
    vendorId: portInfo.vendorId ?? null,
    productId: portInfo.productId ?? null,
    friendlyName:
      [portInfo.manufacturer, portInfo.path].filter(Boolean).join(' ') ||
      portInfo.path ||
      'Unknown port',
  }));
}

function simplifyMidiPorts(ports) {
  return (ports || []).map((portInfo) => ({
    id: portInfo.id ?? portInfo.name ?? null,
    name: portInfo.name ?? portInfo.id ?? 'Unknown MIDI port',
    manufacturer: portInfo.manufacturer ?? null,
    version: portInfo.version ?? null,
    engine: portInfo.engine ?? null,
  }));
}

// Whitelist and normalize config fields coming from the browser console.
function normalizeConfig(body = {}) {
  const nextConfig = {};
  for (const key of PUBLIC_CONFIG_KEYS) {
    if (body[key] === undefined || body[key] === null) continue;
    if (typeof body[key] === 'string') {
      nextConfig[key] = body[key].trim();
    } else {
      nextConfig[key] = body[key];
    }
  }
  if (nextConfig.oscPort !== undefined) {
    nextConfig.oscPort = normalizePositiveInt(nextConfig.oscPort, 9000);
  }
  if (nextConfig.oscListen !== undefined) {
    nextConfig.oscListen = normalizePositiveInt(nextConfig.oscListen, 9000);
  }
  if (nextConfig.feedbackWindowMs !== undefined) {
    nextConfig.feedbackWindowMs = normalizePositiveInt(
      nextConfig.feedbackWindowMs,
      120,
    );
  }
  if (nextConfig.rtP95TargetMs !== undefined) {
    nextConfig.rtP95TargetMs = normalizePositiveInt(
      nextConfig.rtP95TargetMs,
      10,
    );
  }
  if (nextConfig.rtJitterP95TargetMs !== undefined) {
    nextConfig.rtJitterP95TargetMs = normalizePositiveInt(
      nextConfig.rtJitterP95TargetMs,
      5,
    );
  }
  if (nextConfig.alertSuppressionMs !== undefined) {
    nextConfig.alertSuppressionMs = normalizePositiveInt(
      nextConfig.alertSuppressionMs,
      3000,
    );
  }
  if (nextConfig.allowFeedbackLoops !== undefined) {
    const raw = nextConfig.allowFeedbackLoops;
    if (typeof raw === 'string') {
      nextConfig.allowFeedbackLoops = ['1', 'true', 'yes', 'on'].includes(
        raw.toLowerCase(),
      );
    } else {
      nextConfig.allowFeedbackLoops = Boolean(raw);
    }
  }
  return nextConfig;
}

// Serve one static asset from either the bridge UI or bundled App directory.
async function serveFile(res, rootDir, requestPath) {
  const filePath = safeJoin(rootDir, requestPath);
  if (!filePath) {
    res.writeHead(403, { 'content-type': 'text/plain; charset=utf-8' });
    res.end('forbidden');
    return;
  }

  try {
    const stats = await fs.stat(filePath);
    const resolvedPath = stats.isDirectory()
      ? path.join(filePath, 'index.html')
      : filePath;
    const body = await fs.readFile(resolvedPath);
    const ext = path.extname(resolvedPath).toLowerCase();
    res.writeHead(200, {
      'cache-control': 'no-store',
      'content-type': MIME_TYPES[ext] || 'application/octet-stream',
    });
    res.end(body);
  } catch (err) {
    const status = err && err.code === 'ENOENT' ? 404 : 500;
    res.writeHead(status, { 'content-type': 'text/plain; charset=utf-8' });
    res.end(status === 404 ? 'not found' : err.message);
  }
}

// Tiny HTTP + websocket wrapper that gives the bridge a browser-driven front door.
function createBrowserBridgeServer({
  service,
  host = '127.0.0.1',
  port = DEFAULT_HTTP_PORT,
  uiDir = path.resolve(__dirname, '..', 'ui'),
  appDir = path.resolve(__dirname, '..', '..', 'App'),
  httpApi = http,
} = {}) {
  if (!service || typeof service.getState !== 'function') {
    throw new Error('bridge service is required');
  }

  let server = null;
  let unsubscribeLine = null;
  let unsubscribeState = null;
  const sockets = new Set();

  // Forward raw serial lines to every connected websocket client.
  function broadcastLine(line) {
    const frame = frameText(`${String(line).trim()}\n`);
    for (const socket of sockets) {
      try {
        socket.write(frame);
      } catch (_) {
        socket.destroy();
      }
    }
  }

  // Remove a websocket client and send a close frame when possible.
  function destroySocket(socket) {
    if (!socket) return;
    sockets.delete(socket);
    try {
      socket.end(closeFrame());
    } catch (_) {
      socket.destroy();
    }
  }

  // JSON response helper for the browser-facing control API.
  function sendJson(res, statusCode, payload) {
    res.writeHead(statusCode, {
      'cache-control': 'no-store',
      'content-type': 'application/json; charset=utf-8',
    });
    res.end(JSON.stringify(payload));
  }

  // Handle the small REST surface that starts/stops the bridge and exposes status.
  async function handleApi(req, res, pathname) {
    if (pathname === '/api/state' && req.method === 'GET') {
      sendJson(res, 200, { state: service.getState() });
      return true;
    }

    if (pathname === '/api/state/snapshot' && req.method === 'GET') {
      const generatedAt = new Date().toISOString();
      const state = service.getState();
      const manifest = state?.manifest || {};
      const payload = {
        generatedAt,
        runtime: {
          pid: process.pid,
          uptimeSeconds: Math.floor(process.uptime()),
          node: process.version,
          platform: process.platform,
        },
        powerSafety: {
          power_profile: manifest.power_profile ?? null,
          led_brightness_cap: manifest.led_brightness_cap ?? null,
          rail_topology_verified: manifest.rail_topology_verified ?? null,
        },
        state,
      };
      res.writeHead(200, {
        'cache-control': 'no-store',
        'content-type': 'application/json; charset=utf-8',
        'content-disposition': `attachment; filename="mn42-bridge-state-${generatedAt.replace(
          /[:.]/g,
          '-',
        )}.json"`,
      });
      res.end(JSON.stringify(payload, null, 2));
      return true;
    }

    if (pathname === '/api/ports' && req.method === 'GET') {
      try {
        const ports = await service.listSerialPorts();
        sendJson(res, 200, { ports: simplifyPorts(ports) });
      } catch (err) {
        sendJson(res, 500, { error: err.message });
      }
      return true;
    }

    if (pathname === '/api/midi-ports' && req.method === 'GET') {
      try {
        const ports =
          typeof service.listMidiPorts === 'function'
            ? await service.listMidiPorts()
            : { inputs: [], outputs: [] };
        sendJson(res, 200, {
          inputs: simplifyMidiPorts(ports.inputs),
          outputs: simplifyMidiPorts(ports.outputs),
        });
      } catch (err) {
        sendJson(res, 500, { error: err.message });
      }
      return true;
    }

    if (pathname === '/api/connect' && req.method === 'POST') {
      try {
        const body = await readJson(req);
        const nextConfig = normalizeConfig(body);
        const running = Boolean(service.getState().running);
        await service.configure(nextConfig, { restart: running });
        if (!running) {
          await service.start();
        }
        sendJson(res, 200, { state: service.getState() });
      } catch (err) {
        sendJson(res, 500, { error: err.message, state: service.getState() });
      }
      return true;
    }

    if (pathname === '/api/disconnect' && req.method === 'POST') {
      try {
        await service.stop();
        sendJson(res, 200, { state: service.getState() });
      } catch (err) {
        sendJson(res, 500, { error: err.message, state: service.getState() });
      }
      return true;
    }

    if (pathname === '/api/performance/reset' && req.method === 'POST') {
      try {
        if (typeof service.resetPerformance === 'function') {
          await service.resetPerformance();
        }
        sendJson(res, 200, { state: service.getState() });
      } catch (err) {
        sendJson(res, 500, { error: err.message, state: service.getState() });
      }
      return true;
    }

    if (pathname === '/api/alerts/clear' && req.method === 'POST') {
      try {
        if (typeof service.clearAlerts === 'function') {
          await service.clearAlerts();
        }
        sendJson(res, 200, { state: service.getState() });
      } catch (err) {
        sendJson(res, 500, { error: err.message, state: service.getState() });
      }
      return true;
    }

    res.writeHead(404, { 'content-type': 'application/json; charset=utf-8' });
    res.end(JSON.stringify({ error: 'not found' }));
    return true;
  }

  // Upgrade `/ws` requests into a raw websocket feed for serial lines and state updates.
  function handleWebSocket(req, socket) {
    const url = new URL(req.url, `http://${req.headers.host || '127.0.0.1'}`);
    if (url.pathname !== '/ws') {
      socket.destroy();
      return;
    }

    const key = req.headers['sec-websocket-key'];
    if (!key) {
      socket.destroy();
      return;
    }

    const accept = crypto
      .createHash('sha1')
      .update(`${key}258EAFA5-E914-47DA-95CA-C5AB0DC85B11`)
      .digest('base64');

    socket.write(
      [
        'HTTP/1.1 101 Switching Protocols',
        'Upgrade: websocket',
        'Connection: Upgrade',
        `Sec-WebSocket-Accept: ${accept}`,
        '',
        '',
      ].join('\r\n'),
    );

    sockets.add(socket);
    let buffered = Buffer.alloc(0);

    socket.on('data', (chunk) => {
      buffered = Buffer.concat([buffered, chunk]);
      while (buffered.length >= 2) {
        const first = buffered[0];
        const second = buffered[1];
        const opcode = first & 0x0f;
        const masked = (second & 0x80) !== 0;
        let offset = 2;
        let length = second & 0x7f;
        if (length === 126) {
          if (buffered.length < 4) return;
          length = buffered.readUInt16BE(2);
          offset = 4;
        } else if (length === 127) {
          destroySocket(socket);
          return;
        }
        const maskLength = masked ? 4 : 0;
        if (buffered.length < offset + maskLength + length) return;
        const mask = masked ? buffered.subarray(offset, offset + 4) : null;
        offset += maskLength;
        const payload = Buffer.from(buffered.subarray(offset, offset + length));
        buffered = buffered.subarray(offset + length);

        if (opcode === 0x8) {
          destroySocket(socket);
          return;
        }
        if (opcode === 0x9) {
          socket.write(Buffer.from([0x8a, 0x00]));
          continue;
        }
        if (opcode !== 0x1) continue;
        if (mask) {
          for (let i = 0; i < payload.length; i += 1) {
            payload[i] ^= mask[i % 4];
          }
        }
        const text = payload.toString('utf8');
        const lines = text.split('\n');
        for (const rawLine of lines) {
          const line = rawLine.trim();
          if (!line) continue;
          try {
            service.sendLine(line);
          } catch (_) {
            // The UI can query /api/state for transport health; raw transport stays line-oriented.
          }
        }
      }
    });

    socket.on('close', () => sockets.delete(socket));
    socket.on('end', () => sockets.delete(socket));
    socket.on('error', () => sockets.delete(socket));
  }

  async function start() {
    if (server) return { host, port: server.address().port };

    unsubscribeLine = service.on('line', broadcastLine);
    unsubscribeState = service.on('state', (state) => {
      if (state && state.running) return;
      for (const socket of [...sockets]) {
        destroySocket(socket);
      }
    });
    server = httpApi.createServer(async (req, res) => {
      const url = new URL(
        req.url,
        `http://${req.headers.host || `${host}:${port}`}`,
      );
      const pathname = url.pathname;

      if (pathname.startsWith('/api/')) {
        await handleApi(req, res, pathname);
        return;
      }

      if (pathname === '/app') {
        res.writeHead(302, { location: '/app/' });
        res.end();
        return;
      }

      if (pathname === '/' || pathname === '/index.html') {
        await serveFile(res, uiDir, '/index.html');
        return;
      }

      if (pathname.startsWith('/app/')) {
        const requestPath =
          pathname === '/app/' ? '/index.html' : pathname.slice('/app'.length);
        await serveFile(res, appDir, requestPath);
        return;
      }

      await serveFile(res, uiDir, pathname);
    });

    server.on('upgrade', (req, socket) => handleWebSocket(req, socket));

    await new Promise((resolve, reject) => {
      server.once('error', reject);
      server.listen(port, host, () => {
        server.off('error', reject);
        resolve();
      });
    });

    return {
      host,
      port: server.address().port,
      url: `http://${host}:${server.address().port}/`,
    };
  }

  async function stop() {
    if (unsubscribeLine) {
      unsubscribeLine();
      unsubscribeLine = null;
    }
    if (unsubscribeState) {
      unsubscribeState();
      unsubscribeState = null;
    }
    for (const socket of [...sockets]) {
      destroySocket(socket);
    }
    if (!server) return;
    const activeServer = server;
    server = null;
    await new Promise((resolve, reject) => {
      activeServer.close((err) => (err ? reject(err) : resolve()));
    });
  }

  return {
    start,
    stop,
  };
}

module.exports = {
  createBrowserBridgeServer,
};
