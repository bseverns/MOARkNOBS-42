#include <Arduino.h>

extern "C" {

void unityOutputStart() {
  Serial.begin(115200);
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

