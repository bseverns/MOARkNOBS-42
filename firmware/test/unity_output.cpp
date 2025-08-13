#ifdef ARDUINO
#include <Arduino.h>
extern "C" {
  void unityOutputStart(unsigned long baudrate) { Serial1.begin(baudrate); }
  void unityOutputChar(unsigned int c)         { Serial1.write((uint8_t)c); }
  void unityOutputFlush(void)                  { Serial1.flush(); }
  void unityOutputComplete(void)               { /* no-op */ }
}
#else
#include <cstdio>
extern "C" {
  void unityOutputStart(unsigned long) { /* no-op */ }
  void unityOutputChar(unsigned int c) { putchar(static_cast<int>(c)); }
  void unityOutputFlush(void) { fflush(stdout); }
  void unityOutputComplete(void) { fflush(stdout); }
}
#endif
