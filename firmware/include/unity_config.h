#pragma once

#ifdef ARDUINO
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

void unityOutputStart();
void unityOutputChar(char c);
void unityOutputFlush();
void unityOutputComplete();

#ifdef __cplusplus
}
#endif

#define UNITY_OUTPUT_START()      unityOutputStart()
#define UNITY_OUTPUT_CHAR(c)      unityOutputChar(c)
#define UNITY_OUTPUT_FLUSH()      unityOutputFlush()
#define UNITY_OUTPUT_COMPLETE()   unityOutputComplete()

#ifndef ARDUINO
static inline void unityOutputStart(void) {}
static inline void unityOutputChar(char c) { putchar(c); }
static inline void unityOutputFlush(void) { fflush(stdout); }
static inline void unityOutputComplete(void) {}
#endif

