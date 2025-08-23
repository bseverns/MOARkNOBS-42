#pragma once

#include <stdio.h>

#include <Arduino.h>

#ifdef UNIT_TEST
#ifdef __cplusplus
#include "usb_midi.h"
#endif
#endif

// Unity wants to know where to spit its test logs.
// On Teensy we shout over Serial; on the host we spew to stdout.
// This split personality keeps both toolchains grinning.

#ifdef __cplusplus
extern "C" {
#endif

// Unity punts its log through these wrappers.  The real work lives in
// unity_output.cpp where we punt bytes over Serial1 so the board never
// touches the USB CDC port.
void unityOutputStart(unsigned long baudrate);
void unityOutputChar(unsigned int c);
void unityOutputFlush();
void unityOutputComplete();

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
#define UNITY_OUTPUT_START() unityOutputStart(115200)
#define UNITY_OUTPUT_CHAR(c) unityOutputChar(c)
#define UNITY_OUTPUT_FLUSH() unityOutputFlush()
#define UNITY_OUTPUT_COMPLETE() unityOutputComplete()
