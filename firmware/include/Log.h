#ifndef LOG_H
#define LOG_H

#if defined(UNIT_TEST)
#include <Arduino.h>

void clearTestLogBuffer();
const String &peekTestLogBuffer();
void appendTestLogBuffer(const char *text);
void appendTestLogBuffer(const String &text);
void testLogPrintf(const char *format, ...);

inline void testLogPrint(const char *text) { appendTestLogBuffer(text ? text : ""); }
inline void testLogPrint(const String &text) { appendTestLogBuffer(text); }
inline void testLogPrint(char value) {
    char buffer[2] = {value, '\0'};
    appendTestLogBuffer(buffer);
}
inline void testLogPrint(float value, int digits) { appendTestLogBuffer(String(value, digits)); }
inline void testLogPrint(double value, int digits) { appendTestLogBuffer(String(value, digits)); }
template <typename T> inline void testLogPrint(const T &value) {
    appendTestLogBuffer(String(value));
}

#define LOG_PRINT(...) testLogPrint(__VA_ARGS__)
#define LOG_PRINTLN(...)                                                                           \
    do {                                                                                           \
        testLogPrint(__VA_ARGS__);                                                                 \
        appendTestLogBuffer("\n");                                                                 \
    } while (0)
#define LOG_PRINTF(...) testLogPrintf(__VA_ARGS__)

// Thin logging wrappers. Define USB_MIDI_SERIAL to pipe messages over the
// Teensy's USB serial interface. Without it, the macros collapse to valid no-ops
// so tests don't whine about missing `Serial` and compilers don't grumble about
// empty bodies.
#elif defined(USB_MIDI_SERIAL)
#define LOG_PRINT(...) Serial.print(__VA_ARGS__)
#define LOG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define LOG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define LOG_PRINT(...)                                                                             \
    do {                                                                                           \
    } while (0)
#define LOG_PRINTLN(...)                                                                           \
    do {                                                                                           \
    } while (0)
#define LOG_PRINTF(...)                                                                            \
    do {                                                                                           \
    } while (0)
#endif

#endif // LOG_H
