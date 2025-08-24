const { spawn } = require('node:child_process');
const { strict: assert } = require('node:assert');
const path = require('node:path');

// Boot the bridge with a sketchy serial mock that drops the line, then make
// sure it claws its way back by reopening the port.
const script = path.join(__dirname, '..', 'mn42_bridge.js');
const serialMock = path.join(__dirname, 'mock_serial_close.js');
const jzzMock = path.join(__dirname, 'mock_jzz.js');

const child = spawn(process.execPath, [
  '-r',
  serialMock,
  '-r',
  jzzMock,
  script,
  '--serial',
  '/dev/fake',
  '--host',
  '127.0.0.1',
  '--osc-listen',
  '0',
]);

let reopened = false;
child.stdout.on('data', (data) => {
  if (data.toString().includes('reopen')) {
    reopened = true;
    child.kill();
  }
});

child.once('exit', () => {
  assert.ok(reopened, 'bridge should try to reopen after close');
  console.log('serial close triggers a reopen—punk rock resilience');
});

child.once('error', (err) => {
  console.error(err.message || err);
  process.exit(1);
});

setTimeout(() => {
  if (!reopened) {
    console.error('no reopen attempt detected');
    child.kill();
  }
}, 1500);
