const { strict: assert } = require('node:assert');

const {
  createMidiTransportLifecycle,
} = require('../lib/transports/midi_transport');

async function run() {
  const logs = [];
  let midiIn = 'unset';
  let midiOut = 'unset';

  const lifecycle = createMidiTransportLifecycle({
    getJzzFactory: () => () => ({
      info: () => ({
        inputs: [],
        outputs: [{ name: 'Apple DLS Synth' }],
      }),
      openMidiOut: () => ({
        or(callback) {
          setImmediate(() =>
            callback.call({
              _err: () => 'Port "MN42 Bridge" not found',
            }),
          );
          return this;
        },
        on() {},
      }),
      openMidiIn: () => ({
        or(callback) {
          setImmediate(() =>
            callback.call({
              _err: () => 'Port "MN42 Bridge" not found',
            }),
          );
          return this;
        },
        connect() {},
        on() {},
      }),
    }),
    getConfig: () => ({ midiLabel: 'MN42 Bridge' }),
    getRuntimeState: () => ({
      running: true,
      stopping: false,
      manualStop: false,
    }),
    setMidiIn: (next) => {
      midiIn = next;
    },
    setMidiOut: (next) => {
      midiOut = next;
    },
    pushLog: (level, message) => logs.push({ level, message }),
    createMessageHandler: () => () => {},
  });

  lifecycle.attachMidi();
  await new Promise((resolve) => setTimeout(resolve, 20));
  lifecycle.cancelRetry();

  assert.equal(midiIn, null, 'failed MIDI input should be cleared');
  assert.equal(midiOut, null, 'failed MIDI output should be cleared');
  assert.ok(
    logs.some(
      (entry) =>
        entry.level === 'error' &&
        entry.message.includes('MIDI out "MN42 Bridge" failed') &&
        entry.message.includes('Apple DLS Synth'),
    ),
    'MIDI output failure should include the missing label and available outputs',
  );
  assert.ok(
    logs.some(
      (entry) =>
        entry.level === 'error' &&
        entry.message.includes('MIDI in "MN42 Bridge" failed') &&
        entry.message.includes('Available inputs: none'),
    ),
    'MIDI input failure should say when no inputs exist',
  );

  console.log('MIDI open failures include actionable port details');
}

run().catch((err) => {
  console.error(err);
  process.exit(1);
});
