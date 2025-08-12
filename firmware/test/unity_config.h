#pragma once

#include <cstdio>
#ifdef UNIT_TEST
#include "../test/USB-MIDI.h"  // rope in the Serial stand-in when the core's MIA
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

#define UNITY_OUTPUT_START(b)      unityTestStart(b)
#define UNITY_OUTPUT_CHAR(c)       unityTestChar(c)
#define UNITY_OUTPUT_FLUSH()       unityTestFlush()
#define UNITY_OUTPUT_COMPLETE()    unityTestComplete()

