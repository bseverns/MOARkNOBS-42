#!/usr/bin/env node

const { once } = require('node:events');
const net = require('node:net');
const path = require('node:path');
const { spawn } = require('node:child_process');

function getArg(flag, fallback = '') {
  const index = process.argv.indexOf(flag);
  if (index === -1 || index + 1 >= process.argv.length) return fallback;
  return String(process.argv[index + 1] || '').trim();
}

function resolveBinaryPath() {
  const explicit = getArg('--binary');
  if (explicit) return path.resolve(explicit);
  return path.resolve(__dirname, '..', 'mn42_bridge_server.js');
}

async function requestJson(url, init = {}) {
  const response = await fetch(url, init);
  let payload = null;
  try {
    payload = await response.json();
  } catch (_) {
    payload = null;
  }
  return { response, payload };
}

async function reservePort() {
  const server = net.createServer();
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  const address = server.address();
  const port = Number(address?.port || 0);
  await new Promise((resolve, reject) => {
    server.close((error) => (error ? reject(error) : resolve()));
  });
  if (!port) {
    throw new Error('failed to reserve a free localhost port');
  }
  return port;
}

async function waitForServerReady(baseUrl, timeoutMs = 8000) {
  const startedAt = Date.now();
  while (Date.now() - startedAt < timeoutMs) {
    try {
      const response = await fetch(`${baseUrl}/api/state`);
      if (response.ok) {
        return response;
      }
    } catch (_) {
      // server is still booting
    }
    await new Promise((resolve) => setTimeout(resolve, 120));
  }
  throw new Error(`server did not become ready within ${timeoutMs}ms`);
}

async function waitForSessionWarmState(baseUrl, timeoutMs = 8000) {
  const startedAt = Date.now();
  let lastPayload = null;
  while (Date.now() - startedAt < timeoutMs) {
    const { response, payload } = await requestJson(
      `${baseUrl}/api/device/session?warm=1`,
    );
    if (!response.ok) {
      await new Promise((resolve) => setTimeout(resolve, 120));
      continue;
    }
    lastPayload = payload;
    const session = payload?.session ?? {};
    if (session.manifest && session.liveConfig) {
      return payload;
    }
    await new Promise((resolve) => setTimeout(resolve, 120));
  }
  return lastPayload;
}

function assert(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}

