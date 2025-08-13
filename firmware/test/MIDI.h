#pragma once

#ifdef UNIT_TEST
#include "usb_midi.h"

#ifndef MIDI_CREATE_INSTANCE
#define MIDI_CREATE_INSTANCE(type, serial, name)
#endif

#else
#include_next <MIDI.h>
#endif
