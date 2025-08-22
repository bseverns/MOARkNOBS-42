const sp = require('serialport');
const { ReadlineParser } = sp;
const { EventEmitter } = require('node:events');
const { PassThrough } = require('node:stream');

// Fake serial port that drops connection once then reports any reopen attempts.
class FakeSerialPort extends EventEmitter {
  constructor(opts, cb) {
    super();
    this._opens = 0;
    setImmediate(() => this.open(cb));
  }
  open(cb) {
    if (this._opens++) process.stdout.write('reopen\n');
    cb && cb();
    setImmediate(() => {
      this.emit('open');
      if (this._opens === 1) setTimeout(() => this.emit('close'), 10);
    });
  }
  write() {}
  pipe() { return new PassThrough(); }
}

// Replace serialport with our minimal troublemaker.
require.cache[require.resolve('serialport')].exports = {
  ...sp,
  SerialPort: FakeSerialPort,
  ReadlineParser,
};

