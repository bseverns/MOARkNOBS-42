#!/usr/bin/env node
/*
 * Hardware-in-the-loop smoke test for the MN42 stack.
 *
 * This script spawns `bridge/mn42_bridge.js`, drives it over OSC, and waits for
 * the Teensy (flashed with `teensy40_full_system`) to stream WebSerial JSON.
 * Each scenario mirrors the stories outlined in docs/TESTING.md so CI can
 * prove the bridge, firmware, and OSC plumbing still play nice when real
 * hardware is bolted on.
 */

'use strict';

const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');
const osc = require(path.resolve(__dirname, '../../bridge/node_modules/osc'));
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

const envSerial = process.env.MN42_SERIAL || process.env.TEST_PORT;
const serialPath = argValue('--serial', envSerial || '/dev/ttyACM0');
const oscOutPort = parseInt(argValue('--osc-out', process.env.MN42_OSC_OUT || '10000'), 10);
const oscInPort = parseInt(argValue('--osc-in', process.env.MN42_OSC_IN || String(oscOutPort + 1)), 10);
const oscHost = argValue('--host', process.env.MN42_OSC_HOST || '127.0.0.1');
const oscBind = argValue('--bind', process.env.MN42_OSC_BIND || '127.0.0.1');
const midiLabel = argValue('--midi', process.env.MN42_MIDI || 'MN42 Bridge System Test');
const reportPath = argValue('--report', process.env.MN42_REPORT || '');
const scenarioTimeoutMs = parseInt(argValue('--timeout', process.env.MN42_SCENARIO_TIMEOUT || '12000'), 10);
const commandTimeoutMs = parseInt(argValue('--command-timeout', process.env.MN42_COMMAND_TIMEOUT || '8000'), 10);
const profileSlot = parseInt(argValue('--profile-slot', process.env.MN42_PROFILE_SLOT || '3'), 10);
const sceneSlot = parseInt(argValue('--scene-slot', process.env.MN42_SCENE_SLOT || '5'), 10);
const potIndex = parseInt(argValue('--pot-index', process.env.MN42_POT_INDEX || '0'), 10);
const exerciseStorage = hasFlag('--exercise-storage') || process.env.MN42_EXERCISE_STORAGE === '1';
const SERIAL_BAUD = 115200;

if (!serialPath) {
  console.error('[system-test] Missing serial port. Pass --serial or set MN42_SERIAL/TEST_PORT.');
  process.exit(2);
}

function now() {
  return new Date().toISOString();
}

function parseArgs(message) {
  if (!Array.isArray(message.args)) return [];
  return message.args.map((arg) => {
    if (arg && typeof arg === 'object' && 'value' in arg) {
      return arg.value;
    }
    return arg;
  });
}

function parseJsonLine(line) {
  try {
    return JSON.parse(line);
  } catch (_) {
    return null;
  }
}

function waitForPortReady(port) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error('OSC port failed to open in time')), 5000);
    port.once('ready', () => {
      clearTimeout(timer);
      resolve();
    });
    port.once('error', (err) => {
      clearTimeout(timer);
      reject(err);
    });
    port.open();
  });
}

function waitForMessage(port, address, timeoutMs, predicate) {
  return new Promise((resolve, reject) => {
    let resolved = false;
    const timer = setTimeout(() => {
      if (!resolved) {
        resolved = true;
        port.removeListener('message', handler);
        reject(new Error(`Timeout waiting for ${address}`));
      }
    }, timeoutMs);

    function handler(message) {
      if (message.address !== address) {
        return;
      }
      try {
        const values = parseArgs(message);
        if (!predicate || predicate(message, values)) {
          if (!resolved) {
            resolved = true;
            clearTimeout(timer);
            port.removeListener('message', handler);
            resolve({ message, values });
          }
        }
      } catch (err) {
        if (!resolved) {
          resolved = true;
          clearTimeout(timer);
          port.removeListener('message', handler);
          reject(err);
        }
      }
    }

    port.on('message', handler);
  });
}

function waitForSerialOpen(port) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error('Serial port failed to open in time')), 5000);
    port.once('open', () => {
      clearTimeout(timer);
      resolve();
    });
    port.once('error', (err) => {
      clearTimeout(timer);
      reject(err);
    });
    port.open();
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
      console.log(`[system-test:serial] ${line}`);
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

