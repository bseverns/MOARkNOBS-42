#pragma once
#include <Arduino.h>
#include <SPI.h>

#ifndef SPI_BITORDER_MSBFIRST
#define SPI_BITORDER_MSBFIRST MSBFIRST
#endif
#ifndef SPI_BITORDER_LSBFIRST
#define SPI_BITORDER_LSBFIRST LSBFIRST
#endif

class Adafruit_SPIDevice {
public:
  Adafruit_SPIDevice(int8_t cs, uint32_t freq = 1000000,
                     uint8_t dataOrder = MSBFIRST,
                     uint8_t dataMode = SPI_MODE0, SPIClass *theSPI = &SPI);
  Adafruit_SPIDevice(int8_t cs, int8_t sck, int8_t miso, int8_t mosi,
                     uint32_t freq = 1000000,
                     uint8_t dataOrder = MSBFIRST,
                     uint8_t dataMode = SPI_MODE0);
  bool begin();
  bool read(uint8_t *buffer, size_t len, uint8_t sendvalue = 0);
  bool write(const uint8_t *buffer, size_t len,
             const uint8_t *prefix = nullptr, size_t prefix_len = 0);
  bool write_then_read(const uint8_t *write_buffer, size_t write_len,
                       uint8_t *read_buffer, size_t read_len,
                       uint8_t sendvalue = 0);
  void setSpeed(uint32_t freq);
  uint32_t getSpeed() const { return _freq; }
  uint8_t transfer(uint8_t send);

private:
  SPISettings _spiSetting;
  SPIClass *_spi;
  int8_t _cs;
  int8_t _sck;
  int8_t _miso;
  int8_t _mosi;
  bool _hwspi;
  bool _begun;
  uint32_t _freq;
  uint8_t _dataOrder;
  uint8_t _dataMode;
};
