#ifndef UTILITY_H
#define UTILITY_H

#include <Arduino.h>
#include <vector>
#include <queue>
#include <FastLED.h>
#include <DisplayManager.h>
#include <EnvelopeFollower.h>
#include "EEPROM.h"

class Utility {
public:
    // Mapping and Value Transformations
    static uint8_t mapToMidiValue(int analogValue, int minValue = 0, int maxValue = 1023);
    static int mapToRange(int value, int inMin, int inMax, int outMin, int outMax);
    static float mapExponential(float value, float inMin, float inMax, float outMin, float outMax, float exponent);

    // Debouncing
    static bool debounce(bool& previousState, bool currentState, unsigned long& lastDebounceTime, unsigned long currentTime, unsigned long debounceDelay);

    // EEPROM Operations
    static uint8_t readEEPROMByte(int address);
    static void writeEEPROMByte(int address, uint8_t value);

    // Timer Helpers
    static bool isTimeElapsed(unsigned long& lastTime, unsigned long interval);

    // LED Utilities
    static CRGB mapValueToColor(uint8_t value, CRGB lowColor, CRGB highColor);

    // Serial Communication Helpers
    static void logError(const char* errorMessage);
    static void logDebug(const char* debugMessage);

    // Filtering
    static int exponentialMovingAverage(int currentValue, int previousValue, float alpha);

    // System Operations
    static void rebootTeensy();

    // Display Utilities
    static void displayCenteredText(Adafruit_SSD1306& display, const char* text);
    static void displayStatus(Adafruit_SSD1306& display, const char* status, unsigned long duration);
   static void updateDisplay(
    Adafruit_SSD1306& display,
    uint8_t beatPosition,
    const std::vector<EnvelopeFollower>& envelopeFollowers, // Accept EnvelopeFollower objects
    const char* statusMessage,
    uint8_t activePot,
    uint8_t activeChannel,
    const char* envelopeMode
);

    static uint16_t readEEPROMWord(int address);
    static void writeEEPROMWord(int address, uint16_t value);
    static void resetEEPROM(int startAddress, int endAddress, uint8_t defaultValue = 0xFF);
    static void processBulkUpdate(const String& command, uint8_t numPots);
};

#endif // UTILITY_H