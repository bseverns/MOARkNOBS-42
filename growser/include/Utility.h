#ifndef UTILITY_H
#define UTILITY_H

#include <Arduino.h>

void rebootTeensy();

class Utility {
public:
    static uint8_t mapToMidiValue(int analogValue, int minValue = 0, int maxValue = 1023);
    static bool debounceButton(uint8_t pin, unsigned long debounceDelay = 50);
};

#endif
