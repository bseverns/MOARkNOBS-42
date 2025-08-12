#include <Arduino.h>

extern "C" {

void unityOutputStart(unsigned long baudrate) {
  Serial.begin(baudrate);
}

void unityOutputChar(unsigned int c) {
  Serial.write(static_cast<uint8_t>(c)); // Unity slings ints; Serial chews bytes
}

void unityOutputFlush() {
  Serial.flush();
}

void unityOutputComplete() {
  Serial.end();
}

}

