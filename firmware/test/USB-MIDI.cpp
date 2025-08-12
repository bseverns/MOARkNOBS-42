#include "USB-MIDI.h"
#if defined(UNIT_TEST)
#include "MIDIHandler.h"
// Guard keeps the stub from hijacking real hardware builds.
#undef usbMIDI
MidiInterfaceStub MIDI;
USBMidiStub usbMIDI;
#endif  // defined(UNIT_TEST)

