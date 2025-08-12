#include <Arduino.h>

extern "C" {

void unityOutputStart(unsigned long baudrate) {
  Serial.begin(baudrate);
}

void unityOutputChar(char c) {
  Serial.write(c);
}

void unityOutputFlush() {
  Serial.flush();
}

void unityOutputComplete() {
  Serial.end();
}

}

