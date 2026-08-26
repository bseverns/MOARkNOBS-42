'use strict';

const assert = require('assert');
const path = require('path');

const {
  buildStructuredWebSocketUrl,
  configDifferencePaths,
  parseBridgeConsoleUrl,
  redactControlToken,
  withControlToken,
} = require(path.resolve(
  __dirname,
  '../../firmware/system_test/mn42_bridge_session_runner.js',
));

const token = 'bench-token_123';
const consoleLine = `bridge console: http://127.0.0.1:8791/?token=${token}`;
const consoleUrl = parseBridgeConsoleUrl(consoleLine);

assert(consoleUrl, 'runner should parse the server console URL');
assert.strictEqual(consoleUrl.searchParams.get('token'), token);
assert.strictEqual(
  buildStructuredWebSocketUrl(consoleUrl),
  `ws://127.0.0.1:8791/ws/events?token=${token}`,
);
assert.deepStrictEqual(
  withControlToken({ method: 'POST', headers: { 'x-test': 'yes' } }, token),
  {
    method: 'POST',
    headers: {
      'x-test': 'yes',
      authorization: `Bearer ${token}`,
    },
  },
);
assert.strictEqual(
  redactControlToken(consoleLine),
  'bridge console: http://127.0.0.1:8791/?token=[redacted]',
);
assert.strictEqual(parseBridgeConsoleUrl('serial up'), null);
assert.deepStrictEqual(
  configDifferencePaths(
    { filter: { idle_floor: 24 }, slots: [{ data1: 1 }] },
    { filter: { idle_floor: 25 }, slots: [{ data1: 1 }] },
  ),
  ['$.filter.idle_floor'],
);

console.log('bridge session runner auth tests passed');
