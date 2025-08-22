const { spawn } = require('node:child_process');
const { strict: assert } = require('node:assert');
const path = require('node:path');
const osc = require('osc');

async function run() {
  // Fire up an OSC listener to catch whatever the bridge screams out.
  const listenPort = 57121;
  const udp = new osc.UDPPort({
    localAddress: '127.0.0.1',
    localPort: listenPort,
  });
  udp.open();
  await new Promise((resolve) => udp.on('ready', resolve));

  let slots;
  udp.on('message', (msg) => {
    if (msg.address === '/mn42/slots') slots = msg.args;
  });

  const script = path.join(__dirname, '..', 'mn42_bridge.js');
  const serialMock = path.join(__dirname, 'mock_serial.js');
  const jzzMock = path.join(__dirname, 'mock_jzz.js');
  const child = spawn(process.execPath, [
    '-r',
    serialMock,
    '-r',
    jzzMock,
    script,
    '--serial',
    '/dev/fake',
    '--osc',
    String(listenPort),
  ]);

  // Wait for the bridge to spit out an OSC packet or time out.
  await new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error('no OSC data')), 1000);
    udp.on('message', () => {
      clearTimeout(timer);
      resolve();
    });
  });

  assert.deepEqual(slots, [1, 2, 3], 'bridge should echo slots via OSC');

  child.kill();
  udp.close();
  console.log('serial JSON turns into OSC, as foretold');
}

run().catch((err) => {
  console.error(err);
  process.exit(1);
});
