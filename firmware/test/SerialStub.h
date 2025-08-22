#pragma once

#if defined(UNIT_TEST) && !defined(ARDUINO)

#include <cstddef>

struct Print {
    template <typename... Args> void print(Args...) {}
    template <typename... Args> void println(Args...) {}
    template <typename... Args> void printf(Args...) {}
    size_t write(uint8_t) { return 1; }
    size_t write(const uint8_t *, size_t) { return 0; }
    size_t write(const char *) { return 0; }
};

struct Stream : Print {};

class HardwareSerial : public Stream {
  public:
    void begin(unsigned long) {}
    size_t write(uint8_t) { return 1; }
    void flush() {}
};

class SerialStub : public HardwareSerial {};

extern SerialStub Serial;
extern SerialStub Serial1;

#elif defined(UNIT_TEST)

#include <Arduino.h>

// On real Teensy builds, lean on the core's HardwareSerial.
using SerialStub = decltype(::Serial1);

#endif // UNIT_TEST
