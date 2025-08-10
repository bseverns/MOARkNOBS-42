#!/usr/bin/env node
// Punk-rock bridge connecting MOARkNOBS-42 to the outside world.
// serialport speaks USB, osc shouts over UDP, jzz slings MIDI.
// Callbacks and retry loops keep the party alive when links bail.

function usage() {
  // Show a tiny help banner for the command-line crowd.
  console.log(`mn42_bridge.js - link MOARkNOBS-42 to OSC & MIDI\n` +
    `Usage: node mn42_bridge.js [--serial PORT] [--osc PORT] [--bind ADDR] [--midi LABEL]`);
}

function getArg(flag, def) {
  // Walk argv for a flag and grab the value sitting next to it.
  const idx = process.argv.indexOf(flag);
  if (idx >= 0 && idx + 1 < process.argv.length) return process.argv[idx + 1];
  return def;
}

if (process.argv.includes('--help') || process.argv.includes('-h')) {
  // Bail early if someone just wants the manual.
  usage();
  process.exit(0);
}

// Pull CLI overrides or fall back to defaults.
const serialName = getArg('--serial', getArg('-s', '/dev/ttyACM0'));
const oscPort = parseInt(getArg('--osc', getArg('-o', '9000')), 10);
const oscBind = getArg('--bind', getArg('-b', '127.0.0.1'));
const midiLabel = getArg('--midi', getArg('-m', 'MN42 Bridge'));

async function main() {
  // Pull in the heavy lifters.
  const { SerialPort, ReadlineParser } = require('serialport');
  const osc = require('osc');
  const JZZ = require('jzz');

  // Fire up an OSC UDP port bound to the chosen address/port.
  const udp = new osc.UDPPort({ localAddress: '0.0.0.0', localPort: oscPort });
  udp.on('error', err => {
    // If the socket coughs, log it, slam it shut, and try again in a sec.
    console.error('udp error:', err.message);
    try { udp.close(); } catch {}
    setTimeout(() => udp.open(), 1000);
  });
  udp.open();

  // Wire up the serial link to the hardware.
  const serial = new SerialPort({ path: serialName, baudRate: 31250 });
  const parser = serial.pipe(new ReadlineParser({ delimiter: '\n' }));
  serial.on('open', () => serial.write('HELLO\n'));
  serial.on('error', err => {
    // Serial trouble? Whine, then poke it again after a beat.
    console.error('serial error:', err.message);
    setTimeout(() => serial.open(e => e && console.error('serial reconnect failed:', e.message)), 1000);
  });

  // Validate inbound commands before we spew them back out.
  const validCmd = m => m && typeof m.cmd === 'string' &&
    typeof m.slot === 'number' && typeof m.value === 'number';

  // MIDI setup lives in a reconnect loop because ports come and go.
  const midi = JZZ();
  let midiOut, midiIn;
  function connectMidi() {
    // Try to grab a MIDI out port.
    midiOut = midi.openMidiOut(midiLabel).or(() => {
      console.error('MIDI out failed');
      setTimeout(connectMidi, 1000);
    });
    if (midiOut && typeof midiOut.on === 'function') {
      midiOut.on('error', err => {
        // If the port dies mid-set, complain and retry.
        console.error('MIDI out error:', err.message);
        setTimeout(connectMidi, 1000);
      });
    }
    // And the matching MIDI in port.
    midiIn = midi.openMidiIn(midiLabel).or(() => {
      console.error('MIDI in failed');
      setTimeout(connectMidi, 1000);
    });
    if (midiIn && typeof midiIn.on === 'function') {
      midiIn.on('error', err => {
        console.error('MIDI in error:', err.message);
        setTimeout(connectMidi, 1000);
      });
    }
    // Pipe incoming MIDI CCs back to the hardware.
    midiIn && midiIn.connect(msg => {
      const arr = msg.toArray();
      if ((arr[0] & 0xF0) === 0xB0) {
        const cmd = { cmd: 'SET_POT', slot: arr[1], value: arr[2] };
        serial.write(JSON.stringify(cmd) + '\n');
      }
    });
  }
  connectMidi();

  // Listen for OSC commands coming in hot.
  udp.on('message', msg => {
    if (msg.address === '/mn42/cmd' && msg.args.length) {
      let data = msg.args[0];
      if (typeof data === 'string') {
        try { data = JSON.parse(data); } catch { return; }
      }
      if (validCmd(data)) serial.write(JSON.stringify(data) + '\n');
    }
  });

  // Relay serial reports back out over OSC and MIDI.
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

// Top-level catch: if all else fails, crash loud.
main().catch(err => {
  console.error('bridge failed:', err.message);
  process.exit(1);
});
