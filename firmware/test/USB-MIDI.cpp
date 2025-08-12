#include "USB-MIDI.h"
#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
#include "MIDIHandler.h"
// Guard keeps the stub from hijacking real hardware builds.
#undef usbMIDI
MidiInterfaceStub MIDI;
USBMidiStub usbMIDI;
#endif

#if defined(UNIT_TEST) && defined(USB_MIDI_STUB) && !defined(ARDUINO)
HardwareSerial Serial1;
SerialStub Serial;
#endif

