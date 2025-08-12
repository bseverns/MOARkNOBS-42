#ifndef LOG_H
#define LOG_H

// Thin logging wrappers. Define USB_MIDI_SERIAL to pipe messages over the
// Teensy's USB serial interface. Without it, the macros collapse to no-ops so
// tests don't whine about missing `Serial`.
#ifdef USB_MIDI_SERIAL
  #define LOG_PRINT(...)   Serial.print(__VA_ARGS__)
  #define LOG_PRINTLN(...) Serial.println(__VA_ARGS__)
  #define LOG_PRINTF(...)  Serial.printf(__VA_ARGS__)
#else
  #define LOG_PRINT(...)
  #define LOG_PRINTLN(...)
  #define LOG_PRINTF(...)
#endif

#endif // LOG_H
