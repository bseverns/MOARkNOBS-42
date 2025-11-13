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

const args = process.argv.slice(2);
function argValue(flag, fallback) {
  const index = args.indexOf(flag);
  if (index >= 0 && index + 1 < args.length) {
    return args[index + 1];
  }
  return fallback;
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
    const payload = { cmd: 'SET_POT', slot: targetSlot, value: newValue };
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
      const revert = { cmd: 'SET_POT', slot: targetSlot, value: baselineSlots[targetSlot] };
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