async function openSerialClient(portPath) {
  const serial = new SerialPort({
    path: portPath,
    baudRate: SERIAL_BAUD,
    autoOpen: false,
  });
  const parser = serial.pipe(new ReadlineParser({ delimiter: '\n' }));
  await waitForSerialOpen(serial);
  return { serial, parser };
}

function writeSerialLine(serial, line) {
  return new Promise((resolve, reject) => {
    serial.write(`${line}\n`, (err) => {
      if (err) {
        reject(err);
        return;
      }
      resolve();
    });
  });
}

async function sendSerialLine(serialClient, line, predicate, timeoutMs = commandTimeoutMs) {
  const wait = waitForSerialLine(serialClient.parser, timeoutMs, predicate, line);
  await writeSerialLine(serialClient.serial, line);
  return wait;
}

async function sendSerialJson(serialClient, line, predicate, timeoutMs = commandTimeoutMs) {
  const raw = await sendSerialLine(
    serialClient,
    line,
    (serialLine) => {
      const json = parseJsonLine(serialLine);
      return json && (!predicate || predicate(json));
    },
    timeoutMs,
  );
  return parseJsonLine(raw);
}

async function closeSerialClient(serialClient) {
  if (!serialClient || !serialClient.serial) return;
  await new Promise((resolve) => {
    serialClient.serial.close(() => resolve());
  });
}

