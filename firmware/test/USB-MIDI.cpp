#include "USB-MIDI.h"
#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
// Guard keeps the stub from hijacking real hardware builds.
MidiInterfaceStub MIDI;
USBMidiStub usbMIDI;
#ifndef ARDUINO
HardwareSerial Serial1;
#endif
#endif  // defined(UNIT_TEST) && defined(USB_MIDI_STUB)

