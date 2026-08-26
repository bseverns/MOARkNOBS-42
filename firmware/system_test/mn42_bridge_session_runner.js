#!/usr/bin/env node
/*
Hardware-in-the-loop bridge session proof for the upgraded browser-console
lane. This runner exercises the bridge the way the App now prefers to use
it: start the HTTP server, connect the service over USB serial, wait for the
cached device session to become ready, then prove stage/apply/cleanup over
`/api/device/*` while watching `/ws/events`.
*/

'use strict';

const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

const {
  validateStructuredEventShape,
} = require(path.resolve(
  __dirname,
  '../../bridge/lib/device/transport_contract',
));

const args = process.argv.slice(2);

function argValue(flag, fallback) {
  const index = args.indexOf(flag);
  if (index >= 0 && index + 1 < args.length) {
    return args[index + 1];
  }
  return fallback;
}

function normalizePositiveInt(value, fallback) {
  const parsed = parseInt(String(value), 10);
  return Number.isInteger(parsed) && parsed > 0 ? parsed : fallback;
}

function clone(value) {
  return value == null ? value : JSON.parse(JSON.stringify(value));
}

function parseBridgeConsoleUrl(text) {
  const match = String(text || '').match(/bridge console:\s+(https?:\/\/\S+)/);
  if (!match) return null;
  try {
    return new URL(match[1]);
  } catch (_) {
    return null;
  }
}

function redactControlToken(text) {
  return String(text || '').replace(/([?&]token=)[^\s]+/g, '$1[redacted]');
}

function buildStructuredWebSocketUrl(consoleUrl) {
  const target = new URL('/ws/events', consoleUrl);
  target.protocol = target.protocol === 'https:' ? 'wss:' : 'ws:';
  const token = consoleUrl.searchParams.get('token');
  if (token) target.searchParams.set('token', token);
  return target.toString();
}

function withControlToken(options = {}, controlToken) {
  return {
    ...options,
    headers: {
      ...(options.headers || {}),
      ...(controlToken ? { authorization: `Bearer ${controlToken}` } : {}),
    },
  };
}

function configDifferencePaths(left, right, pathLabel = '$', output = []) {
  if (output.length >= 32 || Object.is(left, right)) return output;
  if (Array.isArray(left) && Array.isArray(right)) {
    const length = Math.max(left.length, right.length);
    for (let index = 0; index < length && output.length < 32; index += 1) {
      configDifferencePaths(left[index], right[index], `${pathLabel}[${index}]`, output);
    }
    return output;
  }
  if (
    left && right &&
    typeof left === 'object' && typeof right === 'object' &&
    !Array.isArray(left) && !Array.isArray(right)
  ) {
    const keys = new Set([...Object.keys(left), ...Object.keys(right)]);
    for (const key of [...keys].sort()) {
      if (output.length >= 32) break;
      configDifferencePaths(left[key], right[key], `${pathLabel}.${key}`, output);
    }
    return output;
  }
  output.push(pathLabel);
  return output;
}

function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function waitFor(predicate, timeoutMs, description) {
  const startedAt = Date.now();
  while (Date.now() - startedAt < timeoutMs) {
    const result = await predicate();
    if (result) return result;
    await delay(100);
  }
  throw new Error(`Timeout waiting for ${description}`);
}

async function fetchJson(url, options = {}) {
  const response = await fetch(url, {
    ...options,
    headers: {
      'content-type': 'application/json',
      ...(options.headers || {}),
    },
  });
  const text = await response.text();
  let payload = {};
  try {
    payload = text ? JSON.parse(text) : {};
  } catch (error) {
    throw new Error(`Invalid JSON from ${url}: ${error.message}`);
  }
  if (!response.ok) {
    const message =
      payload?.error?.message || payload?.error || `${response.status} ${response.statusText}`;
    const err = new Error(message);
    err.status = response.status;
    err.payload = payload;
    throw err;
  }
  return payload;
}