async function main() {
  const binaryPath = resolveBinaryPath();
  const serialPort = getArg('--serial');
  const port = await reservePort();
  const host = '127.0.0.1';
  const baseUrl = `http://${host}:${port}`;
  const launchArgs = [
    '--no-open-browser',
    '--http-host',
    host,
    '--http-port',
    String(port),
  ];
  if (serialPort) {
    launchArgs.push('--serial', serialPort);
  }
  const launchCommand = binaryPath.endsWith('.js')
    ? process.execPath
    : binaryPath;
  const launchCommandArgs = binaryPath.endsWith('.js')
    ? [binaryPath, ...launchArgs]
    : launchArgs;
  const child = spawn(launchCommand, launchCommandArgs, {
    stdio: ['ignore', 'pipe', 'pipe'],
  });

  let stdout = '';
  let stderr = '';
  let cleanupError = null;
  child.stdout?.on('data', (chunk) => {
    stdout += chunk.toString();
  });
  child.stderr?.on('data', (chunk) => {
    stderr += chunk.toString();
  });

  try {
    await waitForServerReady(baseUrl);

    if (serialPort) {
      const { response: connectResponse, payload: connectPayload } =
        await requestJson(`${baseUrl}/api/connect`, {
          method: 'POST',
          headers: { 'content-type': 'application/json' },
          body: JSON.stringify({ serialName: serialPort }),
        });
      assert(connectResponse.ok, 'serial-backed smoke connect must return 200');
      assert(
        Boolean(connectPayload?.state?.running),
        'serial-backed smoke connect must start the bridge runtime',
      );
    }

    const rootResponse = await fetch(`${baseUrl}/`);
    assert(rootResponse.ok, 'console root must return 200');
    const rootHtml = await rootResponse.text();
    assert(
      rootHtml.includes('Bridge command center'),
      'console root must include browser console content',
    );

    const appResponse = await fetch(`${baseUrl}/app/`);
    assert(appResponse.ok, '/app/ must return 200');
    const appHtml = await appResponse.text();
    assert(
      appHtml.includes('MOARkNOBS-42 Browser Configurator'),
      '/app/ must include the packaged App shell',
    );

    const presetsResponse = await fetch(`${baseUrl}/api/presets`);
    assert(presetsResponse.ok, '/api/presets must return 200');
    const presetsPayload = await presetsResponse.json();
    assert(
      Array.isArray(presetsPayload.presets) &&
        presetsPayload.presets.length >= 4,
      '/api/presets must expose packaged known-good recipes',
    );

    const sessionPayload = serialPort
      ? await waitForSessionWarmState(baseUrl)
      : (await requestJson(`${baseUrl}/api/device/session?warm=1`)).payload;
    assert(sessionPayload, '/api/device/session?warm=1 must return JSON');
    assert(
      sessionPayload?.session?.schema &&
        sessionPayload.session.schema.type === 'object',
      'warmed device session must expose bundled schema authority',
    );
    assert(
      typeof sessionPayload?.session?.schemaSource === 'string',
      'warmed device session must report a schema source',
    );

    const { response: invalidStageResponse, payload: invalidStagePayload } =
      await requestJson(`${baseUrl}/api/device/stage`, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({
          config: { slots: 'not-an-array' },
        }),
      });
    assert(
      !invalidStageResponse.ok,
      '/api/device/stage must reject invalid or premature staged config writes',
    );
    assert(
      typeof invalidStagePayload?.error?.code === 'string' &&
        invalidStagePayload.error.code.length > 0,
      '/api/device/stage rejection must expose a machine-readable error code',
    );
    assert(
      typeof invalidStagePayload?.error?.message === 'string' &&
        invalidStagePayload.error.message.length > 0,
      '/api/device/stage rejection must expose a machine-readable error message',
    );

    const warmedSession = sessionPayload?.session ?? {};
    const canStageValidConfig = Boolean(
      warmedSession.manifest && warmedSession.liveConfig,
    );
    if (canStageValidConfig) {
      const validConfig = JSON.parse(
        JSON.stringify(warmedSession.stagedConfig ?? warmedSession.liveConfig),
      );
      const nextFreq = Number(validConfig?.filter?.freq);
      validConfig.filter = {
        ...(validConfig.filter || {}),
        freq: Number.isFinite(nextFreq) ? nextFreq + 1 : 801,
      };
      const { response: validStageResponse, payload: validStagePayload } =
        await requestJson(`${baseUrl}/api/device/stage`, {
          method: 'POST',
          headers: { 'content-type': 'application/json' },
          body: JSON.stringify({ config: validConfig }),
        });
      assert(
        validStageResponse.ok,
        'hardware-backed packaged smoke stage must accept a valid staged config',
      );
      assert(
        typeof validStagePayload?.result === 'object' &&
          validStagePayload.result !== null,
        'hardware-backed packaged smoke stage must return a structured result',
      );
      await requestJson(`${baseUrl}/api/device/rollback`, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({ reason: 'artifact_smoke_cleanup' }),
      });
    } else {
      console.log(
        'packaged bridge smoke note: valid /api/device/stage acceptance remains HIL-only because warm=1 loads schema authority but does not synthesize a device manifest/live config',
      );
    }

    console.log('packaged bridge server smoke checks passed');
  } finally {
    child.kill('SIGTERM');
    await Promise.race([
      once(child, 'exit'),
      new Promise((resolve) => setTimeout(resolve, 3000)),
    ]);
    if (
      child.exitCode &&
      child.exitCode !== 0 &&
      !stdout.includes('packaged bridge server smoke checks passed')
    ) {
      cleanupError = new Error(
        `packaged bridge server exited with code ${child.exitCode}\nstdout:\n${stdout}\nstderr:\n${stderr}`,
      );
    }
  }
  if (cleanupError) {
    throw cleanupError;
  }
}

main().catch((error) => {
  console.error(error.message || error);
  process.exit(1);
});
