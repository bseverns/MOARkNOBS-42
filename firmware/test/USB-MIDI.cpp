#include "USB-MIDI.h"
#if defined(UNIT_TEST) && !defined(ARDUINO)
// Guard keeps the stub from hijacking real hardware builds.
MidiInterfaceStub MIDI;
USBMidiStub usbMIDI;
HardwareSerial Serial1;
#endif

