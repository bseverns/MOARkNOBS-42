#ifndef UTILITY_H
#define UTILITY_H

#include <Arduino.h>
#include "LEDManager.h"
#include "DisplayManager.h"
#include "EnvelopeFollower.h"

void rebootTeensy();

class Utility {
public:
    static uint8_t mapToMidiValue(int analogValue, int minValue = 0, int maxValue = 1023);
    static bool debounceButton(uint8_t pin, unsigned long debounceDelay = 50);
    // Debounce function
    static bool debounce(
        bool &previousState,       // Reference to the previous stable state
        bool currentState,         // Current raw state
        unsigned long &lastDebounceTime, // Reference to the last debounce time
        unsigned long currentTime,       // Current system time (millis)
        unsigned long debounceDelay      // Minimum time required for a stable state
    );
    static void updateVisuals(
    uint8_t midiBeatPosition,
    const std::vector<EnvelopeFollower>& envelopeFollowers,
    const char* statusMessage,
    uint8_t activePot,
    uint8_t activeChannel,
    LEDManager& ledManager,
    DisplayManager& displayManager
    ); 
};

#endif
