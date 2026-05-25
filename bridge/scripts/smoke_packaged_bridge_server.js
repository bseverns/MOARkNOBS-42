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

function assert(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}

async function main() {
  const binaryPath = resolveBinaryPath();
  const port = await reservePort();
  const host = '127.0.0.1';
  const baseUrl = `http://${host}:${port}`;
  const launchArgs = ['--http-host', host, '--http-port', String(port)];
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

    const rootResponse = await fetch(`${baseUrl}/`);
    assert(rootResponse.ok, 'console root must return 200');
    const rootHtml = await rootResponse.text();
    assert(
      rootHtml.includes('Desktop session runtime'),
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

    const sessionResponse = await fetch(`${baseUrl}/api/device/session?warm=1`);
    assert(sessionResponse.ok, '/api/device/session?warm=1 must return 200');
    const sessionPayload = await sessionResponse.json();
    assert(
      sessionPayload?.session?.schema &&
        sessionPayload.session.schema.type === 'object',
      'warmed device session must expose bundled schema authority',
    );
    assert(
      sessionPayload?.session?.schemaSource === 'bundled',
      'warmed device session must report bundled schema source',
    );

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
