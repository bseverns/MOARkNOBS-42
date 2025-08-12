#include "USB-MIDI.h"
#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
#include "MIDIHandler.h"
// Guard keeps the stub from hijacking real hardware builds.
#undef usbMIDI
MidiInterfaceStub MIDI;
USBMidiStub usbMIDI;
#ifndef ARDUINO
HardwareSerial Serial1;

// Minimal stand‑in for the Teensy's USB Serial gadget.  Tests compile with
// `USB_MIDI_SERIAL` yanked, so anything that still yaps over `Serial` would
// normally explode at link time.  This stub swallows prints so Unity builds
// can cruise.
class SerialStub {
 public:
  template <typename T> void print(const T&) {}
  template <typename T> void println(const T&) {}
  void printf(const char*, ...) {}
};
SerialStub Serial;
#endif  // !ARDUINO
#endif  // defined(UNIT_TEST) && defined(USB_MIDI_STUB)

