#include "Adafruit_BusIO_Register.h"
#include <string.h>

Adafruit_BusIO_Register::Adafruit_BusIO_Register(Adafruit_I2CDevice *i2cdevice,
                                                 uint16_t regaddr,
                                                 uint8_t width, uint8_t bitorder)
    : _i2cdevice(i2cdevice), _spidevice(nullptr), _addr(regaddr),
      _width(width), _bitorder(bitorder) {}

Adafruit_BusIO_Register::Adafruit_BusIO_Register(Adafruit_SPIDevice *spidevice,
                                                 uint16_t regaddr,
                                                 uint8_t width, uint8_t bitorder)
    : _i2cdevice(nullptr), _spidevice(spidevice), _addr(regaddr),
      _width(width), _bitorder(bitorder) {}

bool Adafruit_BusIO_Register::read(uint8_t *buffer) {
  if (_i2cdevice) {
    uint8_t addrbuf[2];
    addrbuf[0] = _addr & 0xFF;
    if (_width > 1) {
      addrbuf[1] = (_addr >> 8) & 0xFF;
    }
    return _i2cdevice->write_then_read(addrbuf, (_width > 1) ? 2 : 1, buffer,
                                      _width);
  } else if (_spidevice) {
    uint8_t addrbuf[2];
    if (_width > 1 && _bitorder == LSBFIRST) {
      addrbuf[0] = _addr & 0xFF;
      addrbuf[1] = (_addr >> 8) & 0xFF;
    } else {
      addrbuf[0] = (_addr >> 8) & 0xFF;
      addrbuf[1] = _addr & 0xFF;
    }
    return _spidevice->write_then_read(addrbuf, (_width > 1) ? 2 : 1, buffer,
                                       _width);
  }
  return false;
}

bool Adafruit_BusIO_Register::write(const uint8_t *buffer) {
  if (_i2cdevice) {
    // allocate enough room for up to a 2-byte address and 4 bytes of data
    uint8_t buf[6];
    buf[0] = _addr & 0xFF;
    if (_width > 1) {
      buf[1] = (_addr >> 8) & 0xFF;
    }
    memcpy(buf + ((_width > 1) ? 2 : 1), buffer, _width);
    return _i2cdevice->write(buf, _width + ((_width > 1) ? 2 : 1));
  } else if (_spidevice) {
    // allocate enough room for up to a 2-byte address and 4 bytes of data
    uint8_t buf[6];
    if (_width > 1 && _bitorder == LSBFIRST) {
      buf[0] = _addr & 0xFF;
      buf[1] = (_addr >> 8) & 0xFF;
    } else {
      buf[0] = (_addr >> 8) & 0xFF;
      buf[1] = _addr & 0xFF;
    }
    memcpy(buf + ((_width > 1) ? 2 : 1), buffer, _width);
    return _spidevice->write(buf, _width + ((_width > 1) ? 2 : 1));
  }
  return false;
}
