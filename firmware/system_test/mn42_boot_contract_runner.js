#!/usr/bin/env node
/*
 * Hardware proof for the exact teensy40_main boot/configurator contract.
 *
 * This runner targets the production firmware image, not the bridge/system
 * demo lane. It proves:
 *  - standalone boot banner + boot_mode marker
 *  - direct serial HELLO while standalone runtime is alive
 *  - ENTER_CONFIG_MODE reboot handoff
 *  - configurator-mode HELLO -> manifest -> schema -> config hydrate
 *  - one small staged SET_ALL apply with matching ACK/checksum
 *  - cleanup back to the original config
 */

'use strict';

const { spawn } = require('child_process');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');

const { SerialPort, ReadlineParser } = require(path.resolve(
  __dirname,
  '../../bridge/node_modules/serialport',
));

const args = process.argv.slice(2);

function argValue(flag, fallback) {
  const index = args.indexOf(flag);
  if (index >= 0 && index + 1 < args.length) {
    return args[index + 1];
  }
  return fallback;
}

function hasFlag(flag) {
  return args.includes(flag);
}

function detectPort() {
  const envPort = process.env.MN42_SERIAL || process.env.TEST_PORT;
  if (envPort) return envPort;

  const preferredPrefixes = [
    '/dev/cu.usbmodem',
    '/dev/cu.usbserial',
    '/dev/ttyACM',
    '/dev/ttyUSB',
    '/dev/tty.usbmodem',
    '/dev/tty.usbserial'
  ];

  try {
    const devEntries = fs.readdirSync('/dev').map((entry) => `/dev/${entry}`);
    for (const prefix of preferredPrefixes) {
      const match = devEntries.find((entry) => entry.startsWith(prefix));
      if (match) return match;
    }
  } catch (_) {
    // fall through to null
  }
  return null;
}

const serialPath = argValue('--serial', detectPort());
const reportPath = argValue('--report', process.env.MN42_BOOT_REPORT || '');
const firmwareEnv = argValue('--firmware-env', process.env.MN42_FIRMWARE_ENV || 'teensy40_main');
const bootTimeoutMs = parseInt(argValue('--boot-timeout', process.env.MN42_BOOT_TIMEOUT || '12000'), 10);
const commandTimeoutMs = parseInt(
  argValue('--command-timeout', process.env.MN42_COMMAND_TIMEOUT || '5000'),
  10,
);
const applyTimeoutMs = parseInt(argValue('--apply-timeout', process.env.MN42_APPLY_TIMEOUT || '30000'), 10);
const shouldFlash = hasFlag('--flash') || process.env.MN42_FLASH_BEFORE_BOOT_TEST === '1';
const attachLive = hasFlag('--attach-live') || process.env.MN42_ATTACH_LIVE === '1';
const skipCleanup = hasFlag('--skip-cleanup');
const repoRoot = path.resolve(__dirname, '../..');
const firmwareDir = path.resolve(repoRoot, 'firmware');
const SERIAL_BAUD = 115200;
const NATIVE_SET_ALL_CHUNK_SIZE = 96;
const SLOT_TYPE_NAMES = [
  'OFF',
  'CC',
  'Note',
  'PitchBend',
  'ProgramChange',
  'Aftertouch',
  'ModWheel',
  'NRPN',
  'RPN',
  'SysEx'
];

if (!serialPath) {
  console.error('[boot-contract] Missing serial port. Pass --serial or set MN42_SERIAL/TEST_PORT.');
  process.exit(2);
}

function now() {
  return new Date().toISOString();
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

function parseJsonLine(line) {
  try {
    return JSON.parse(line);
  } catch (_) {
    return null;
  }
}

function chunkString(text, size) {
  const chunks = [];
  for (let index = 0; index < text.length; index += size) {
    chunks.push(text.slice(index, index + size));
  }
  return chunks;
}

function spawnLogged(command, commandArgs, options = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn(command, commandArgs, {
      cwd: options.cwd || repoRoot,
      env: { ...process.env, ...(options.env || {}) },
      stdio: ['ignore', 'pipe', 'pipe']
    });
    let stdout = '';
    let stderr = '';
    child.stdout.on('data', (chunk) => {
      const text = chunk.toString();
      stdout += text;
      process.stdout.write(text);
    });
    child.stderr.on('data', (chunk) => {
      const text = chunk.toString();
      stderr += text;
      process.stderr.write(text);
    });
    child.once('error', reject);
    child.once('exit', (code, signal) => {
      if (code === 0) {
        resolve({ stdout, stderr, code, signal });
        return;
      }
      reject(
        new Error(
          `${command} ${commandArgs.join(' ')} failed with ${signal ? `signal ${signal}` : `exit ${code}`}`,
        ),
      );
    });
  });
}

