const { spawnSync } = require('node:child_process');
const { strict: assert } = require('node:assert');
const path = require('node:path');

const script = path.join(__dirname, '..', 'mn42_bridge.js');
const result = spawnSync(process.execPath, [script, '--help'], { encoding: 'utf8' });

assert.equal(result.status, 0, 'script should exit cleanly');
assert.match(result.stdout, /Usage:/, 'help text should mention usage');

console.log('mn42_bridge --help prints usage and exits without drama');
