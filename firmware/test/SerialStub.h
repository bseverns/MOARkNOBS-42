#pragma once

#if defined(UNIT_TEST) && (defined(USB_MIDI_STUB) || !defined(ARDUINO))

struct Print {
  template <typename... Args> void print(Args...) {}
  template <typename... Args> void println(Args...) {}
  template <typename... Args> void printf(Args...) {}
};

struct Stream : Print {};

struct HardwareSerial : Stream {};

class SerialStub : public HardwareSerial {};

static SerialStub Serial;
static SerialStub Serial1;

#endif // defined(UNIT_TEST) && (defined(USB_MIDI_STUB) || !defined(ARDUINO))