async function runStorageScenarios(scenarios) {
  if (!exerciseStorage) {
    scenarios.push({
      id: 'storage-smoke-armed',
      title: 'EEPROM storage smoke path is available',
      ok: true,
      detail:
        'Skipped destructive storage checks. Re-run with --exercise-storage on sacrificial profile/scene slots to prove profile, macro, and scene persistence.',
    });
    return;
  }

  const serialClient = await openSerialClient(serialPath);
  let baselinePot = null;
  let activeProfile = 0;
  let originalProfile = null;

  try {
    await new Promise((resolve) => setTimeout(resolve, 1200));

    const hello = await sendSerialJson(
      serialClient,
      'HELLO',
      (json) => json.hello === 'mn42',
      scenarioTimeoutMs,
    );
    scenarios.push({
      id: 'serial-hello',
      title: 'Direct serial control lane answers HELLO',
      ok: true,
      detail: `Device replied with hello=${hello.hello}`,
    });

    const activeProfileDoc = await sendSerialJson(
      serialClient,
      'GET_PROFILE',
      (json) => Number.isInteger(json.profile) && Array.isArray(json.slots),
      scenarioTimeoutMs,
    );
    activeProfile = activeProfileDoc.profile;

    originalProfile = await sendSerialJson(
      serialClient,
      `GET_PROFILE,${profileSlot}`,
      (json) => json.profile === profileSlot && Array.isArray(json.slots),
      scenarioTimeoutMs,
    );

    const baselineConfig = await sendSerialJson(
      serialClient,
      'GET_CONFIG',
      (json) => Array.isArray(json.pots),
      scenarioTimeoutMs,
    );
    baselinePot = baselineConfig.pots[potIndex];

    const savedChannel = baselinePot && Number.isInteger(baselinePot.channel) ? baselinePot.channel : 2;
    const savedCc = baselinePot && Number.isInteger(baselinePot.cc) ? baselinePot.cc : 74;
    const altChannel = savedChannel === 16 ? 15 : savedChannel + 1;
    const altCc = savedCc === 127 ? 0 : savedCc + 1;

    await sendSerialLine(
      serialClient,
      `SET_POT,${potIndex},${savedChannel},${savedCc}`,
      (line) => line === 'Pot configuration updated!',
    );
    const saveProfile = await sendSerialJson(
      serialClient,
      `SAVE_PROFILE,${profileSlot}`,
      (json) => json.profile_saved === true && json.profile === profileSlot,
      scenarioTimeoutMs,
    );

    await sendSerialLine(
      serialClient,
      `SET_POT,${potIndex},${altChannel},${altCc}`,
      (line) => line === 'Pot configuration updated!',
    );
    const loadProfile = await sendSerialJson(
      serialClient,
      `LOAD_PROFILE,${profileSlot}`,
      (json) => json.profile_loaded === true && json.profile === profileSlot,
      scenarioTimeoutMs,
    );
    const loadedConfig = await sendSerialJson(
      serialClient,
      'GET_CONFIG',
      (json) => Array.isArray(json.pots),
      scenarioTimeoutMs,
    );
    const loadedPot = loadedConfig.pots[potIndex];
    if (!loadedPot || loadedPot.channel !== savedChannel || loadedPot.cc !== savedCc) {
      throw new Error(
        `Profile load did not restore pot ${potIndex}; saw ${JSON.stringify(loadedPot)}`,
      );
    }

    const resetProfile = await sendSerialJson(
      serialClient,
      `RESET_PROFILE,${profileSlot}`,
      (json) => json.profile_reset === true && json.profile === profileSlot,
      scenarioTimeoutMs,
    );
    const resetConfig = await sendSerialJson(
      serialClient,
      'GET_CONFIG',
      (json) => Array.isArray(json.pots),
      scenarioTimeoutMs,
    );
    const resetPot = resetConfig.pots[potIndex];
    if (!resetPot || resetPot.channel !== 1 || resetPot.cc !== 0) {
      throw new Error(`Profile reset did not restore defaults; saw ${JSON.stringify(resetPot)}`);
    }

    scenarios.push({
      id: 'profile-storage',
      title: 'Profile save/load/reset survives a direct serial round-trip',
      ok: true,
      detail: `SAVE=${saveProfile.profile_saved} LOAD=${loadProfile.profile_loaded} RESET=${resetProfile.profile_reset} on profile slot ${profileSlot}`,
    });

    await sendSerialLine(
      serialClient,
      `SET_POT,${potIndex},${savedChannel},${savedCc}`,
      (line) => line === 'Pot configuration updated!',
    );
    const savedMacro = await sendSerialJson(
      serialClient,
      'SAVE_MACRO_SLOT',
      (json) => json.macro_saved === true,
      scenarioTimeoutMs,
    );
    await sendSerialLine(
      serialClient,
      `SET_POT,${potIndex},${altChannel},${altCc}`,
      (line) => line === 'Pot configuration updated!',
    );
    const recalledMacro = await sendSerialJson(
      serialClient,
      'RECALL_MACRO_SLOT',
      (json) => json.macro_recalled === true,
      scenarioTimeoutMs,
    );
    const macroConfig = await sendSerialJson(
      serialClient,
      'GET_CONFIG',
      (json) => Array.isArray(json.pots),
      scenarioTimeoutMs,
    );
    const macroPot = macroConfig.pots[potIndex];
    if (!macroPot || macroPot.channel !== savedChannel || macroPot.cc !== savedCc) {
      throw new Error(`Macro recall did not restore pot ${potIndex}; saw ${JSON.stringify(macroPot)}`);
    }

    scenarios.push({
      id: 'macro-storage',
      title: 'Macro snapshot save/recall restores live state',
      ok: true,
      detail: `macro_saved=${savedMacro.macro_saved} macro_recalled=${recalledMacro.macro_recalled}`,
    });

    const sceneName = `Bench ${sceneSlot + 1}`;
    const saveScene = await sendSerialJson(
      serialClient,
      JSON.stringify({ cmd: 'SAVE_SCENE', slot: sceneSlot, name: sceneName }),
      (json) => json.cmd === 'SAVE_SCENE' && json.scene_saved === true && json.scene_slot === sceneSlot,
      scenarioTimeoutMs,
    );
    await sendSerialLine(
      serialClient,
      `SET_POT,${potIndex},${altChannel},${altCc}`,
      (line) => line === 'Pot configuration updated!',
    );
    const recallScene = await sendSerialJson(
      serialClient,
      JSON.stringify({ cmd: 'RECALL_SCENE', slot: sceneSlot }),
      (json) =>
        json.cmd === 'RECALL_SCENE' && json.scene_recalled === true && json.scene_slot === sceneSlot,
      scenarioTimeoutMs,
    );
    const sceneList = await sendSerialJson(
      serialClient,
      JSON.stringify({ cmd: 'GET_SCENES' }),
      (json) => json.cmd === 'GET_SCENES' && Array.isArray(json.scenes),
      scenarioTimeoutMs,
    );
    const listedScene = sceneList.scenes.find((entry) => entry.slot === sceneSlot);
    if (!listedScene || !listedScene.available || listedScene.name !== saveScene.scene_name) {
      throw new Error(`Scene inventory does not match saved scene slot ${sceneSlot}`);
    }
    const sceneConfig = await sendSerialJson(
      serialClient,
      'GET_CONFIG',
      (json) => Array.isArray(json.pots),
      scenarioTimeoutMs,
    );
    const scenePot = sceneConfig.pots[potIndex];
    if (!scenePot || scenePot.channel !== savedChannel || scenePot.cc !== savedCc) {
      throw new Error(`Scene recall did not restore pot ${potIndex}; saw ${JSON.stringify(scenePot)}`);
    }

    scenarios.push({
      id: 'scene-storage',
      title: 'Scene save/recall and inventory reporting stay coherent',
      ok: true,
      detail: `scene_saved=${saveScene.scene_saved} scene_recalled=${recallScene.scene_recalled} on scene slot ${sceneSlot}`,
    });
  } finally {
    try {
      if (baselinePot) {
        await sendSerialLine(
          serialClient,
          `SET_POT,${potIndex},${baselinePot.channel},${baselinePot.cc}`,
          (line) => line === 'Pot configuration updated!',
          scenarioTimeoutMs,
        );
      }
      if (originalProfile && originalProfile.stored) {
        const restorePayload = JSON.stringify(originalProfile);
        await sendSerialLine(
          serialClient,
          `SET_PROFILE,${profileSlot},${restorePayload}`,
          (line) => line === 'OK',
          scenarioTimeoutMs,
        );
      }
      if (Number.isInteger(activeProfile)) {
        await sendSerialJson(
          serialClient,
          `LOAD_PROFILE,${activeProfile}`,
          (json) => json.profile_loaded === true && json.profile === activeProfile,
          scenarioTimeoutMs,
        );
      }
    } catch (err) {
      scenarios.push({
        id: 'storage-restore-warning',
        title: 'Storage smoke cleanup warning',
        ok: false,
        detail: err.message,
      });
    }
    await closeSerialClient(serialClient);
  }
}

