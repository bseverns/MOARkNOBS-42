#include <unity_config.h>

// Unity's auto-generated config pipes output straight to Serial.
// That's cute until the host doesn't rock a USB gadget.
// Mirror those hooks to our punk wrappers so nothing touches
// Serial unless we mean it.

extern "C" {

void unityOutputStart(unsigned long baudrate) {
  unityTestStart(baudrate);
}

void unityOutputChar(unsigned int c) {
  unityTestChar(c);
}

void unityOutputFlush(void) {
  unityTestFlush();
}

void unityOutputComplete(void) {
  unityTestComplete();
}

}
