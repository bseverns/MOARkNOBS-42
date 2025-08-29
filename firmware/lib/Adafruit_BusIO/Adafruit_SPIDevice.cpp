#include "Adafruit_SPIDevice.h"

Adafruit_SPIDevice::Adafruit_SPIDevice(int8_t cs, uint32_t freq,
                                       uint8_t dataOrder, uint8_t dataMode,
                                       SPIClass *theSPI)
    : _spiSetting(freq, dataOrder, dataMode), _spi(theSPI), _cs(cs),
      _sck(-1), _miso(-1), _mosi(-1), _hwspi(true), _begun(false),
      _freq(freq), _dataOrder(dataOrder), _dataMode(dataMode) {}

Adafruit_SPIDevice::Adafruit_SPIDevice(int8_t cs, int8_t sck, int8_t miso,
                                       int8_t mosi, uint32_t freq,
                                       uint8_t dataOrder, uint8_t dataMode)
    : _spiSetting(freq, dataOrder, dataMode), _spi(&SPI), _cs(cs),
      _sck(sck), _miso(miso), _mosi(mosi), _hwspi(false), _begun(false),
      _freq(freq), _dataOrder(dataOrder), _dataMode(dataMode) {}

bool Adafruit_SPIDevice::begin() {
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
  if (!_hwspi) {
    if (_sck >= 0)
      pinMode(_sck, OUTPUT);
    if (_mosi >= 0)
      pinMode(_mosi, OUTPUT);
    if (_miso >= 0)
      pinMode(_miso, INPUT);
  }
  _spi->begin();
  _begun = true;
  return true;
}

bool Adafruit_SPIDevice::read(uint8_t *buffer, size_t len, uint8_t sendvalue) {
  if (!_begun) begin();
  _spi->beginTransaction(_spiSetting);
  digitalWrite(_cs, LOW);
  for (size_t i = 0; i < len; i++) {
    buffer[i] = _spi->transfer(sendvalue);
  }
  digitalWrite(_cs, HIGH);
  _spi->endTransaction();
  return true;
}

bool Adafruit_SPIDevice::write(const uint8_t *buffer, size_t len,
                               const uint8_t *prefix, size_t prefix_len) {
  if (!_begun) begin();
  _spi->beginTransaction(_spiSetting);
  digitalWrite(_cs, LOW);
  if (prefix && prefix_len) {
    for (size_t i = 0; i < prefix_len; i++) {
      _spi->transfer(prefix[i]);
    }
  }
  for (size_t i = 0; i < len; i++) {
    _spi->transfer(buffer[i]);
  }
  digitalWrite(_cs, HIGH);
  _spi->endTransaction();
  return true;
}

bool Adafruit_SPIDevice::write_then_read(const uint8_t *write_buffer,
                                         size_t write_len, uint8_t *read_buffer,
                                         size_t read_len, uint8_t sendvalue) {
  if (!_begun) begin();
  _spi->beginTransaction(_spiSetting);
  digitalWrite(_cs, LOW);
  for (size_t i = 0; i < write_len; i++) {
    _spi->transfer(write_buffer[i]);
  }
  for (size_t i = 0; i < read_len; i++) {
    read_buffer[i] = _spi->transfer(sendvalue);
  }
  digitalWrite(_cs, HIGH);
  _spi->endTransaction();
  return true;
}

void Adafruit_SPIDevice::setSpeed(uint32_t freq) {
  _freq = freq;
  _spiSetting = SPISettings(freq, _dataOrder, _dataMode);
}

uint8_t Adafruit_SPIDevice::transfer(uint8_t send) {
  if (!_begun) begin();
  _spi->beginTransaction(_spiSetting);
  digitalWrite(_cs, LOW);
  uint8_t recv = _spi->transfer(send);
  digitalWrite(_cs, HIGH);
  _spi->endTransaction();
  return recv;
}
