'use strict';

const assert = require('assert');
const path = require('path');

const { renderMarkdown } = require(path.resolve(
  __dirname,
  '../../firmware/system_test/mn42_live_controls_runner.js',
));

const failed = renderMarkdown({
  result: 'failed',
  error: 'serial device unavailable',
  live_controls: {},
  config_stability: {},
});

assert.match(failed, /## Failure/);
assert.match(failed, /does not prove firmware identity/);
assert.doesNotMatch(failed, /## Proven/);
assert.doesNotMatch(failed, /undefined/);

const passed = renderMarkdown({
  result: 'passed',
  live_controls: {
    usb_midi: {
      baseline: { usb_midi_out: true },
      mutated: { usb_midi_out: false },
      restored: { usb_midi_out: true },
    },
    note_dynamics: {
      baseline: { velocity_shift: 0, change_probability: 100 },
      mutated: { velocity_shift: -12, change_probability: 83 },
      restored: { velocity_shift: 0, change_probability: 100 },
    },
    jitter: {
      baseline: { depth: 1, smoothness: 0.5 },
      mutated: { depth: 0.25, smoothness: 0.75 },
      restored: { depth: 1, smoothness: 0.5 },
    },
    clock: {
      baseline: { follow_external: true, clock_out_enabled: false, tapped_bpm: 120 },
      mutated: { follow_external: false, clock_out_enabled: true, tapped_bpm: 123.5 },
      restored: { follow_external: true, clock_out_enabled: false, tapped_bpm: 120 },
    },
  },
  config_stability: { baseline_hash: 'before', final_hash: 'before', stable: true },
});

assert.match(passed, /## Proven/);
assert.match(passed, /Stable: yes/);

console.log('live controls receipt tests passed');
