const { strict: assert } = require('node:assert');
const { openBrowser, shouldOpenBrowser } = require('../lib/open_browser');

assert.equal(shouldOpenBrowser(['node', 'server.js'], false), false);
assert.equal(
  shouldOpenBrowser(['node', 'server.js', '--open-browser'], false),
  true,
);
assert.equal(shouldOpenBrowser(['server'], true), true);
assert.equal(shouldOpenBrowser(['server', '--no-open-browser'], true), false);

const cases = [
  ['darwin', 'open', ['http://127.0.0.1:8787/']],
  ['win32', 'cmd', ['/c', 'start', '', 'http://127.0.0.1:8787/']],
  ['linux', 'xdg-open', ['http://127.0.0.1:8787/']],
];

for (const [platform, command, args] of cases) {
  let actual;
  let unrefCalled = false;
  openBrowser('http://127.0.0.1:8787/', {
    platform,
    spawn(commandValue, argsValue, optionsValue) {
      actual = [commandValue, argsValue, optionsValue];
      return { unref: () => (unrefCalled = true) };
    },
  });
  assert.deepEqual(actual, [
    command,
    args,
    { detached: true, stdio: 'ignore' },
  ]);
  assert.equal(unrefCalled, true);
}

console.log(
  'bridge console opens the native browser without a new runtime dependency',
);
