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

void unityOutputStart(unsigned long baudrate);
void unityOutputChar(unsigned int c);
void unityOutputFlush();
void unityOutputComplete();

#ifdef __cplusplus
}
#endif

#ifndef UNITY_OUTPUT_START
#define UNITY_OUTPUT_START(b)      unityOutputStart(b)
#endif
#ifndef UNITY_OUTPUT_CHAR
#define UNITY_OUTPUT_CHAR(c)      unityOutputChar(c)
#endif
#ifndef UNITY_OUTPUT_FLUSH
#define UNITY_OUTPUT_FLUSH()      unityOutputFlush()
#endif
#ifndef UNITY_OUTPUT_COMPLETE
#define UNITY_OUTPUT_COMPLETE()   unityOutputComplete()
#endif

#if !defined(ARDUINO) && !defined(TEENSYDUINO) && !defined(UNIT_TEST)
static inline void unityOutputStart(unsigned long baudrate) { (void)baudrate; }
static inline void unityOutputChar(unsigned int c) { putchar(c); }
static inline void unityOutputFlush(void) { fflush(stdout); }
static inline void unityOutputComplete(void) {}
#endif

