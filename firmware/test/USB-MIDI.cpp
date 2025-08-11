#include "USB-MIDI.h"
#if !defined(ARDUINO)
MidiInterfaceStub MIDI;
USBMidiStub usbMIDI;
HardwareSerial Serial1;
#endif

