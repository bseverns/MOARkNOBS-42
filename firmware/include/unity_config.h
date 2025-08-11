#pragma once

#include <Arduino.h>

// Unity wants to know where to spit its test logs.
// Route every character over Serial and keep the rest quiet.
#define UNITY_OUTPUT_START()      Serial.begin(115200)
#define UNITY_OUTPUT_CHAR(c)      Serial.write(c)
#define UNITY_OUTPUT_FLUSH()      Serial.flush()
#define UNITY_OUTPUT_COMPLETE()   Serial.end()