function waitForSerialOpen(serial) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error('Serial port failed to open in time')), 5000);
    serial.once('open', () => {
      clearTimeout(timer);
      resolve();
    });
    serial.once('error', (err) => {
      clearTimeout(timer);
      reject(err);
    });
    serial.open();
  });
}

async function openSerialClient(portPath, serialLog) {
  const serial = new SerialPort({
    path: portPath,
    baudRate: SERIAL_BAUD,
    autoOpen: false
  });
  const parser = serial.pipe(new ReadlineParser({ delimiter: '\n' }));
  parser.on('data', (rawLine) => {
    const line = String(rawLine || '').trim();
    if (!line) return;
    serialLog.push({ at: now(), direction: 'rx', line });
    console.log(`[boot-contract:serial] ${line}`);
  });
  await waitForSerialOpen(serial);
  return { serial, parser };
}

async function closeSerialClient(serialClient) {
  if (!serialClient?.serial) return;
  await new Promise((resolve) => {
    serialClient.serial.close(() => resolve());
  });
}

function waitForSerialLine(parser, timeoutMs, predicate, description) {
  return new Promise((resolve, reject) => {
    let done = false;
    const timer = setTimeout(() => {
      if (!done) {
        done = true;
        parser.removeListener('data', handler);
        reject(new Error(`Timeout waiting for serial response: ${description}`));
      }
    }, timeoutMs);

    function handler(rawLine) {
      const line = String(rawLine || '').trim();
      if (!line) return;
      try {
        if (!predicate || predicate(line)) {
          if (!done) {
            done = true;
            clearTimeout(timer);
            parser.removeListener('data', handler);
            resolve(line);
          }
        }
      } catch (err) {
        if (!done) {
          done = true;
          clearTimeout(timer);
          parser.removeListener('data', handler);
          reject(err);
        }
      }
    }

    parser.on('data', handler);
  });
}

function writeSerialLine(serialClient, line, serialLog) {
  serialLog.push({ at: now(), direction: 'tx', line });
  console.log(`[boot-contract:host] ${line}`);
  return new Promise((resolve, reject) => {
    serialClient.serial.write(`${line}\n`, (err) => {
      if (err) {
        reject(err);
        return;
      }
      resolve();
    });
  });
}

async function sendSerialJson(serialClient, line, predicate, timeoutMs, serialLog) {
  const waiter = waitForSerialLine(
    serialClient.parser,
    timeoutMs,
    (rawLine) => {
      const json = parseJsonLine(rawLine);
      return json && (!predicate || predicate(json, rawLine));
    },
    line,
  );
  await writeSerialLine(serialClient, line, serialLog);
  const raw = await waiter;
  return parseJsonLine(raw);
}

async function sendSerialRaw(serialClient, line, predicate, timeoutMs, serialLog) {
  const waiter = waitForSerialLine(serialClient.parser, timeoutMs, predicate, line);
  await writeSerialLine(serialClient, line, serialLog);
  return waiter;
}

async function loadAppConfigHelpers() {
  const sessionModuleUrl = pathToFileURL(path.resolve(repoRoot, 'App/runtime/config_session.js')).href;
  const normalizeModuleUrl = pathToFileURL(
    path.resolve(repoRoot, 'App/runtime/config_normalize.js'),
  ).href;
  const sessionModule = await import(sessionModuleUrl);
  const normalizeModule = await import(normalizeModuleUrl);
  return {
    compactConfigForDevice: sessionModule.compactConfigForDevice,
    normalizeConfig: normalizeModule.normalizeConfig
  };
}

