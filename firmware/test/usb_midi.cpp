// Only spin up the fake usbMIDI guts when both UNIT_TEST and
// USB_MIDI_STUB flags are waving. That keeps the Teensy core's real
// implementation from clashing with our test double.
#include "usb_midi.h"
#if defined(UNIT_TEST) && defined(USB_MIDI_STUB)
#include "MIDIHandler.h"
// Guard keeps the stub from hijacking real hardware builds.
MidiInterfaceStub MIDI; // fake MIDI interface for tests
usb_midi_class usbMIDI; // stand-in for Teensy's usbMIDI
#endif                  // defined(UNIT_TEST) && defined(USB_MIDI_STUB)
