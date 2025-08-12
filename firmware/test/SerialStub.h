#pragma once

#ifdef UNIT_TEST
#if !defined(ARDUINO) || defined(USB_MIDI_STUB)
struct HardwareSerial {};
static HardwareSerial Serial1;

class SerialStub {
 public:
  template <typename... Args> void print(Args...) {}
  template <typename... Args> void println(Args...) {}
  template <typename... Args> void printf(Args...) {}
};

static SerialStub Serial;
#endif // !defined(ARDUINO) || defined(USB_MIDI_STUB)
#endif // UNIT_TEST
