// Common utility routines and lightweight task scheduler.
// Includes mapping helpers, EEPROM wrappers and global schedulers.
// Used by nearly every other module.
#ifndef UTILITY_H
#define UTILITY_H

#include <Arduino.h>
#include <functional>
#include <vector>
#include <queue>
#include <FastLED.h>
#include <DisplayManager.h>
#include <EnvelopeFollower.h>
#include <Globals.h>
#include "EEPROM.h"
#include "TimeUtils.h"

class MIDIHandler;
class EnvelopeFollower;

/** Small structure representing a scheduled callback. */
struct ScheduledTask {
    std::function<void()> callback;
    unsigned long runAt;
    bool repeat;
    unsigned long interval;

    ScheduledTask(std::function<void()> cb, unsigned long delayMs, bool rpt)
        : callback(cb), runAt(now() + delayMs), repeat(rpt), interval(delayMs) {}
};

/**
 * Simple cooperative task scheduler used by Utility.
 *
 * Callbacks get collected and fired later, so they must not try to
 * monkey with the scheduler's task list directly.
 */
class TaskScheduler {
public:
    void addTask(std::function<void()> callback, unsigned long delayMs, bool repeat = false);

    /**
     * Tick the scheduler: collect due tasks, fire callbacks, and then
     * wipe finished one-shots.
     */
    void update();
   
private:
    std::vector<ScheduledTask> tasks;
};

/** Collection of miscellaneous helper functions. */
class Utility {
public:
    /**
     * Schedule a Note On immediately followed by a delayed Note Off.
     *
     * @param midiHandler  Reference to the MIDI handler used to send messages.
     * @param note         MIDI note number.
     * @param velocity     Note on velocity.
     * @param channel      MIDI channel (1‑16).
     * @param durationMs   Duration in milliseconds before the Note Off is sent.
     */
    static void scheduleNoteOnOff(
        MIDIHandler& midiHandler,
        uint8_t note,
        uint8_t velocity,
        uint8_t channel,
        unsigned long durationMs
    );

    // Mapping and Value Transformations
    /** Map a raw analog reading to the 0‑127 MIDI range. */
    static uint8_t mapToMidiValue(int analogValue, int minValue = 0, int maxValue = 1023);

    /** Generic integer mapping helper. */
    static int mapToRange(int value, int inMin, int inMax, int outMin, int outMax);

    /** Map a value from one float range to another. */
    static float scale(float value, float inMin, float inMax, float outMin, float outMax);

    /** Exponential scaling used for envelope shaping. */
    static float mapExponential(float value, float inMin, float inMax, float outMin, float outMax, float exponent);

    // Debouncing
    /** Simple digital input debouncing helper. */
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

    static void processBulkUpdate(const String& command, uint8_t numPots);

    /** Sample the hardware VREF divider and return the measured voltage. */
    static float readVrefADC(uint8_t pin = VREF_ADC_PIN);

    /** High, medium and low priority schedulers used globally. */
    static TaskScheduler schedulerHigh;
    static TaskScheduler schedulerMid;
    static TaskScheduler schedulerLow;

    static uint16_t readEEPROMWord(int address);
    static void writeEEPROMWord(int address, uint16_t value);
    static void resetEEPROM(int startAddress, int endAddress, uint8_t defaultValue = 0xFF);
};

/**
 * @brief Drive a 4-bit multiplexer select bus.
 *
 * Convenience helper used by both the button and potentiometer scanners
 * to update the CD74HC4067 address lines.
 */
inline void setMux(const uint8_t selPins[4], uint8_t index) {
    for (uint8_t i = 0; i < 4; ++i) {
        digitalWrite(selPins[i], (index >> i) & 1);
    }
}

#endif // UTILITY_H
