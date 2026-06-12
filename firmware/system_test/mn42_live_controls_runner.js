#!/usr/bin/env node
/*
 * Hardware receipt runner for direct firmware live-control lanes.
 *
 * Proves on an attached teensy40_main board:
 *  - USB MIDI output toggle round-trip
 *  - note dynamics live lane round-trip
 *  - jitter live lane round-trip
 *  - clock live lane round-trip
 *  - live-only controls do not mutate the normalized GET_CONFIG snapshot
 *  - normalized GET_CONFIG remains stable before/after the live-only changes
 */

'use strict';

const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');
const { pathToFileURL } = require('url');

const { SerialPort, ReadlineParser } = require(path.resolve(
  __dirname,
  '../../bridge/node_modules/serialport',
));

const args = process.argv.slice(2);
const repoRoot = path.resolve(__dirname, '../..');

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

function now() {
  return new Date().toISOString();
}

function dateStamp() {
  return now().slice(0, 10);
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
    return null;
  }
  return null;
}

function parseJsonLine(line) {
  try {
    return JSON.parse(line);
  } catch (_) {
    return null;
  }
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

function boolText(value) {
  return value ? 'true' : 'false';
}

function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function gitOutput(...commandArgs) {
  const result = spawnSync('git', commandArgs, {
    cwd: repoRoot,
    encoding: 'utf8'
  });
  if (result.status !== 0) return null;
  return result.stdout.trim() || null;
}

function sha256Json(value) {
  return crypto.createHash('sha256').update(JSON.stringify(value)).digest('hex');
}

function numberClose(a, b, tolerance = 0.001) {
  return Math.abs(Number(a) - Number(b)) <= tolerance;
}

function chooseAltInt(current, preferred, minValue, maxValue) {
  const normalized = Math.max(minValue, Math.min(maxValue, Math.round(Number(current) || 0)));
  const candidate = Math.max(minValue, Math.min(maxValue, preferred));
  if (candidate !== normalized) return candidate;
  if (candidate < maxValue) return candidate + 1;
  return candidate > minValue ? candidate - 1 : candidate;
}

function chooseAltFloat(current, preferred, minValue, maxValue) {
  const normalized = Math.max(minValue, Math.min(maxValue, Number(current) || 0));
  const candidate = Math.max(minValue, Math.min(maxValue, preferred));
  if (!numberClose(candidate, normalized, 0.0005)) return candidate;
  const plus = Math.min(maxValue, candidate + 0.125);
  if (!numberClose(plus, normalized, 0.0005)) return plus;
  return Math.max(minValue, candidate - 0.125);
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
    baudRate: 115200,
    autoOpen: false
  });
  const parser = serial.pipe(new ReadlineParser({ delimiter: '\n' }));
  parser.on('data', (rawLine) => {
    const line = String(rawLine || '').trim();
    if (!line) return;
    serialLog.push({ at: now(), direction: 'rx', line });
    console.log(`[live-controls:serial] ${line}`);
  });
  await waitForSerialOpen(serial);
  return { serial, parser };
}

