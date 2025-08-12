#include "USB-MIDI.h"
#ifdef UNIT_TEST
// Guard keeps the stub from hijacking real hardware builds.
MidiInterfaceStub MIDI;
USBMidiStub usbMIDI;
#ifndef ARDUINO
HardwareSerial Serial1;
#endif
#endif  // UNIT_TEST

