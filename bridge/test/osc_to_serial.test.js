const { strict: assert } = require('node:assert');
const osc = require('osc');
const path = require('node:path');

async function run() {
  // Hijack serialport to use its mock so we can spy on writes without real hardware.
  const sp = require('serialport');
  const { SerialPortMock, ReadlineParser } = sp;
  require.cache[require.resolve('serialport')].exports = { ...sp, SerialPort: SerialPortMock, ReadlineParser };
  SerialPortMock.binding.createPort('/dev/fake', {});

  const writes = [];
  const origWrite = SerialPortMock.prototype.write;
  SerialPortMock.prototype.write = function(data, cb) {
    writes.push(data.toString());
    return origWrite.call(this, data, cb);
  };

  // Stub out JZZ so MIDI gets looped back into the bridge.
  let midiHandler;
  function JZZ() {
    return {
      openMidiOut: () => ({ or() { return this; }, send() {}, on() {} }),
      openMidiIn: () => ({ or() { return this; }, connect(cb) { midiHandler = cb; return this; }, on() {} }),
    };
  }
  require.cache[require.resolve('jzz')] = { exports: JZZ };

  // Boot the bridge pointing at our fake ports.
  process.argv = [process.execPath, path.join(__dirname, '..', 'mn42_bridge.js'), '--serial', '/dev/fake', '--osc', '9701'];
  require('../mn42_bridge.js');
  await new Promise(r => setTimeout(r, 100)); // let UDP and serial settle

  // Fire an OSC command and ensure it becomes a serial JSON line.
  const udp = new osc.UDPPort({ localAddress: '127.0.0.1', localPort: 0, remoteAddress: '127.0.0.1', remotePort: 9701 });
  udp.open();
  await new Promise(resolve => udp.on('ready', resolve));
  const cmd = { cmd: 'SET_POT', slot: 2, value: 42 };
  udp.send({ address: '/mn42/cmd', args: JSON.stringify(cmd) });
  await new Promise(r => setTimeout(r, 100));
  assert.ok(writes.includes(JSON.stringify(cmd) + '\n'), 'OSC cmd should hit serial');

  // Now spoof a MIDI CC that should trigger the same JSON payload.
  midiHandler({ toArray: () => [0xB0, 3, 77] });
  await new Promise(r => setTimeout(r, 100));
  assert.ok(writes.includes('{"cmd":"SET_POT","slot":3,"value":77}\n'), 'MIDI CC should forward SET_POT JSON');

  udp.close();
  console.log('OSC and MIDI shove JSON at serial like champs');
  process.exit(0);
}

run().catch(err => { console.error(err); process.exit(1); });