async function closeSerialClient(serialClient) {
  if (!serialClient?.serial) return;
  await new Promise((resolve) => serialClient.serial.close(() => resolve()));
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
  console.log(`[live-controls:host] ${line}`);
  return new Promise((resolve, reject) => {
    serialClient.serial.write(`${line}\n`, (err) => {
      if (err) reject(err);
      else resolve();
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

async function loadNormalizeConfig() {
  const normalizeModuleUrl = pathToFileURL(
    path.resolve(repoRoot, 'App/runtime/config_normalize.js'),
  ).href;
  const normalizeModule = await import(normalizeModuleUrl);
  return normalizeModule.normalizeConfig;
}

async function enterConfiguratorLane(portPath, serialLog, timeouts) {
  let serialClient = await reopenSerialClientWithRetry(portPath, timeouts.bootTimeoutMs, serialLog);
  try {
    await sendSerialJson(
      serialClient,
      'HELLO',
      (json) => json.hello === 'mn42',
      timeouts.commandTimeoutMs,
      serialLog,
    );
    await sendSerialJson(
      serialClient,
      'ENTER_CONFIG_MODE',
      (json) =>
        json.command === 'ENTER_CONFIG_MODE' && json.status === 'ok' && json.rebooting === true,
      timeouts.commandTimeoutMs,
      serialLog,
    );
    await closeSerialClient(serialClient);
    serialClient = null;
    await sleep(1200);
    serialClient = await reopenSerialClientWithRetry(portPath, timeouts.bootTimeoutMs, serialLog);
    await sendSerialJson(
      serialClient,
      'HELLO',
      (json) => json.hello === 'mn42',
      timeouts.commandTimeoutMs,
      serialLog,
    );
    return serialClient;
  } catch (error) {
    await closeSerialClient(serialClient);
    throw error;
  }
}

async function getManifest(serialClient, timeoutMs, serialLog) {
  return sendSerialJson(
    serialClient,
    'GET_MANIFEST',
    (json) => json.device_name === 'MOARkNOBS-42' && Number.isInteger(json.schema_version),
    timeoutMs,
    serialLog,
  );
}

async function getConfigSnapshot(serialClient, manifest, normalizeConfig, timeoutMs, serialLog) {
  const raw = await sendSerialJson(
    serialClient,
    'GET_CONFIG',
    (json) =>
      Array.isArray(json?.slots) &&
      Array.isArray(json?.efSlots) &&
      json?.filter &&
      json?.arg &&
      json?.led,
    timeoutMs,
    serialLog,
  );
  const normalized = normalizeConfig(raw, manifest);
  return {
    raw,
    normalized,
    hash: sha256Json(normalized)
  };
}

async function getUsbMidi(serialClient, timeoutMs, serialLog) {
  return sendSerialJson(
    serialClient,
    'GET_USB_MIDI',
    (json) => json.command === 'GET_USB_MIDI' && typeof json.usb_midi_out === 'boolean',
    timeoutMs,
    serialLog,
  );
}

async function setUsbMidi(serialClient, enabled, timeoutMs, serialLog) {
  return sendSerialJson(
    serialClient,
    `SET_USB_MIDI,${enabled ? 1 : 0}`,
    (json) =>
      json.command === 'SET_USB_MIDI' &&
      json.status === 'ok' &&
      json.usb_midi_out === Boolean(enabled),
    timeoutMs,
    serialLog,
  );
}

async function getNoteDynamics(serialClient, timeoutMs, serialLog) {
  return sendSerialJson(
    serialClient,
    'GET_NOTE_DYNAMICS',
    (json) =>
      json.command === 'GET_NOTE_DYNAMICS' &&
      Number.isInteger(json.velocity_shift) &&
      Number.isInteger(json.change_probability),
    timeoutMs,
    serialLog,
  );
}

async function setNoteDynamics(serialClient, state, timeoutMs, serialLog) {
  return sendSerialJson(
    serialClient,
    `SET_NOTE_DYNAMICS,${state.velocity_shift},${state.change_probability}`,
    (json) =>
      json.command === 'SET_NOTE_DYNAMICS' &&
      json.status === 'ok' &&
      json.velocity_shift === state.velocity_shift &&
      json.change_probability === state.change_probability,
    timeoutMs,
    serialLog,
  );
}

async function getJitter(serialClient, timeoutMs, serialLog) {
  return sendSerialJson(
    serialClient,
    'GET_JITTER',
    (json) =>
      json.command === 'GET_JITTER' &&
      Number.isFinite(json.depth) &&
      Number.isFinite(json.smoothness),
    timeoutMs,
    serialLog,
  );
}

async function setJitter(serialClient, state, timeoutMs, serialLog) {
  return sendSerialJson(
    serialClient,
    `SET_JITTER,${state.depth.toFixed(3)},${state.smoothness.toFixed(3)}`,
    (json) =>
      json.command === 'SET_JITTER' &&
      json.status === 'ok' &&
      numberClose(json.depth, state.depth) &&
      numberClose(json.smoothness, state.smoothness),
    timeoutMs,
    serialLog,
  );
}

async function getClock(serialClient, timeoutMs, serialLog) {
  return sendSerialJson(
    serialClient,
    'GET_CLOCK',
    (json) =>
      json.command === 'GET_CLOCK' &&
      typeof json.follow_external === 'boolean' &&
      typeof json.clock_out_enabled === 'boolean' &&
      Number.isFinite(json.tapped_bpm),
    timeoutMs,
    serialLog,
  );
}

async function setClock(serialClient, state, timeoutMs, serialLog) {
  return sendSerialJson(
    serialClient,
    `SET_CLOCK,${state.follow_external ? 1 : 0},${state.clock_out_enabled ? 1 : 0},${state.tapped_bpm.toFixed(2)}`,
    (json) =>
      json.command === 'SET_CLOCK' &&
      json.status === 'ok' &&
      json.follow_external === state.follow_external &&
      json.clock_out_enabled === state.clock_out_enabled &&
      numberClose(json.tapped_bpm, state.tapped_bpm, 0.01),
    timeoutMs,
    serialLog,
  );
}

async function exerciseUsbMidi(serialClient, timeoutMs, serialLog) {
  const baseline = await getUsbMidi(serialClient, timeoutMs, serialLog);
  const mutated = !baseline.usb_midi_out;
  await setUsbMidi(serialClient, mutated, timeoutMs, serialLog);
  const readback = await getUsbMidi(serialClient, timeoutMs, serialLog);
  try {
    if (readback.usb_midi_out !== mutated) {
      throw new Error(`USB MIDI readback mismatch: expected ${mutated}, saw ${readback.usb_midi_out}`);
    }
    return {
      baseline: { usb_midi_out: baseline.usb_midi_out },
      mutated: { usb_midi_out: mutated },
      readback: { usb_midi_out: readback.usb_midi_out }
    };
  } finally {
    await setUsbMidi(serialClient, baseline.usb_midi_out, timeoutMs, serialLog);
  }
}

async function exerciseNoteDynamics(serialClient, timeoutMs, serialLog) {
  const baseline = await getNoteDynamics(serialClient, timeoutMs, serialLog);
  const mutated = {
    velocity_shift: chooseAltInt(baseline.velocity_shift, -12, -64, 63),
    change_probability: chooseAltInt(baseline.change_probability, 83, 0, 100)
  };
  await setNoteDynamics(serialClient, mutated, timeoutMs, serialLog);
  const readback = await getNoteDynamics(serialClient, timeoutMs, serialLog);
  try {
    if (
      readback.velocity_shift !== mutated.velocity_shift ||
      readback.change_probability !== mutated.change_probability
    ) {
      throw new Error(
        `Note dynamics readback mismatch: expected ${JSON.stringify(mutated)}, saw ${JSON.stringify(readback)}`
      );
    }
    return {
      baseline: {
        velocity_shift: baseline.velocity_shift,
        change_probability: baseline.change_probability
      },
      mutated,
      readback: {
        velocity_shift: readback.velocity_shift,
        change_probability: readback.change_probability
      }
    };
  } finally {
    await setNoteDynamics(
      serialClient,
      {
        velocity_shift: baseline.velocity_shift,
        change_probability: baseline.change_probability
      },
      timeoutMs,
      serialLog,
    );
  }
}

async function exerciseJitter(serialClient, timeoutMs, serialLog) {
  const baseline = await getJitter(serialClient, timeoutMs, serialLog);
  const mutated = {
    depth: chooseAltFloat(baseline.depth, 0.250, 0.0, 1.0),
    smoothness: chooseAltFloat(baseline.smoothness, 0.750, 0.0, 1.0)
  };
  await setJitter(serialClient, mutated, timeoutMs, serialLog);
  const readback = await getJitter(serialClient, timeoutMs, serialLog);
  try {
    if (!numberClose(readback.depth, mutated.depth) || !numberClose(readback.smoothness, mutated.smoothness)) {
      throw new Error(
        `Jitter readback mismatch: expected ${JSON.stringify(mutated)}, saw ${JSON.stringify(readback)}`
      );
    }
    return {
      baseline: { depth: baseline.depth, smoothness: baseline.smoothness },
      mutated,
      readback: { depth: readback.depth, smoothness: readback.smoothness }
    };
  } finally {
    await setJitter(
      serialClient,
      {
        depth: Number(baseline.depth),
        smoothness: Number(baseline.smoothness)
      },
      timeoutMs,
      serialLog,
    );
  }
}

async function exerciseClock(serialClient, timeoutMs, serialLog) {
  const baseline = await getClock(serialClient, timeoutMs, serialLog);
  const mutated = {
    follow_external: !baseline.follow_external,
    clock_out_enabled: !baseline.clock_out_enabled,
    tapped_bpm: chooseAltFloat(baseline.tapped_bpm, 123.5, 20.0, 300.0)
  };
  await setClock(serialClient, mutated, timeoutMs, serialLog);
  const readback = await getClock(serialClient, timeoutMs, serialLog);
  try {
    if (
      readback.follow_external !== mutated.follow_external ||
      readback.clock_out_enabled !== mutated.clock_out_enabled ||
      !numberClose(readback.tapped_bpm, mutated.tapped_bpm, 0.01)
    ) {
      throw new Error(
        `Clock readback mismatch: expected ${JSON.stringify(mutated)}, saw ${JSON.stringify(readback)}`
      );
    }
    return {
      baseline: {
        follow_external: baseline.follow_external,
        clock_out_enabled: baseline.clock_out_enabled,
        tapped_bpm: baseline.tapped_bpm
      },
      mutated,
      readback: {
        follow_external: readback.follow_external,
        clock_out_enabled: readback.clock_out_enabled,
        tapped_bpm: readback.tapped_bpm
      }
    };
  } finally {
    await setClock(
      serialClient,
      {
        follow_external: baseline.follow_external,
        clock_out_enabled: baseline.clock_out_enabled,
        tapped_bpm: Number(baseline.tapped_bpm)
      },
      timeoutMs,
      serialLog,
    );
  }
}

function renderMarkdown(report) {
  const live = report.live_controls || {};
  const manifest = report.manifest || {};
  const config = report.config_stability || {};
  return `# Firmware Bench Summary: Live Controls

Date: ${dateStamp()}
Commit: ${report.git?.commit ?? 'unknown/not captured'}
Commit short: ${report.git?.short_commit ?? 'unknown/not captured'}
Firmware git_sha: ${manifest.git_sha ?? 'unknown/not captured'}
Firmware version: ${manifest.fw_version ?? 'unknown/not captured'}
Schema version: ${manifest.schema_version ?? 'unknown/not captured'}
Power profile: ${manifest.power_profile ?? 'unknown/not captured'}
Serial port: ${report.serial ?? 'unknown/not captured'}
Host: ${report.host?.hostname ?? 'unknown/not captured'}
Platform: ${report.host?.platform ?? 'unknown/not captured'}
Firmware env: ${report.firmware_env ?? 'unknown/not captured'}
Runner: firmware/system_test/mn42_live_controls_runner.js
JSON report: ${report.artifacts?.report ?? 'unknown/not captured'}

## Result

${report.result === 'passed' ? 'PASS' : 'FAIL'}

## Proven

- USB MIDI output toggle round-trips through \`GET_USB_MIDI\` / \`SET_USB_MIDI\`.
- Note dynamics round-trips through \`GET_NOTE_DYNAMICS\` / \`SET_NOTE_DYNAMICS\`.
- Jitter round-trips through \`GET_JITTER\` / \`SET_JITTER\`.
- Clock round-trips through \`GET_CLOCK\` / \`SET_CLOCK\`.
- Normalized \`GET_CONFIG\` hash is unchanged before and after the live-only lanes.
- Live-only controls did not create a config diff on the firmware lane.

## Lane Summary

- USB MIDI: ${boolText(live.usb_midi?.baseline?.usb_midi_out)} -> ${boolText(live.usb_midi?.mutated?.usb_midi_out)} -> ${boolText(live.usb_midi?.restored?.usb_midi_out)}
- Note dynamics: velocity ${live.note_dynamics?.baseline?.velocity_shift}, probability ${live.note_dynamics?.baseline?.change_probability} -> velocity ${live.note_dynamics?.mutated?.velocity_shift}, probability ${live.note_dynamics?.mutated?.change_probability} -> restored velocity ${live.note_dynamics?.restored?.velocity_shift}, probability ${live.note_dynamics?.restored?.change_probability}
- Jitter: depth ${live.jitter?.baseline?.depth}, smoothness ${live.jitter?.baseline?.smoothness} -> depth ${live.jitter?.mutated?.depth}, smoothness ${live.jitter?.mutated?.smoothness} -> restored depth ${live.jitter?.restored?.depth}, smoothness ${live.jitter?.restored?.smoothness}
- Clock: follow_external ${boolText(live.clock?.baseline?.follow_external)}, clock_out ${boolText(live.clock?.baseline?.clock_out_enabled)}, bpm ${live.clock?.baseline?.tapped_bpm} -> follow_external ${boolText(live.clock?.mutated?.follow_external)}, clock_out ${boolText(live.clock?.mutated?.clock_out_enabled)}, bpm ${live.clock?.mutated?.tapped_bpm} -> restored follow_external ${boolText(live.clock?.restored?.follow_external)}, clock_out ${boolText(live.clock?.restored?.clock_out_enabled)}, bpm ${live.clock?.restored?.tapped_bpm}

## Config Stability

- Baseline normalized GET_CONFIG hash: \`${config.baseline_hash ?? 'unknown/not captured'}\`
- Final normalized GET_CONFIG hash: \`${config.final_hash ?? 'unknown/not captured'}\`
- Stable: ${config.stable ? 'yes' : 'no'}

## Caveats

- This receipt proves firmware-side live control behavior directly over the serial/configurator lane.
- “Does not dirty staged config” is evidenced here by unchanged normalized \`GET_CONFIG\` state before/after the live-only commands.
- This receipt does not claim Bridge/App session behavior by itself.
`;
}

async function main() {
  const serialPath = argValue('--serial', detectPort());
  const reportPath = argValue(
    '--report',
    path.resolve(repoRoot, 'logs', `live-controls-${dateStamp()}.json`)
  );
  const markdownPath = argValue(
    '--markdown',
    path.resolve(repoRoot, 'docs/bench/firmware', `${dateStamp()}_live-controls-summary.md`)
  );
  const firmwareEnv = argValue('--firmware-env', process.env.MN42_FIRMWARE_ENV || 'teensy40_main');
  const bootTimeoutMs = parseInt(argValue('--boot-timeout', process.env.MN42_BOOT_TIMEOUT || '12000'), 10);
  const commandTimeoutMs = parseInt(
    argValue('--command-timeout', process.env.MN42_COMMAND_TIMEOUT || '5000'),
    10,
  );

  if (!serialPath) {
    throw new Error('Missing serial port. Pass --serial or set MN42_SERIAL/TEST_PORT.');
  }

  const normalizeConfig = await loadNormalizeConfig();
  const serialLog = [];
  const scenarios = [];
  const report = {
    generated_at_utc: now(),
    serial: serialPath,
    firmware_env: firmwareEnv,
    artifacts: {
      report: reportPath,
      markdown_summary: markdownPath
    },
    git: {
      commit: gitOutput('rev-parse', 'HEAD'),
      short_commit: gitOutput('rev-parse', '--short', 'HEAD')
    },
    host: {
      hostname: os.hostname(),
      platform: `${os.platform()} ${os.release()}`
    },
    live_controls: {},
    config_stability: {},
    scenarios,
    serial_log: serialLog
  };

  let serialClient = null;
  try {
    serialClient = await enterConfiguratorLane(serialPath, serialLog, {
      bootTimeoutMs,
      commandTimeoutMs
    });
    const manifest = await getManifest(serialClient, commandTimeoutMs, serialLog);
    report.manifest = summarizeManifest(manifest);

    const baselineConfig = await getConfigSnapshot(
      serialClient,
      manifest,
      normalizeConfig,
      commandTimeoutMs,
      serialLog,
    );
    report.config_stability.baseline_hash = baselineConfig.hash;

    const usbMidi = await exerciseUsbMidi(serialClient, commandTimeoutMs, serialLog);
    usbMidi.restored = {
      usb_midi_out: usbMidi.baseline.usb_midi_out
    };
    report.live_controls.usb_midi = usbMidi;
    scenarios.push({
      id: 'usb-midi-toggle',
      title: 'USB MIDI output toggle round-trips and restores',
      ok: true,
      detail: `${boolText(usbMidi.baseline.usb_midi_out)} -> ${boolText(usbMidi.mutated.usb_midi_out)} -> ${boolText(usbMidi.restored.usb_midi_out)}`
    });

    const noteDynamics = await exerciseNoteDynamics(serialClient, commandTimeoutMs, serialLog);
    noteDynamics.restored = clone(noteDynamics.baseline);
    report.live_controls.note_dynamics = noteDynamics;
    scenarios.push({
      id: 'note-dynamics-lane',
      title: 'Note dynamics live lane round-trips and restores',
      ok: true,
      detail: `velocity ${noteDynamics.baseline.velocity_shift} -> ${noteDynamics.mutated.velocity_shift}; probability ${noteDynamics.baseline.change_probability} -> ${noteDynamics.mutated.change_probability}`
    });

    const jitter = await exerciseJitter(serialClient, commandTimeoutMs, serialLog);
    jitter.restored = {
      depth: Number(jitter.baseline.depth),
      smoothness: Number(jitter.baseline.smoothness)
    };
    report.live_controls.jitter = jitter;
    scenarios.push({
      id: 'jitter-lane',
      title: 'Jitter live lane round-trips and restores',
      ok: true,
      detail: `depth ${jitter.baseline.depth} -> ${jitter.mutated.depth}; smoothness ${jitter.baseline.smoothness} -> ${jitter.mutated.smoothness}`
    });

    const clock = await exerciseClock(serialClient, commandTimeoutMs, serialLog);
    clock.restored = {
      follow_external: clock.baseline.follow_external,
      clock_out_enabled: clock.baseline.clock_out_enabled,
      tapped_bpm: Number(clock.baseline.tapped_bpm)
    };
    report.live_controls.clock = clock;
    scenarios.push({
      id: 'clock-lane',
      title: 'Clock live lane round-trips and restores',
      ok: true,
      detail: `follow_external ${boolText(clock.baseline.follow_external)} -> ${boolText(clock.mutated.follow_external)}; clock_out ${boolText(clock.baseline.clock_out_enabled)} -> ${boolText(clock.mutated.clock_out_enabled)}; bpm ${clock.baseline.tapped_bpm} -> ${clock.mutated.tapped_bpm}`
    });

    const finalConfig = await getConfigSnapshot(
      serialClient,
      manifest,
      normalizeConfig,
      commandTimeoutMs,
      serialLog,
    );
    report.config_stability.final_hash = finalConfig.hash;
    report.config_stability.stable = baselineConfig.hash === finalConfig.hash;
    scenarios.push({
      id: 'live-controls-no-config-diff',
      title: 'Live control does not dirty staged config',
      ok: report.config_stability.stable,
      detail: `normalized GET_CONFIG hash ${baselineConfig.hash} -> ${finalConfig.hash}`
    });
    scenarios.push({
      id: 'get-config-stable',
      title: 'GET_CONFIG remains stable before and after live-only changes',
      ok: report.config_stability.stable,
      detail: `normalized GET_CONFIG hash ${baselineConfig.hash} -> ${finalConfig.hash}`
    });

    if (!report.config_stability.stable) {
      throw new Error(
        `Normalized GET_CONFIG changed across live-only lanes: ${baselineConfig.hash} -> ${finalConfig.hash}`
      );
    }

    report.result = 'passed';
  } catch (error) {
    report.result = 'failed';
    report.error = error.message || String(error);
    scenarios.push({
      id: 'failure',
      title: 'Live controls runner failed',
      ok: false,
      detail: report.error
    });
    throw error;
  } finally {
    fs.mkdirSync(path.dirname(reportPath), { recursive: true });
    fs.writeFileSync(reportPath, `${JSON.stringify(report, null, 2)}\n`, 'utf8');
    fs.mkdirSync(path.dirname(markdownPath), { recursive: true });
    fs.writeFileSync(markdownPath, `${renderMarkdown(report)}\n`, 'utf8');
    await closeSerialClient(serialClient);
    console.log(`[live-controls] wrote report to ${reportPath}`);
    console.log(`[live-controls] wrote markdown summary to ${markdownPath}`);
  }
}

main().catch((error) => {
  console.error(`[live-controls] ${error.message || error}`);
  process.exit(1);
});
