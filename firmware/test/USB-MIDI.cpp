#include "USB-MIDI.h"
#if !defined(ARDUINO) || defined(UNIT_TEST)
MidiInterfaceStub MIDI;
USBMidiStub usbMIDI;
HardwareSerial Serial1;
#endif

