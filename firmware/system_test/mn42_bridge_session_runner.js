#!/usr/bin/env node
/*
 * Hardware-in-the-loop bridge session proof for the upgraded browser-console
 * lane. This runner exercises the bridge the way the App now prefers to use
 * it: start the HTTP server, connect the service over USB serial, wait for the
 * cached device session to become ready, then prove stage/apply/cleanup over
 * `/api/device/*` while watching `/ws/events`.
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
  const wsUrl = `ws://${host}:${port}/ws/events`;
  const scenarios = [];
  const structuredEvents = [];
  const serverStdout = [];
  const serverStderr = [];
  const startedAt = Date.now();

  const server = spawn('node', serverArgs, {
    stdio: ['ignore', 'pipe', 'pipe'],
  });

  let serverReady = false;
  let serverExitCode = null;
  server.stdout.on('data', (chunk) => {
    const text = chunk.toString();
    serverStdout.push(text);
    process.stdout.write(`[bridge-session:server] ${text}`);
    if (text.includes('bridge console:')) {
      serverReady = true;
    }
  });
  server.stderr.on('data', (chunk) => {
    const text = chunk.toString();
    serverStderr.push(text);
    process.stderr.write(`[bridge-session:server:err] ${text}`);
  });
  server.once('exit', (code, signal) => {
    serverExitCode = code === null ? signal : code;
  });

  let ws = null;
  let baselineConfig = null;
  let mutatedConfig = null;

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
    await waitFor(() => serverReady, timeoutMs, 'bridge server startup');
    scenarios.push({
      id: 'server-start',
      title: 'Bridge server starts and exposes the console address',
      ok: true,
      detail: baseUrl,
    });

    ws = new WebSocket(wsUrl);
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

    const connectPayload = await fetchJson(`${baseUrl}/api/connect`, {
      method: 'POST',
      body: JSON.stringify({
        serialName: serialPath,
        oscPort: oscOutPort,
        oscListen: oscInPort,
        oscHost: '127.0.0.1',
        oscBind: '127.0.0.1',
        midiLabel,
      }),
    });
    if (!connectPayload?.state?.running) {
      throw new Error('Bridge service did not enter running state after /api/connect');
    }

    const sessionPayload = await waitFor(async () => {
      const payload = await fetchJson(`${baseUrl}/api/device/session`, {
        method: 'GET',
      });
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

    const stagePayload = await fetchJson(`${baseUrl}/api/device/stage`, {
      method: 'POST',
      body: JSON.stringify({ config: mutatedConfig }),
    });
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

    const applyPayload = await fetchJson(`${baseUrl}/api/device/apply`, {
      method: 'POST',
      body: JSON.stringify({}),
    });
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

    const cleanupStage = await fetchJson(`${baseUrl}/api/device/stage`, {
      method: 'POST',
      body: JSON.stringify({ config: baselineConfig }),
    });
    if (!cleanupStage?.result?.dirty) {
      throw new Error('Cleanup stage did not mark the bridge session dirty');
    }

    const cleanupApply = await fetchJson(`${baseUrl}/api/device/apply`, {
      method: 'POST',
      body: JSON.stringify({}),
    });
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

main().catch((error) => {
  console.error(`[bridge-session] fatal: ${error.message}`);
  process.exit(1);
});
