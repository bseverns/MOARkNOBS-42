const { spawn } = require('node:child_process');
const { strict: assert } = require('node:assert');
const path = require('node:path');
const osc = require('osc');

async function run() {
  // Listen for bridge OSC output and make sure oversized telemetry still forwards.
  const listenPort = 57122;
  const udp = new osc.UDPPort({
    localAddress: '127.0.0.1',
    localPort: listenPort,
  });
  udp.open();
  await new Promise((resolve) => udp.on('ready', resolve));

  let slots = null;
  let envelopes = null;
  let child = null;

  try {
    const telemetrySeen = new Promise((resolve, reject) => {
      const timer = setTimeout(
        () =>
          reject(new Error('no forwarded telemetry from large serial payload')),
        1500,
      );
      udp.on('message', (msg) => {
        if (msg.address === '/mn42/slots') slots = msg.args;
        if (msg.address === '/mn42/envelopes') envelopes = msg.args;
        if (Array.isArray(slots) && Array.isArray(envelopes)) {
          clearTimeout(timer);
          resolve();
        }
      });
    });

    const script = path.join(__dirname, '..', 'mn42_bridge.js');
    const serialMock = path.join(__dirname, 'mock_serial_large.js');
    const jzzMock = path.join(__dirname, 'mock_jzz.js');
    child = spawn(process.execPath, [
      '-r',
      serialMock,
      '-r',
      jzzMock,
      script,
      '--serial',
      '/dev/fake',
      '--osc',
      String(listenPort),
      '--host',
      '127.0.0.1',
      '--osc-listen',
      '0',
    ]);

    await telemetrySeen;

    assert.equal(
      slots.length,
      42,
      'slots array should survive large serial frame',
    );
    assert.equal(
      envelopes.length,
      6,
      'envelopes array should survive large serial frame',
    );
    assert.equal(slots[0], 0);
    assert.equal(slots[41], 123);
    assert.equal(envelopes[0], 127);
    assert.equal(envelopes[5], 87);
  } finally {
    if (child) {
      child.kill();
    }
    udp.close();
  }

  console.log('large serial telemetry payload still forwards to OSC');
  process.exit(0);
}

run().catch((err) => {
  console.error(err);
  process.exit(1);
});
