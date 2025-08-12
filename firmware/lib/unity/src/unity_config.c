#include <Arduino.h>
#include <unity_config.h>

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

