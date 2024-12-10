#include "Utility.h"

uint8_t Utility::mapToMidiValue(int analogValue, int minValue, int maxValue) {
    return map(analogValue, minValue, maxValue, 0, 127);
}

bool Utility::debounceButton(uint8_t pin, unsigned long debounceDelay) {
    static unsigned long lastDebounceTime = 0;
    static bool lastButtonState = LOW;

    bool currentState = digitalRead(pin);
    if (currentState != lastButtonState) {
        lastDebounceTime = millis();
    }
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (currentState != lastButtonState) {
            lastButtonState = currentState;
            return currentState;
        }
    }
    return false;
}
