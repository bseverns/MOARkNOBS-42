// Only spin up the fake usbMIDI guts when both UNIT_TEST and
// USB_MIDI_STUB flags are waving. That keeps the Teensy core's real
// implementation from clashing with our test double.
#include "usb_midi.h"
#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
#include "MIDIHandler.h"
// Guard keeps the stub from hijacking real hardware builds.
#undef usbMIDI
MidiInterfaceStub MIDI;  // fake MIDI interface for tests
USBMidiStub usbMIDI;     // test double standing in for Teensy's usbMIDI
#endif  // defined(UNIT_TEST) && defined(USB_MIDI_STUB)

