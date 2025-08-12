#include <Arduino.h>
#include <cstdio>
#include "unity_config.h"

extern "C" {

void unityOutputStart(unsigned long baudrate) {
#ifdef USB_MIDI_SERIAL
  Serial.begin(baudrate);
#else
  (void)baudrate; // No-op when the USB serial gadget isn't around
#endif
}

void unityOutputChar(unsigned int c) {
#ifdef USB_MIDI_SERIAL
  Serial.write(static_cast<uint8_t>(c)); // Unity slings ints; Serial chews bytes
#else
  putchar(static_cast<char>(c));
#endif
}

void unityOutputFlush() {
#ifdef USB_MIDI_SERIAL
  Serial.flush();
#else
  fflush(stdout); // make sure host logs don't get stuck in a buffer
#endif
}

void unityOutputComplete() {
#ifdef USB_MIDI_SERIAL
  Serial.end();
#else
  fflush(stdout); // last gasp when the USB serial gadget bails
#endif
}

}

