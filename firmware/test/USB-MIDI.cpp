#include "USB-MIDI.h"
#if defined(UNIT_TEST)
// Guard keeps the stub from hijacking real hardware builds.
MidiInterfaceStub MIDI;
USBMidiStub usbMIDI;
HardwareSerial Serial1;
#endif  // defined(UNIT_TEST)

