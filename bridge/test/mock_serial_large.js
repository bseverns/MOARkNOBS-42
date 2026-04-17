const sp = require('serialport');
const { SerialPortMock, ReadlineParser } = sp;

// Hijack serialport so the bridge sees our deterministic large-payload fake port.
require.cache[require.resolve('serialport')].exports = {
  ...sp,
  SerialPort: SerialPortMock,
  ReadlineParser,
};

const slots = Array.from({ length: 42 }, (_, index) => (index * 3) % 128);
const envelopes = Array.from(
  { length: 6 },
  (_, index) => (127 - index * 8) % 128,
);
const slotArgs = slots.map((_, index) => ({
  enabled: index % 2 === 0,
  method: index % 14,
  sourceA: index % 6,
  sourceB: (index + 1) % 6,
}));

const telemetry = JSON.stringify({
  type: 'telemetry',
  slots,
  envelopes,
  slotArgs,
  diagnostics: {
    loop_overruns: 0,
    midi_drops: 0,
    loop_max_us: 721,
    midi_isr_max_us: 288,
  },
});

SerialPortMock.binding.createPort('/dev/fake', {
  echo: false,
  record: false,
  readyData: `{"hello":"mn42"}\n${telemetry}\n`,
});
