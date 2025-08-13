#include <Arduino.h>
extern "C" {
  void unityOutputStart(unsigned long baudrate) { Serial1.begin(baudrate); }
  void unityOutputChar(unsigned int c)         { Serial1.write((uint8_t)c); }
  void unityOutputFlush(void)                  { Serial1.flush(); }
  void unityOutputComplete(void)               { /* no-op */ }
}