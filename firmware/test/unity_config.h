#pragma once

#include <stdio.h>

#ifdef UNIT_TEST
#ifdef __cplusplus
// Rename the core's usbMIDI symbols before Arduino drags them in.
#define usb_midi_class teensy_core_usb_midi_class
#define usbMIDI teensy_core_usbMIDI
#endif
#endif

#include <Arduino.h>

#ifdef UNIT_TEST
#ifdef __cplusplus
// Undo the rename and slip in our stub so tests talk to the fake.
#undef usb_midi_class
#undef usbMIDI
#include "usb_midi.h"
#endif
#endif

// Unity wants to know where to spit its test logs.
// On Teensy we shout over Serial; on the host we spew to stdout.
// This split personality keeps both toolchains grinning.

#ifdef __cplusplus
extern "C" {
#endif

// Wrap the output hooks in uniquely named functions so the Unity
// macros never see `Serial` directly.  That keeps the tests from
// tripping over missing USB gadgets on the host.
//
// `unityTestStart()` still takes a baud so we can pick a lane if the
// default doesn't groove with the hardware.
void unityTestStart(unsigned long baudrate);
void unityTestChar(unsigned int c);
void unityTestFlush();
void unityTestComplete();

#ifdef __cplusplus
}
#endif
// Chuck Unity's baked-in hooks so our wrappers take the wheel.
#undef UNITY_OUTPUT_START
#undef UNITY_OUTPUT_CHAR
#undef UNITY_OUTPUT_FLUSH
#undef UNITY_OUTPUT_COMPLETE

// Kick off the log line at 115200 by default.  Tweak the magic number
// if your setup jams at a different tempo.
#define UNITY_OUTPUT_START()       unityTestStart(115200)
#define UNITY_OUTPUT_CHAR(c)       unityTestChar(c)
#define UNITY_OUTPUT_FLUSH()       unityTestFlush()
#define UNITY_OUTPUT_COMPLETE()    unityTestComplete()

