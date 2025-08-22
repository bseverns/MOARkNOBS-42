const { spawnSync } = require('node:child_process');
const { strict: assert } = require('node:assert');
const path = require('node:path');

// This test roughs up the CLI with `--help` because that's the fastest way to
// prove the argument parser even boots. If the bridge can't explain itself on
// demand, something's deeply wrong.
const script = path.join(__dirname, '..', 'mn42_bridge.js');
const mock = path.join(__dirname, 'mock_jzz.js');
const result = spawnSync(process.execPath, ['-r', mock, script, '--help'], { encoding: 'utf8' }); // snag stdout/stderr for inspection

// It should exit gracefully and spit out a usage banner. Anything else means
// the CLI wiring is busted.
assert.equal(result.status, 0, 'script should exit cleanly'); // non-zero exit means it crashed, not cool
assert.match(result.stdout, /Usage:/, 'help text should mention usage'); // the help text needs a Usage line or it's useless

console.log('mn42_bridge --help prints usage and exits without drama'); // victory lap message
