#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <cstdio>
#endif

// Unity wants to know where to spit its test logs.
// On Teensy we shout over Serial; on the host we spew to stdout.
// This split personality keeps both toolchains grinning.
#ifdef ARDUINO
#define UNITY_OUTPUT_START()      Serial.begin(115200)
#define UNITY_OUTPUT_CHAR(c)      Serial.write(c)
#define UNITY_OUTPUT_FLUSH()      Serial.flush()
#define UNITY_OUTPUT_COMPLETE()   Serial.end()
#else
#define UNITY_OUTPUT_START()
#define UNITY_OUTPUT_CHAR(c)      putchar(c)
#define UNITY_OUTPUT_FLUSH()      fflush(stdout)
#define UNITY_OUTPUT_COMPLETE()
#endif

