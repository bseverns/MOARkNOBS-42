#include "USB-MIDI.h"
#ifdef USB_MIDI_STUB
// Guard keeps the stub from hijacking real hardware builds.
MidiInterfaceStub MIDI;
USBMidiStub usbMIDI;
HardwareSerial Serial1;
#endif  // defined(UNIT_TEST)

