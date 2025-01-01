#ifndef UTILITY_H
#define UTILITY_H

#include <Arduino.h>
#include <vector> // Include for std::vector
#include <EEPROM.h>

// Forward declaration
class EnvelopeFollower;
class LEDManager;
class DisplayManager;

void rebootTeensy();

class Utility {
public:
    static uint8_t mapToMidiValue(int analogValue, int minValue = 0, int maxValue = 1023);
    static bool debounceButton(uint8_t pin, unsigned long debounceDelay = 50);
    static bool debounce(
        bool &previousState,
        bool currentState,
        unsigned long &lastDebounceTime,
        unsigned long currentTime,
        unsigned long debounceDelay
    );
    static void updateVisuals(
        uint8_t midiBeatPosition,
        const std::vector<EnvelopeFollower>& envelopeFollowers,
        const char* statusMessage,
        uint8_t activePot,
        uint8_t activeChannel,
        LEDManager& ledManager,
        DisplayManager& displayManager,
        const char* envelopeMode
    );
    static void processBulkUpdate(const String& command, uint8_t numPots);
};

#endif // UTILITY_H
