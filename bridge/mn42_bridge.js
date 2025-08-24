#!/usr/bin/env node
// Punk-rock bridge connecting MOARkNOBS-42 to the outside world.
// serialport speaks USB, osc shouts over UDP, jzz slings MIDI.
// Callbacks and retry loops keep the party alive when links bail.

// Let yargs boss around our CLI flags.
const yargs = require('yargs/yargs');
const { hideBin } = require('yargs/helpers');
const argv = yargs(hideBin(process.argv))
  .scriptName('mn42_bridge.js')
  .usage(
    'mn42_bridge.js - link MOARkNOBS-42 to OSC & MIDI\n' +
      'Usage: $0 [--serial PORT] [--osc PORT] [--bind ADDR] [--midi LABEL]',
  )
  .option('serial', {
    alias: 's',
    type: 'string',
    describe: 'serial port to poke',
    default: '/dev/ttyACM0',
  })
  .option('osc', {
    alias: 'o',
    type: 'number',
    describe: 'remote OSC port to scream at',
    default: 9000,
  })
  .option('bind', {
    alias: 'b',
    type: 'string',
    describe: 'local address to bind the OSC listener to',
    default: '127.0.0.1',
  })
  .option('midi', {
    alias: 'm',
    type: 'string',
    describe: 'label for the virtual MIDI port',
    default: 'MN42 Bridge',
  })
  .help()
  .alias('h', 'help')
  .parseSync();

// Pull CLI overrides or fall back to defaults.
const { serial: serialName, osc: oscPort, bind: oscBind, midi: midiLabel } = argv;

async function main() {
  // Pull in the heavy lifters.
  const { SerialPort, ReadlineParser } = require('serialport');
  const osc = require('osc');
  const JZZ = require('jzz');

  // Fire up an OSC UDP port. Bind the local port (default 9000, tweak with
  // --osc-listen) and aim outgoing packets at whatever --osc points to.
  const udp = new osc.UDPPort({ localAddress: oscBind, localPort: oscListen });
  udp.on('error', (err) => {
    // If the socket coughs, log it, slam it shut, and try again in a sec.
    console.error('udp error:', err.message);
    try {
      udp.close();
    } catch (err) {
      void err;
    }
    setTimeout(() => udp.open(), 1000);
  });
  udp.open();

  // Wire up the serial link to the hardware.
  const serial = new SerialPort({ path: serialName, baudRate: 31250 });
  const parser = serial.pipe(new ReadlineParser({ delimiter: '\n' }));
  serial.on('open', () => serial.write('HELLO\n'));
  serial.on('error', (err) => {
    // Serial trouble? Whine, then poke it again after a beat.
    console.error('serial error:', err.message);
    setTimeout(
      () =>
        serial.open(
          (e) => e && console.error('serial reconnect failed:', e.message),
        ),
      1000,
    );
  });
  serial.on('close', () => {
    // Cable yanked? Grumble and try to crawl back after a tick.
    console.error('serial disconnected');
    setTimeout(
      () =>
        serial.open(
          (e) => e && console.error('serial reconnect failed:', e.message),
        ),
      1000,
    );
  });

  // Validate inbound commands before we spew them back out.
  const validCmd = (m) =>
    m &&
    typeof m.cmd === 'string' &&
    typeof m.slot === 'number' &&
    typeof m.value === 'number';

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
      midiOut.on('error', (err) => {
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
      midiIn.on('error', (err) => {
        console.error('MIDI in error:', err.message);
        setTimeout(connectMidi, 1000);
      });
    }
    // Pipe incoming MIDI CCs back to the hardware.
    midiIn &&
      midiIn.connect((msg) => {
        const arr = msg.toArray();
        if ((arr[0] & 0xf0) === 0xb0) {
          const cmd = { cmd: 'SET_POT', slot: arr[1], value: arr[2] };
          serial.write(JSON.stringify(cmd) + '\n');
        }
      });
  }
  connectMidi();

  // Listen for OSC commands coming in hot.
  udp.on('message', (msg) => {
    if (msg.address === '/mn42/cmd' && msg.args.length) {
      let data = msg.args[0];
      if (typeof data === 'string') {
        try {
          data = JSON.parse(data);
        } catch {
          return;
        }
      }
      if (validCmd(data)) serial.write(JSON.stringify(data) + '\n');
    }
  });

  // Relay serial reports back out over OSC and MIDI.
  let ready = false;
  parser.on('data', (line) => {
    line = line.trim();
    if (!ready) {
      if (line === '{"hello":"mn42"}') ready = true;
      return;
    }
    let data;
    try {
      data = JSON.parse(line);
    } catch {
      return;
    }
    if (data.slots) {
      udp.send({ address: '/mn42/slots', args: data.slots }, oscBind, oscPort);
      midiOut && data.slots.forEach((v, i) => midiOut.send([0xb0, i, v]));
    }
    if (data.envelopes) {
      udp.send(
        { address: '/mn42/envelopes', args: data.envelopes },
        oscBind,
        oscPort,
      );
      midiOut && data.envelopes.forEach((v, i) => midiOut.send([0xb1, i, v]));
    }
  });
}

// Top-level catch: if all else fails, crash loud.
main().catch((err) => {
  console.error('bridge failed:', err.message);
  process.exit(1);
});
