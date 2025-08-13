#pragma once

#if defined(UNIT_TEST) && !defined(ARDUINO)

#include <cstddef>
#include <cstdint>

struct Print {
  template <typename... Args> void print(Args...) {}
  template <typename... Args> void println(Args...) {}
  template <typename... Args> void printf(Args...) {}
  size_t write(uint8_t) { return 1; }
  size_t write(const uint8_t*, size_t) { return 0; }
  size_t write(const char*) { return 0; }
};

struct Stream : Print {};

struct HardwareSerial : Stream {
  void begin(unsigned long) {}
  void flush() {}
};

class SerialStub : public HardwareSerial {};

extern SerialStub Serial;
extern SerialStub Serial1;

#else

#include <Arduino.h>

class SerialStub : public HardwareSerial {
 public:
  using HardwareSerial::HardwareSerial;
  void begin(unsigned long) {}
  size_t write(uint8_t) { return 1; }
  void flush() {}
};

#endif // defined(UNIT_TEST) && !defined(ARDUINO)
