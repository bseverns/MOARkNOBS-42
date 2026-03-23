#!/usr/bin/env node

const { spawnSync } = require('node:child_process');
const path = require('node:path');

const bridgeEntry = path.resolve(__dirname, '..', 'mn42_bridge_server.js');

function run(args) {
  return spawnSync(process.execPath, [bridgeEntry, ...args], {
    encoding: 'utf8',
    timeout: 8000,
  });
}

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

console.log('browser bridge server smoke checks passed');