async function main() {
  const bridgePath = path.resolve(__dirname, '../../bridge/mn42_bridge.js');
  const bridgeArgs = [
    bridgePath,
    '--serial',
    serialPath,
    '--osc',
    String(oscOutPort),
    '--osc-listen',
    String(oscInPort),
    '--host',
    oscHost,
    '--bind',
    oscBind,
    '--midi',
    midiLabel,
  ];

  console.log(`[system-test] ${now()} spinning up bridge: node ${bridgeArgs.join(' ')}`);
  const bridge = spawn('node', bridgeArgs, {
    stdio: ['ignore', 'pipe', 'pipe'],
  });

  const bridgeStdout = [];
  const bridgeStderr = [];
  bridge.stdout.on('data', (chunk) => {
    const text = chunk.toString();
    bridgeStdout.push(text);
    process.stdout.write(`[bridge] ${text}`);
  });
  bridge.stderr.on('data', (chunk) => {
    const text = chunk.toString();
    bridgeStderr.push(text);
    process.stderr.write(`[bridge:err] ${text}`);
  });

  let bridgeExitCode = null;
  bridge.once('exit', (code, signal) => {
    bridgeExitCode = code === null ? signal : code;
  });

  const rxPort = new osc.UDPPort({ localAddress: oscBind, localPort: oscOutPort });
  const txPort = new osc.UDPPort({ localAddress: oscBind, localPort: 0 });

  const observed = {
    slotsMessages: 0,
    envelopeMessages: 0,
    lastSlots: [],
  };

  rxPort.on('message', (message) => {
    if (message.address === '/mn42/slots') {
      observed.slotsMessages += 1;
      observed.lastSlots = parseArgs(message);
      console.log(`[system-test] slots[0..4]: ${observed.lastSlots.slice(0, 5).join(', ')}`);
    } else if (message.address === '/mn42/envelopes') {
      observed.envelopeMessages += 1;
      const vals = parseArgs(message);
      console.log(`[system-test] envelopes: ${vals.join(', ')}`);
    }
  });

  await Promise.all([waitForPortReady(rxPort), waitForPortReady(txPort)]);

  const scenarios = [];
  const startedAt = Date.now();

  try {
    // Scenario 1: handshake & stream
    const handshake = await waitForMessage(
      rxPort,
      '/mn42/slots',
      scenarioTimeoutMs,
      (_msg, values) => Array.isArray(values) && values.length > 0,
    );
    scenarios.push({
      id: 'handshake',
      title: 'Bridge hears HELLO and streams slots',
      ok: true,
      detail: `First slot frame held ${handshake.values.length} entries`,
    });

    const baselineSlots = handshake.values.slice();

    // Scenario 2: OSC command loop
    const targetSlot = 0;
    const newValue = baselineSlots[targetSlot] === 127 ? 0 : 127;
    const payload = { cmd: 'SET_SLOT_VALUE', slot: targetSlot, value: newValue };
    console.log(`[system-test] sending OSC command: ${JSON.stringify(payload)}`);
    txPort.send(
      {
        address: '/mn42/cmd',
        args: [
          {
            type: 's',
            value: JSON.stringify(payload),
          },
        ],
      },
      oscHost,
      oscInPort,
    );

    const update = await waitForMessage(
      rxPort,
      '/mn42/slots',
      commandTimeoutMs,
      (_msg, values) => values[targetSlot] === newValue,
    );
    scenarios.push({
      id: 'osc-loop',
      title: 'OSC command updates slot snapshot',
      ok: true,
      detail: `Slot ${targetSlot} moved ${baselineSlots[targetSlot]} -> ${update.values[targetSlot]}`,
    });

    // Scenario 3: keep-alive / ongoing stream
    await waitForMessage(
      rxPort,
      '/mn42/slots',
      scenarioTimeoutMs,
      (_msg, values) => values.length === observed.lastSlots.length,
    );
    scenarios.push({
      id: 'keep-alive',
      title: 'Streaming continues without extra HELLO',
      ok: true,
      detail: `Received ${observed.slotsMessages} slot frames and ${observed.envelopeMessages} envelope frames`,
    });

    // Restore baseline to avoid leaving the rig in a weird state.
    if (baselineSlots[targetSlot] !== newValue) {
      const revert = { cmd: 'SET_SLOT_VALUE', slot: targetSlot, value: baselineSlots[targetSlot] };
      console.log(`[system-test] restoring slot ${targetSlot}: ${JSON.stringify(revert)}`);
      txPort.send(
        {
          address: '/mn42/cmd',
          args: [
            {
              type: 's',
              value: JSON.stringify(revert),
            },
          ],
        },
        oscHost,
        oscInPort,
      );
    }
  } catch (err) {
    scenarios.push({
      id: `error-${scenarios.length + 1}`,
      title: 'Scenario failure',
      ok: false,
      detail: err.message,
    });
    console.error(`[system-test] Scenario failed: ${err.message}`);
  } finally {
    txPort.close();
    rxPort.close();
    bridge.kill('SIGINT');
    await new Promise((resolve) => setTimeout(resolve, 500));
  }

  try {
    await runStorageScenarios(scenarios);
  } catch (err) {
    scenarios.push({
      id: `storage-error-${scenarios.length + 1}`,
      title: 'Storage scenario failure',
      ok: false,
      detail: err.message,
    });
    console.error(`[system-test] Storage scenarios failed: ${err.message}`);
  }

  if (bridgeExitCode !== null) {
    console.log(`[system-test] bridge exit code: ${bridgeExitCode}`);
  }

  const durationMs = Date.now() - startedAt;
  const summary = {
    serial: serialPath,
    oscOutPort,
    oscInPort,
    scenarios,
    bridgeExitCode,
    durationMs,
  };

  if (reportPath) {
    try {
      fs.writeFileSync(reportPath, JSON.stringify(summary, null, 2));
      console.log(`[system-test] wrote report to ${reportPath}`);
    } catch (err) {
      console.error(`[system-test] failed to write report: ${err.message}`);
    }
  }

  const failures = scenarios.filter((scenario) => !scenario.ok);
  scenarios.forEach((scenario) => {
    const mark = scenario.ok ? 'PASS' : 'FAIL';
    console.log(`[system-test] ${mark} – ${scenario.title}: ${scenario.detail}`);
  });

  if (bridgeStdout.length) {
    console.log('[system-test] bridge stdout captured');
  }
  if (bridgeStderr.length) {
    console.log('[system-test] bridge stderr captured');
  }

  if (failures.length > 0) {
    process.exitCode = 1;
  }
}

main().catch((err) => {
  console.error(`[system-test] fatal: ${err.message}`);
  process.exit(1);
});
