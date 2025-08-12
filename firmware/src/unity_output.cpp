#include <cstdio>
#ifdef USB_MIDI_SERIAL
#include <Arduino.h>
#endif
#include "unity_config.h"

// The Unity test runner wants a handful of hooks to squirt out text.
// We punt those to uniquely named functions so nothing drags in
// `Serial` unless we absolutely mean it.
extern "C" {

void unityTestStart(unsigned long baudrate) {
#ifdef USB_MIDI_SERIAL
  Serial.begin(baudrate);
#else
  (void)baudrate; // No-op when the USB serial gadget isn't around
#endif
}

void unityTestChar(unsigned int c) {
#ifdef USB_MIDI_SERIAL
  Serial.write(static_cast<uint8_t>(c)); // Unity slings ints; Serial chews bytes
#else
  putchar(static_cast<char>(c));
#endif
}

void unityTestFlush() {
#ifdef USB_MIDI_SERIAL
  Serial.flush();
#else
  fflush(stdout); // make sure host logs don't get stuck in a buffer
#endif
}

void unityTestComplete() {
#ifdef USB_MIDI_SERIAL
  Serial.end();
#else
  fflush(stdout); // last gasp when the USB serial gadget bails
#endif
}

}

