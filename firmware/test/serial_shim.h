#pragma once

#include <Arduino.h>

#if !defined(USB_SERIAL) && !defined(USB_MIDI_SERIAL)

extern HardwareSerial &Serial; // defined in serial_shim.cpp

#endif
