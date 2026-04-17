#!/usr/bin/env node

const { spawnSync } = require('node:child_process');
const path = require('node:path');

const bridgeEntry = path.resolve(__dirname, '..', 'mn42_bridge_server.js');

// Invoke the browser-bridge server entrypoint with a small timeout so smoke
// checks fail fast in CI.
function run(args) {
  return spawnSync(process.execPath, [bridgeEntry, ...args], {
    encoding: 'utf8',
    timeout: 8000,
  });
}

// Print useful process output before exiting the smoke script on failure.
function fail(message, proc) {
  console.error(`FAIL: ${message}`);
  if (proc) {
    console.error(`  status: ${proc.status}`);
    if (proc.stdout) console.error(`  stdout: ${proc.stdout.trim()}`);
    if (proc.stderr) console.error(`  stderr: ${proc.stderr.trim()}`);
    if (proc.error) console.error(`  error: ${proc.error.message}`);
  }
  process.exit(1);
}

const help = run(['--help']);
if (help.status !== 0) {
  fail('--help must exit 0', help);
}
if (!help.stdout.includes('Usage: node mn42_bridge_server.js')) {
  fail('--help output must include server usage banner', help);
}

const badOsc = run(['--osc', 'banana']);
if (badOsc.status !== 1) {
  fail('invalid --osc must exit 1', badOsc);
}
if (!badOsc.stderr.includes('bad --osc port')) {
  fail('invalid --osc must print validation error', badOsc);
}

const badAlertSuppression = run(['--alert-suppression-ms', 'banana']);
if (badAlertSuppression.status !== 1) {
  fail('invalid --alert-suppression-ms must exit 1', badAlertSuppression);
}
if (!badAlertSuppression.stderr.includes('bad --alert-suppression-ms')) {
  fail(
    'invalid --alert-suppression-ms must print validation error',
    badAlertSuppression,
  );
}

console.log('browser bridge server smoke checks passed');
