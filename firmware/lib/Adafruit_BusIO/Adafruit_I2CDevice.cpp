#include "Adafruit_I2CDevice.h"

Adafruit_I2CDevice::Adafruit_I2CDevice(uint8_t addr, TwoWire *theWire)
    : _wire(theWire), _addr(addr), _begun(false) {}

bool Adafruit_I2CDevice::begin(bool addr_detect) {
  _wire->begin();
  _begun = true;
  if (addr_detect) {
    _wire->beginTransmission(_addr);
    return _wire->endTransmission() == 0;
  }
  return true;
}

bool Adafruit_I2CDevice::detected() {
  _wire->beginTransmission(_addr);
  return _wire->endTransmission() == 0;
}

bool Adafruit_I2CDevice::read(uint8_t *buffer, size_t len) {
  if (!_begun) begin();
  size_t idx = 0;
  _wire->requestFrom((int)_addr, (int)len);
  while (_wire->available() && idx < len) {
    buffer[idx++] = _wire->read();
  }
  return idx == len;
}

bool Adafruit_I2CDevice::write(const uint8_t *buffer, size_t len, bool stop,
                               const uint8_t *prefix, size_t prefix_len) {
  if (!_begun) begin();
  _wire->beginTransmission(_addr);
  if (prefix && prefix_len) {
    _wire->write(prefix, prefix_len);
  }
  _wire->write(buffer, len);
  return _wire->endTransmission(stop) == 0;
}

bool Adafruit_I2CDevice::write_then_read(const uint8_t *write_buffer,
                                         size_t write_len, uint8_t *read_buffer,
                                         size_t read_len, bool stop) {
  if (!write(write_buffer, write_len, stop)) {
    return false;
  }
  return read(read_buffer, read_len);
}

bool Adafruit_I2CDevice::setSpeed(uint32_t speed) {
  // Teensy's TwoWire always exposes setClock(), but it's a method, not a
  // function pointer.  The original BusIO checked for its existence at
  // runtime, which trips up strict compilers.  Slam the clock unconditionally
  // and trust the platform; if it doesn't implement setClock() we'll learn
  // about it at link time.
  _wire->setClock(speed);
  return true;
}
