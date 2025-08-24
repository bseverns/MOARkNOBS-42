const { strict: assert } = require('node:assert');
const osc = require('osc');
const path = require('node:path');

// Hurl a bunch of OSC commands at the bridge and make sure it only forwards
// the ones that play by the rules. Anything sketchy should get tossed before
// it ever hits the wire.
async function run() {
  // yank in serialport and swap in its mock so we don't touch real hardware
  const sp = require('serialport');
  const { SerialPortMock, ReadlineParser } = sp;
  // patch the module cache so every require('serialport') gets our mock
  require.cache[require.resolve('serialport')].exports = {
    ...sp,
    SerialPort: SerialPortMock,
    ReadlineParser,
  };
  SerialPortMock.binding.createPort('/dev/fake', {}); // fake USB port to keep the bridge happy

  // hijack writes so we can spy on what actually goes out the "serial" line
  const writes = [];
  const origWrite = SerialPortMock.prototype.write;
  SerialPortMock.prototype.write = function (data, cb) {
    writes.push(data.toString());
    return origWrite.call(this, data, cb);
  };

  // ghost out JZZ so the bridge thinks MIDI is alive and well
  function JZZ() {
    return {
      openMidiOut: () => ({
        or() {
          return this;
        },
        send() {},
        on() {},
      }),
      openMidiIn: () => ({
        or() {
          return this;
        },
        connect() {
          return this;
        },
        on() {},
      }),
    };
  }
  require.cache[require.resolve('jzz')] = { exports: JZZ };

  // fire up the bridge in serial+OSC mode aimed at our fake port
  process.argv = [
    process.execPath,
    path.join(__dirname, '..', 'mn42_bridge.js'),
    '--serial',
    '/dev/fake',
    '--osc',
    '9711',
    '--osc-listen',
    '9000',
    '--host',
    '127.0.0.1',
  ];
  require('../mn42_bridge.js');
  await new Promise((r) => setTimeout(r, 100)); // give it a tick to boot

  // open a UDP channel so we can lob OSC packets at the bridge
  const udp = new osc.UDPPort({
    localAddress: '127.0.0.1',
    localPort: 0,
    remoteAddress: '127.0.0.1',
    remotePort: 9000,
  });
  udp.open();
  await new Promise((resolve) => udp.on('ready', resolve));

  // these commands are squeaky clean and should forward without complaint
  const good = [
    { cmd: 'SET_POT', slot: 0, value: 0 },
    { cmd: 'SET_POT', slot: 41, value: 127 },
  ];
  good.forEach((c) =>
    udp.send({ address: '/mn42/cmd', args: JSON.stringify(c) }),
  );

  // these are mangled in one way or another and should get dropped cold
  const bad = [
    { cmd: 'SET_POT', slot: -1, value: 0 }, // slot too small
    { cmd: 'SET_POT', slot: 42, value: 0 }, // slot too big
    { cmd: 'SET_POT', slot: 0, value: 128 }, // value out of range
    { cmd: 'SET_POT', slot: 0, value: 0, junk: 'x'.repeat(200) }, // payload over 128B
  ];
  bad.forEach((c) =>
    udp.send({ address: '/mn42/cmd', args: JSON.stringify(c) }),
  );
  udp.send({ address: '/mn42/cmd', args: 'notjson' }); // and one that's not even JSON

  await new Promise((r) => setTimeout(r, 100));
  udp.close();

  // pluck out the JSON writes and make sure only the good ones made it through
  const forwards = writes.filter((w) => w.startsWith('{'));
  assert.deepEqual(
    forwards,
    good.map((c) => JSON.stringify(c) + '\n'),
    'only in-range cmds should forward',
  );
  console.log('validation clamps size and range before forwarding');
  process.exit(0);
}

run().catch((err) => {
  console.error(err);
  process.exit(1);
});
