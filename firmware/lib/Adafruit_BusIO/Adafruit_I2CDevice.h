#pragma once
#include <Arduino.h>
#include <Wire.h>

class Adafruit_I2CDevice {
public:
  Adafruit_I2CDevice(uint8_t addr, TwoWire *theWire = &Wire);
  bool begin(bool addr_detect = true);
  bool detected();
  bool read(uint8_t *buffer, size_t len);
  bool write(const uint8_t *buffer, size_t len, bool stop = true,
             const uint8_t *prefix = nullptr, size_t prefix_len = 0);
  bool write_then_read(const uint8_t *write_buffer, size_t write_len,
                       uint8_t *read_buffer, size_t read_len, bool stop = true);
  bool setSpeed(uint32_t speed);
  uint8_t address() const { return _addr; }

private:
  TwoWire *_wire;
  uint8_t _addr;
  bool _begun;
};
