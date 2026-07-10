const { strict: assert } = require('node:assert');

const {
  parseMajor,
  run,
  validateNodeVersion,
} = require('../../tools/require_node_24');

function captureRun(version) {
  let stderr = '';
  const status = run(version, {
    stderr: {
      write(chunk) {
        stderr += chunk;
      },
    },
  });
  return { status, stderr };
}

assert.equal(parseMajor('v24.13.0'), 24);
assert.equal(validateNodeVersion('24.0.0').ok, true);
assert.equal(validateNodeVersion('24.99.0').ok, true);
assert.equal(validateNodeVersion('20.11.1').ok, false);
assert.equal(validateNodeVersion('25.0.0').ok, false);

const rejected = captureRun('20.11.1');
assert.equal(rejected.status, 1);
assert.match(rejected.stderr, /require Node 24\.x/);
assert.match(rejected.stderr, /current Node is 20\.11\.1/);

const accepted = captureRun('24.13.0');
assert.equal(accepted.status, 0);
assert.equal(accepted.stderr, '');

console.log('Node 24 preflight rejects unsupported runtimes clearly');