function nextIdleFloorValue(config) {
  const current = Number(config?.filter?.idle_floor);
  if (!Number.isInteger(current)) return 25;
  return current >= 127 ? 126 : current + 1;
}

function summarizeManifest(manifest) {
  return {
    device_name: manifest?.device_name,
    fw_version: manifest?.fw_version,
    schema_version: manifest?.schema_version,
    git_sha: manifest?.git_sha,
    power_profile: manifest?.power_profile
  };
}

function ensureSchemaRoots(schema) {
  const requiredRoots = ['slots', 'efSlots', 'filter', 'arg', 'led'];
  const missing = requiredRoots.filter((key) => !(schema?.properties && schema.properties[key]));
  if (missing.length) {
    throw new Error(`Device schema missing required roots: ${missing.join(', ')}`);
  }
}

async function waitForBootMode(serialClient, expectedMode, timeoutMs) {
  const bootLine = await waitForSerialLine(
    serialClient.parser,
    timeoutMs,
    (line) => {
      const json = parseJsonLine(line);
      return json?.type === 'boot_mode' && json.mode === expectedMode;
    },
    `boot mode ${expectedMode}`,
  );
  return parseJsonLine(bootLine);
}

async function waitForBanner(serialClient, timeoutMs) {
  const banners = {};
  const deadline = Date.now() + timeoutMs;

  while (Date.now() < deadline) {
    const remaining = deadline - Date.now();
    const line = await waitForSerialLine(
      serialClient.parser,
      remaining,
      (candidate) =>
        candidate.startsWith('MN42 FW ') || candidate.startsWith('Reset ') || parseJsonLine(candidate)?.type === 'boot_mode',
      'boot banner',
    );
    if (line.startsWith('MN42 FW ') && !banners.versionLine) {
      if (line.includes(' schema ')) banners.schemaLine = line;
      else banners.versionLine = line;
    } else if (line.startsWith('Reset ')) {
      banners.resetLine = line;
    } else {
      const parsed = parseJsonLine(line);
      if (parsed?.type === 'boot_mode') {
        banners.bootMode = parsed;
      }
    }
    if (banners.versionLine && banners.schemaLine && banners.resetLine && banners.bootMode) {
      return banners;
    }
  }

  throw new Error('Timed out waiting for full boot banner');
}

async function tryWaitForBanner(serialClient, timeoutMs) {
  try {
    return await waitForBanner(serialClient, timeoutMs);
  } catch (_) {
    return null;
  }
}

async function flashFirmwareIfRequested() {
  if (!shouldFlash) return null;
  console.log(`[boot-contract] ${now()} flashing ${firmwareEnv}`);
  return spawnLogged('pio', ['run', '-d', firmwareDir, '-e', firmwareEnv, '-t', 'upload']);
}

async function reopenSerialClientWithRetry(portPath, timeoutMs, serialLog) {
  const deadline = Date.now() + timeoutMs;
  let lastError = null;
  while (Date.now() < deadline) {
    try {
      return await openSerialClient(portPath, serialLog);
    } catch (err) {
      lastError = err;
      await sleep(250);
    }
  }
  throw lastError || new Error(`Serial port did not become ready: ${portPath}`);
}

async function sendChunkedSetAll(serialClient, payload, serialLog) {
  const nativePayload = {
    seq: payload.seq,
    checksum: payload.checksum,
    config: payload.deviceConfig ?? payload.config
  };
  const jsonPayload = JSON.stringify(nativePayload);
  for (const chunk of chunkString(jsonPayload, NATIVE_SET_ALL_CHUNK_SIZE)) {
    await writeSerialLine(serialClient, `SET_ALL ${chunk}`, serialLog);
    await sleep(4);
  }
}