async function main() {
  if (typeof WebSocket !== 'function') {
    throw new Error('Node runtime does not expose WebSocket; Node 24 is required');
  }

  const serialPath = argValue(
    '--serial',
    process.env.MN42_SERIAL || process.env.TEST_PORT || '',
  );
  if (!serialPath) {
    throw new Error('Missing serial port. Pass --serial or set MN42_SERIAL/TEST_PORT.');
  }

  const host = argValue('--http-host', process.env.MN42_HTTP_HOST || '127.0.0.1');
  const port = normalizePositiveInt(
    argValue('--http-port', process.env.MN42_HTTP_PORT || '8791'),
    8791,
  );
  const oscOutPort = normalizePositiveInt(
    argValue('--osc-out', process.env.MN42_OSC_OUT || '10020'),
    10020,
  );
  const oscInPort = normalizePositiveInt(
    argValue('--osc-in', process.env.MN42_OSC_IN || '10021'),
    10021,
  );
  const midiLabel = argValue('--midi', process.env.MN42_MIDI || 'Teensy MIDI');
  const timeoutMs = normalizePositiveInt(
    argValue('--timeout', process.env.MN42_SESSION_TIMEOUT || '15000'),
    15000,
  );
  const reportPath = argValue(
    '--report',
    process.env.MN42_REPORT || 'logs/bridge-session-test.json',
  );

  const serverPath = path.resolve(__dirname, '../../bridge/mn42_bridge_server.js');
  const serverArgs = [
    serverPath,
    '--http-host',
    host,
    '--http-port',
    String(port),
    '--serial',
    serialPath,
    '--osc',
    String(oscOutPort),
    '--osc-listen',
    String(oscInPort),
    '--host',
    '127.0.0.1',
    '--bind',
    '127.0.0.1',
    '--midi',
    midiLabel,
  ];

  const baseUrl = `http://${host}:${port}`;
  const scenarios = [];
  const structuredEvents = [];
  const serverStdout = [];
  const serverStderr = [];
  const startedAt = Date.now();

  const server = spawn('node', serverArgs, {
    stdio: ['ignore', 'pipe', 'pipe'],
  });

  let bridgeConsoleUrl = null;
  let controlToken = null;
  let serverStdoutBuffer = '';
  let serverExitCode = null;
  server.stdout.on('data', (chunk) => {
    const text = chunk.toString();
    serverStdout.push(text);
    serverStdoutBuffer += text;
    const lines = serverStdoutBuffer.split('\n');
    serverStdoutBuffer = lines.pop() || '';
    for (const line of lines) {
      process.stdout.write(`[bridge-session:server] ${redactControlToken(line)}\n`);
      const parsedUrl = parseBridgeConsoleUrl(line);
      if (parsedUrl) {
        bridgeConsoleUrl = parsedUrl;
        controlToken = parsedUrl.searchParams.get('token');
      }
    }
  });
  server.stderr.on('data', (chunk) => {
    const text = chunk.toString();
    serverStderr.push(text);
    process.stderr.write(`[bridge-session:server:err] ${text}`);
  });
  server.once('exit', (code, signal) => {
    if (serverStdoutBuffer) {
      process.stdout.write(
        `[bridge-session:server] ${redactControlToken(serverStdoutBuffer)}\n`,
      );
      serverStdoutBuffer = '';
    }
    serverExitCode = code === null ? signal : code;
  });

  let ws = null;
  let baselineConfig = null;
  let mutatedConfig = null;
  let lastSession = null;
  let failureDetails = null;

  async function cleanup() {
    if (ws) {
      try {
        ws.close();
      } catch (_) {}
      ws = null;
    }
    if (server && server.exitCode === null && server.signalCode === null) {
      server.kill('SIGINT');
      await delay(500);
    }
  }

  try {
    await waitFor(() => bridgeConsoleUrl, timeoutMs, 'bridge server startup');
    if (!controlToken) {
      throw new Error('Bridge server did not expose a control token');
    }
    scenarios.push({
      id: 'server-start',
      title: 'Bridge server starts and exposes the console address',
      ok: true,
      detail: baseUrl,
    });

    ws = new WebSocket(buildStructuredWebSocketUrl(bridgeConsoleUrl));
    ws.addEventListener('message', (event) => {
      const text = String(event.data || '').trim();
      if (!text) return;
      for (const line of text.split('\n')) {
        if (!line.trim()) continue;
        try {
          structuredEvents.push(JSON.parse(line));
        } catch (_) {}
      }
    });
    await new Promise((resolve, reject) => {
      const timer = setTimeout(
        () => reject(new Error('Timeout waiting for /ws/events socket')),
        5000,
      );
      ws.addEventListener('open', () => {
        clearTimeout(timer);
        resolve();
      });
      ws.addEventListener('error', (event) => {
        clearTimeout(timer);
        reject(new Error(event?.message || 'Structured websocket failed'));
      });
    });

    const connectPayload = await fetchJson(`${baseUrl}/api/connect`, withControlToken({
      method: 'POST',
      body: JSON.stringify({
        serialName: serialPath,
        oscPort: oscOutPort,
        oscListen: oscInPort,
        oscHost: '127.0.0.1',
        oscBind: '127.0.0.1',
        midiLabel,
      }),
    }, controlToken));
    if (!connectPayload?.state?.running) {
      throw new Error('Bridge service did not enter running state after /api/connect');
    }

    const sessionPayload = await waitFor(async () => {
      const payload = await fetchJson(`${baseUrl}/api/device/session`, {
        method: 'GET',
      });
      lastSession = clone(payload?.session ?? null);
      return payload?.session?.ready ? payload : null;
    }, timeoutMs, 'device session readiness');

    baselineConfig = clone(sessionPayload.session.liveConfig);
    if (!baselineConfig?.filter) {
      throw new Error('Device session did not expose a filter block in liveConfig');
    }

    scenarios.push({
      id: 'session-ready',
      title: 'Structured bridge session becomes ready on hardware',
      ok: true,
      detail: `schemaSource=${sessionPayload.session.schemaSource} fw=${sessionPayload.session.firmwareIdentity?.fw_version || 'unknown'}`,
    });

    await waitFor(
      () =>
        structuredEvents.find((event) => {
          const errors = validateStructuredEventShape(event);
          return !errors.length && event.event === 'device.ready';
        }),
      timeoutMs,
      'device.ready event on /ws/events',
    );
    scenarios.push({
      id: 'ready-event',
      title: 'Structured websocket emits device.ready',
      ok: true,
      detail: 'device.ready observed on /ws/events',
    });

    mutatedConfig = clone(baselineConfig);
    const baselineIdleFloor = Number.isInteger(mutatedConfig.filter.idle_floor)
      ? mutatedConfig.filter.idle_floor
      : 24;
    const nextIdleFloor = baselineIdleFloor >= 127 ? baselineIdleFloor - 1 : baselineIdleFloor + 1;
    mutatedConfig.filter.idle_floor = nextIdleFloor;

    const stagePayload = await fetchJson(`${baseUrl}/api/device/stage`, withControlToken({
      method: 'POST',
      body: JSON.stringify({ config: mutatedConfig }),
    }, controlToken));
    if (!stagePayload?.result?.dirty) {
      throw new Error('Stage call did not mark the bridge session dirty');
    }
    scenarios.push({
      id: 'stage',
      title: 'Structured stage endpoint accepts a live device config',
      ok: true,
      detail: `idle_floor ${baselineIdleFloor} -> ${nextIdleFloor}`,
    });

    await waitFor(
      () =>
        structuredEvents.find(
          (event) =>
            event.event === 'device.config.dirty' &&
            event.payload?.dirty === true,
        ),
      timeoutMs,
      'device.config.dirty event on /ws/events',
    );

    const applyPayload = await fetchJson(`${baseUrl}/api/device/apply`, withControlToken({
      method: 'POST',
      body: JSON.stringify({}),
    }, controlToken));
    if (!applyPayload?.result?.applied || !applyPayload?.result?.checksum) {
      throw new Error('Apply did not return an ACK/checksum result');
    }

    const appliedSession = await waitFor(async () => {
      const payload = await fetchJson(`${baseUrl}/api/device/session`, {
        method: 'GET',
      });
      return payload?.session?.liveConfig?.filter?.idle_floor === nextIdleFloor
        ? payload
        : null;
    }, timeoutMs, 'applied live config readback');

    scenarios.push({
      id: 'apply',
      title: 'Structured apply returns ACK and promotes staged config',
      ok: true,
      detail: `checksum=${applyPayload.result.checksum}`,
    });

    await waitFor(
      () =>
        structuredEvents.find(
          (event) =>
            event.event === 'device.apply.ack' &&
            event.payload?.checksum === applyPayload.result.checksum,
        ),
      timeoutMs,
      'device.apply.ack event on /ws/events',
    );

    const cleanupStage = await fetchJson(`${baseUrl}/api/device/stage`, withControlToken({
      method: 'POST',
      body: JSON.stringify({ config: baselineConfig }),
    }, controlToken));
    if (!cleanupStage?.result?.dirty) {
      throw new Error('Cleanup stage did not mark the bridge session dirty');
    }

    const cleanupApply = await fetchJson(`${baseUrl}/api/device/apply`, withControlToken({
      method: 'POST',
      body: JSON.stringify({}),
    }, controlToken));
    if (!cleanupApply?.result?.applied) {
      throw new Error('Cleanup apply did not return success');
    }

    await waitFor(async () => {
      const payload = await fetchJson(`${baseUrl}/api/device/session`, {
        method: 'GET',
      });
      return payload?.session?.liveConfig?.filter?.idle_floor === baselineIdleFloor
        ? payload
        : null;
    }, timeoutMs, 'cleanup config readback');

    scenarios.push({
      id: 'cleanup',
      title: 'Cleanup apply restores the original live config',
      ok: true,
      detail: `idle_floor restored to ${baselineIdleFloor}`,
    });

    if (
      appliedSession?.session?.lastApplyResult?.status &&
      appliedSession.session.lastApplyResult.status !== 'ack'
    ) {
      throw new Error(
        `Unexpected apply status: ${appliedSession.session.lastApplyResult.status}`,
      );
    }
  } catch (error) {
    scenarios.push({
      id: `error-${scenarios.length + 1}`,
      title: 'Scenario failure',
      ok: false,
      detail: error.message,
    });
    console.error(`[bridge-session] Scenario failed: ${error.message}`);
    failureDetails = { originalError: error.message };
    try {
      const payload = await fetchJson(`${baseUrl}/api/device/session`, {
        method: 'GET',
      });
      lastSession = clone(payload?.session ?? null);
      if (mutatedConfig && lastSession?.liveConfig) {
        failureDetails.readbackDifferencePaths = configDifferencePaths(
          lastSession.liveConfig,
          mutatedConfig,
        );
      }
    } catch (diagnosticError) {
      failureDetails.sessionReadError = diagnosticError.message;
    }

    if (baselineConfig) {
      const baselineIdleFloor = baselineConfig.filter?.idle_floor;
      let cleanupApplyError = null;
      try {
        const stage = await fetchJson(
          `${baseUrl}/api/device/stage`,
          withControlToken({
            method: 'POST',
            body: JSON.stringify({ config: baselineConfig }),
          }, controlToken),
        );
        if (stage?.result?.dirty) {
          try {
            await fetchJson(
              `${baseUrl}/api/device/apply`,
              withControlToken({
                method: 'POST',
                body: JSON.stringify({}),
              }, controlToken),
            );
          } catch (cleanupError) {
            cleanupApplyError = cleanupError.message;
          }
        }
        await waitFor(async () => {
          const payload = await fetchJson(`${baseUrl}/api/device/session`, {
            method: 'GET',
          });
          lastSession = clone(payload?.session ?? null);
          return lastSession?.liveConfig?.filter?.idle_floor === baselineIdleFloor;
        }, timeoutMs, 'failure cleanup config readback');
        failureDetails.cleanupApplyError = cleanupApplyError;
        failureDetails.cleanupRestoredIdleFloor = baselineIdleFloor;
        scenarios.push({
          id: 'cleanup-after-failure',
          title: 'Failure cleanup restores the original live config target',
          ok: true,
          detail: `idle_floor restored to ${baselineIdleFloor}`,
        });
      } catch (cleanupError) {
        failureDetails.cleanupError = cleanupError.message;
        scenarios.push({
          id: 'cleanup-after-failure',
          title: 'Failure cleanup restores the original live config target',
          ok: false,
          detail: cleanupError.message,
        });
      }
    }
  } finally {
    await cleanup();
    const report = {
      serial: serialPath,
      httpHost: host,
      httpPort: port,
      oscOutPort,
      oscInPort,
      midiLabel,
      scenarios,
      structuredEventNames: structuredEvents.map((entry) => entry.event),
      failureDiagnostics: scenarios.some((entry) => !entry.ok)
        ? {
            session: lastSession,
            details: failureDetails,
            alerts: structuredEvents
              .filter((entry) => entry.event === 'bridge.alert')
              .map((entry) => clone(entry.payload)),
          }
        : null,
      serverExitCode,
      durationMs: Date.now() - startedAt,
    };
    fs.mkdirSync(path.dirname(reportPath), { recursive: true });
    fs.writeFileSync(reportPath, `${JSON.stringify(report, null, 2)}\n`);
    console.log(`[bridge-session] wrote report to ${reportPath}`);
    for (const scenario of scenarios) {
      const prefix = scenario.ok ? 'PASS' : 'FAIL';
      console.log(
        `[bridge-session] ${prefix} – ${scenario.title}: ${scenario.detail}`,
      );
    }
    if (scenarios.some((entry) => !entry.ok)) {
      process.exit(1);
    }
  }
}

if (require.main === module) {
  main().catch((error) => {
    console.error(`[bridge-session] fatal: ${error.message}`);
    process.exit(1);
  });
}

module.exports = {
  buildStructuredWebSocketUrl,
  parseBridgeConsoleUrl,
  redactControlToken,
  withControlToken,
  configDifferencePaths,
};
