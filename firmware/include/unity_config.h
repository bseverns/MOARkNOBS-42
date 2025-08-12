#pragma once

#if defined(ARDUINO) || defined(TEENSYDUINO) || defined(UNIT_TEST)
#include <Arduino.h>
#else
#include <cstdio>
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

#ifndef UNITY_OUTPUT_START
#define UNITY_OUTPUT_START(b)      unityTestStart(b)
#endif
#ifndef UNITY_OUTPUT_CHAR
#define UNITY_OUTPUT_CHAR(c)      unityTestChar(c)
#endif
#ifndef UNITY_OUTPUT_FLUSH
#define UNITY_OUTPUT_FLUSH()      unityTestFlush()
#endif
#ifndef UNITY_OUTPUT_COMPLETE
#define UNITY_OUTPUT_COMPLETE()   unityTestComplete()
#endif

