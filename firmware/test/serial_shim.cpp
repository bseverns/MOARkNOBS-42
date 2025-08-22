#include <Arduino.h>

#if !defined(USB_SERIAL) && !defined(USB_MIDI_SERIAL)

HardwareSerial &Serial = Serial1; // harmless alias so headers with &Serial compile

#endif