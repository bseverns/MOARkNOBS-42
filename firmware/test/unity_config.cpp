#include <unity_config.h>

// Unity's auto-generated config pipes output straight to Serial.
// That's cute until the host doesn't rock a USB gadget.
// We still mirror those hooks here for any rogue host builds, but the
// heavy lifting now lives in unity_output.cpp.

#ifdef ARDUINO
#include <Arduino.h>
extern "C" {
// unityTestStart still lets us pick the tempo; UNITY_OUTPUT_START()
// slaps it with 115200 unless you remix the macro.
void unityTestStart(unsigned long baudrate) { Serial1.begin(baudrate); }
void unityTestChar(unsigned int c) { Serial1.write(static_cast<uint8_t>(c)); }
void unityTestFlush(void) { Serial1.flush(); }
void unityTestComplete(void) {
    // no-op
}
}
#else
#include <cstdio>
extern "C" {
void unityTestStart(unsigned long) {
    // no-op
}
void unityTestChar(unsigned int c) { putchar(static_cast<int>(c)); }
void unityTestFlush(void) { fflush(stdout); }
void unityTestComplete(void) { fflush(stdout); }
}
#endif