async function applyConfig(serialClient, baselineConfig, nextConfig, schema, manifest, seq, compactConfigForDevice, serialLog) {
  const payload = {
    rpc: 'set_config',
    seq,
    schema_version: schema?.schema_version,
    manifest: {
      fw_version: manifest?.fw_version,
      git_sha: manifest?.git_sha,
      build_time: manifest?.build_time,
      schema_version: manifest?.schema_version
    },
    config: nextConfig,
    deviceConfig: compactConfigForDevice(nextConfig, baselineConfig, {
      clone,
      slotTypeNames: SLOT_TYPE_NAMES
    })
  };
  const checksum = crypto.createHash('sha256').update(JSON.stringify(payload)).digest('hex');
  payload.checksum = checksum;

  const ackWaiter = waitForSerialLine(
    serialClient.parser,
    applyTimeoutMs,
    (line) => {
      const json = parseJsonLine(line);
      return json?.type === 'ack' && json?.seq === seq && json?.checksum === checksum;
    },
    `ACK seq=${seq}`,
  );
  await sendChunkedSetAll(serialClient, payload, serialLog);
  const ackLine = await ackWaiter;
  return { payload, ack: parseJsonLine(ackLine), checksum };
}

async function main() {
  const { compactConfigForDevice, normalizeConfig } = await loadAppConfigHelpers();
  const serialLog = [];
  const scenarios = [];
  const report = {
    generated_at_utc: now(),
    lane: attachLive ? 'teensy40_main_attach_live_contract' : 'teensy40_main_boot_contract',
    serial: serialPath,
    firmware_env: firmwareEnv,
    flashed: shouldFlash,
    attach_live: attachLive,
    scenarios,
    serial_log: serialLog
  };

  let serialClient = null;
  let baselineConfig = null;
  let schema = null;
  let manifest = null;

  try {
    if (shouldFlash) {
      await flashFirmwareIfRequested();
      await sleep(1200);
    }

    serialClient = await reopenSerialClientWithRetry(serialPath, bootTimeoutMs, serialLog);
    if (!attachLive) {
      const standaloneBanner = await waitForBanner(serialClient, bootTimeoutMs);
      if (standaloneBanner.bootMode?.mode !== 'standalone_runtime') {
        throw new Error(
          `Expected standalone_runtime boot, saw ${JSON.stringify(standaloneBanner.bootMode || null)}`,
        );
      }
      scenarios.push({
        id: 'standalone-boot',
        title: 'Standalone boot banner and boot_mode marker appear',
        ok: true,
        detail: `${standaloneBanner.versionLine} | ${standaloneBanner.schemaLine} | ${standaloneBanner.resetLine}`
      });
    }

    const standaloneHello = await sendSerialJson(
      serialClient,
      'HELLO',
      (json) => json.hello === 'mn42',
      commandTimeoutMs,
      serialLog,
    );
    scenarios.push({
      id: attachLive ? 'live-attach-hello' : 'standalone-hello',
      title: attachLive
        ? 'Already-running firmware answers HELLO before configurator handoff'
        : 'Standalone runtime answers HELLO',
      ok: true,
      detail: `hello=${standaloneHello.hello}`
    });

    const rebootAck = await sendSerialJson(
      serialClient,
      'ENTER_CONFIG_MODE',
      (json) => json.command === 'ENTER_CONFIG_MODE' && json.status === 'ok' && json.rebooting === true,
      commandTimeoutMs,
      serialLog,
    );
    scenarios.push({
      id: 'config-mode-request',
      title: 'ENTER_CONFIG_MODE acknowledges and requests reboot',
      ok: true,
      detail: JSON.stringify(rebootAck)
    });

    await closeSerialClient(serialClient);
    serialClient = null;
    await sleep(1200);
    serialClient = await reopenSerialClientWithRetry(serialPath, bootTimeoutMs, serialLog);
    if (!attachLive) {
      const configBanner = await waitForBanner(serialClient, bootTimeoutMs);
      if (configBanner.bootMode?.mode !== 'usb_configurator') {
        throw new Error(
          `Expected usb_configurator boot, saw ${JSON.stringify(configBanner.bootMode || null)}`,
        );
      }
      scenarios.push({
        id: 'configurator-boot',
        title: 'Configurator reboot lands in usb_configurator mode',
        ok: true,
        detail: `${configBanner.versionLine} | ${configBanner.schemaLine} | ${configBanner.resetLine}`
      });
    } else {
      const configBanner = await tryWaitForBanner(serialClient, Math.min(bootTimeoutMs, 2500));
      scenarios.push({
        id: 'configurator-reconnect',
        title: 'Configurator reconnect succeeds after ENTER_CONFIG_MODE',
        ok: true,
        detail: configBanner
          ? `${configBanner.versionLine} | ${configBanner.schemaLine} | ${configBanner.resetLine}`
          : 'Serial port reattached; configurator banner was not required in attach-live mode.'
      });
    }

    const hello = await sendSerialJson(
      serialClient,
      'HELLO',
      (json) => json.hello === 'mn42',
      commandTimeoutMs,
      serialLog,
    );
    manifest = await sendSerialJson(
      serialClient,
      'GET_MANIFEST',
      (json) => json.device_name === 'MOARkNOBS-42' && Number.isInteger(json.schema_version),
      commandTimeoutMs,
      serialLog,
    );
    schema = await sendSerialJson(
      serialClient,
      'GET_SCHEMA',
      (json) => json?.type === 'object' && json?.properties,
      commandTimeoutMs,
      serialLog,
    );
    ensureSchemaRoots(schema);
    const baselineConfigRaw = await sendSerialJson(
      serialClient,
      'GET_CONFIG',
      (json) => Array.isArray(json?.slots) && Array.isArray(json?.efSlots) && json?.filter && json?.arg && json?.led,
      commandTimeoutMs,
      serialLog,
    );
    baselineConfig = normalizeConfig(baselineConfigRaw, manifest);
    scenarios.push({
      id: 'configurator-handshake',
      title: 'Configurator lane completes HELLO -> manifest -> schema -> config',
      ok: true,
      detail: `hello=${hello.hello} manifest=${JSON.stringify(summarizeManifest(manifest))} slots=${baselineConfig.slots.length}`
    });

    const mutatedConfig = clone(baselineConfig);
    mutatedConfig.filter.idle_floor = nextIdleFloorValue(baselineConfig);
    const applyResult = await applyConfig(
      serialClient,
      baselineConfig,
      mutatedConfig,
      schema,
      manifest,
      1,
      compactConfigForDevice,
      serialLog,
    );
    const appliedConfigRaw = await sendSerialJson(
      serialClient,
      'GET_CONFIG',
      (json) => Number(json?.filter?.idle_floor) === mutatedConfig.filter.idle_floor,
      commandTimeoutMs,
      serialLog,
    );
    const appliedConfig = normalizeConfig(appliedConfigRaw, manifest);
    scenarios.push({
      id: 'set-all-ack',
      title: 'SET_ALL apply returns matching ACK and persists the staged change',
      ok: true,
      detail: `checksum=${applyResult.checksum} idle_floor=${baselineConfig.filter.idle_floor} -> ${appliedConfig.filter.idle_floor}`
    });

    if (!skipCleanup) {
      const revertResult = await applyConfig(
        serialClient,
        appliedConfig,
        baselineConfig,
        schema,
        manifest,
        2,
        compactConfigForDevice,
        serialLog,
      );
      await sendSerialJson(
        serialClient,
        'GET_CONFIG',
        (json) => Number(json?.filter?.idle_floor) === baselineConfig.filter.idle_floor,
        commandTimeoutMs,
        serialLog,
      );
      scenarios.push({
        id: 'cleanup',
        title: 'Runner restores the original config after the apply proof',
        ok: true,
        detail: `checksum=${revertResult.checksum} restored idle_floor=${baselineConfig.filter.idle_floor}`
      });
    }

    report.result = 'passed';
  } catch (err) {
    report.result = 'failed';
    report.error = err.message;
    scenarios.push({
      id: 'failure',
      title: 'Boot contract runner failed',
      ok: false,
      detail: err.message
    });
    throw err;
  } finally {
    if (reportPath) {
      fs.mkdirSync(path.dirname(reportPath), { recursive: true });
      fs.writeFileSync(reportPath, JSON.stringify(report, null, 2) + '\n', 'utf8');
    }
    await closeSerialClient(serialClient);
  }
}

main().catch((err) => {
  console.error(`[boot-contract] ${err.message}`);
  process.exit(1);
});
