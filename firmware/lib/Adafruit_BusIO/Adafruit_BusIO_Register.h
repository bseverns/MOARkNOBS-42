#pragma once
#include "Adafruit_I2CDevice.h"
#include "Adafruit_SPIDevice.h"

class Adafruit_BusIO_Register {
public:
  Adafruit_BusIO_Register(Adafruit_I2CDevice *i2cdevice, uint16_t regaddr,
                          uint8_t width = 1, uint8_t bitorder = MSBFIRST);
  Adafruit_BusIO_Register(Adafruit_SPIDevice *spidevice, uint16_t regaddr,
                          uint8_t width = 1, uint8_t bitorder = MSBFIRST);
  bool read(uint8_t *buffer);
  bool write(const uint8_t *buffer);

private:
  Adafruit_I2CDevice *_i2cdevice;
  Adafruit_SPIDevice *_spidevice;
  uint16_t _addr;
  uint8_t _width;
  uint8_t _bitorder;
};
