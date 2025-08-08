#!/usr/bin/env node

function usage() {
  console.log(`mn42_bridge.js - link MOARkNOBS-42 to OSC & MIDI\n` +
`Usage: node mn42_bridge.js [--serial PORT] [--osc PORT] [--midi LABEL]`);
}

function getArg(flag, def) {
  const idx = process.argv.indexOf(flag);
  if (idx >= 0 && idx + 1 < process.argv.length) return process.argv[idx + 1];
  return def;
}

if (process.argv.includes('--help') || process.argv.includes('-h')) {
  usage();
  process.exit(0);
}

const serialName = getArg('--serial', getArg('-s', '/dev/ttyACM0'));
const oscPort = parseInt(getArg('--osc', getArg('-o', '9000')), 10);
const midiLabel = getArg('--midi', getArg('-m', 'MN42 Bridge'));

async function main() {
  const { SerialPort, ReadlineParser } = require('serialport');
  const osc = require('osc');
  const JZZ = require('jzz');

  const udp = new osc.UDPPort({ localAddress: '0.0.0.0', localPort: oscPort });
  udp.open();

  const midi = JZZ();
  const midiOut = midi.openMidiOut(midiLabel).or(() => console.error('MIDI out failed'));
  const midiIn = midi.openMidiIn(midiLabel).or(() => console.error('MIDI in failed'));

  const serial = new SerialPort({ path: serialName, baudRate: 31250 });
  const parser = serial.pipe(new ReadlineParser({ delimiter: '\n' }));
  serial.on('open', () => serial.write('HELLO\n'));

  midiIn && midiIn.connect(msg => {
    const arr = msg.toArray();
    if ((arr[0] & 0xF0) === 0xB0) {
      const cmd = { cmd: 'SET_POT', slot: arr[1], value: arr[2] };
      serial.write(JSON.stringify(cmd) + '\n');
    }
  });

  udp.on('message', msg => {
    if (msg.address === '/mn42/cmd' && msg.args.length) {
      serial.write(JSON.stringify(msg.args[0]) + '\n');
    }
  });

  let ready = false;
  parser.on('data', line => {
    line = line.trim();
    if (!ready) {
      if (line === '{"hello":"mn42"}') ready = true;
      return;
    }
    let data;
    try { data = JSON.parse(line); } catch { return; }
    if (data.slots) {
      udp.send({ address: '/mn42/slots', args: data.slots });
      midiOut && data.slots.forEach((v, i) => midiOut.send([0xB0, i, v]));
    }
    if (data.envelopes) {
      udp.send({ address: '/mn42/envelopes', args: data.envelopes });
      midiOut && data.envelopes.forEach((v, i) => midiOut.send([0xB1, i, v]));
    }
  });
}

main().catch(err => {
  console.error('bridge failed:', err.message);
  process.exit(1);
});
