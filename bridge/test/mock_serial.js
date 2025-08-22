const sp = require('serialport');
const { SerialPortMock, ReadlineParser } = sp;

// Hijack the serialport module so the bridge gets our tame mock instead.
require.cache[require.resolve('serialport')].exports = {
  ...sp,
  SerialPort: SerialPortMock,
  ReadlineParser,
};

// Pre-bake a faux hardware port loaded with a handshake and some slot data.
SerialPortMock.binding.createPort('/dev/fake', {
  echo: false,
  record: false,
  readyData: '{"hello":"mn42"}\n{"slots":[1,2,3]}\n'
});
