#pragma once

#ifdef UNIT_TEST
#ifndef ARDUINO
struct HardwareSerial {};
static HardwareSerial Serial1;

class SerialStub {
 public:
  template <typename... Args> void print(Args...) {}
  template <typename... Args> void println(Args...) {}
  template <typename... Args> void printf(Args...) {}
};

static SerialStub Serial;
#endif // !ARDUINO
#endif // UNIT_TEST
