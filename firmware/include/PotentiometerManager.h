// Oversees reading all analog pots via multiplexers.
// Calls a user-supplied callback for MIDI output and drives the LEDManager.
// Used continuously by firmware_main.cpp.
#ifndef POTENTIOMETER_MANAGER_H
#define POTENTIOMETER_MANAGER_H

#include <Arduino.h>
#include <functional>
#include <vector> // For std::vector
#include "LEDManager.h"
#include "Utility.h"
#include "Globals.h"

// Forward declaration to avoid circular dependency
class EnvelopeFollower;

#define PRIMARY_MUX_PINS 4
#define SECONDARY_MUX_PINS 4
constexpr uint8_t NUM_POTS = 42;


/**
 * @brief Reads all potentiometers via a pair of analog multiplexers and
 *        forwards the values as MIDI messages.
 */
class PotentiometerManager {
private:
    const uint8_t* primaryMuxPins;   // Pins for primary mux bank
    const uint8_t* secondaryMuxPins; // Pins for secondary mux bank
    const uint8_t analogPin;         // Analog pin for mux output
    uint8_t potChannels[NUM_POTS];   // MIDI channel for each pot
    uint8_t potCCNumbers[NUM_POTS];  // MIDI CC number for each pot
    int potLastValues[NUM_POTS];     // Last read values for each pot

    void selectMuxBank(uint8_t bank); // Select the primary mux bank
    void selectPotBank(uint8_t pot);  // Select the secondary mux pot

    // Callback for sending MIDI messages
  std::function<void(uint8_t, uint8_t, uint8_t, uint8_t)> midiCallback;

    // Helper for filtered analog reads
    int readAnalogFiltered(uint8_t pin); // New function for analog filtering

    int argEnvA;
    int argEnvB;

public:
    /**
     * @param primaryPins   Pointer to four GPIO pins selecting the primary mux.
     * @param secondaryPins Pointer to four GPIO pins selecting the pot within the mux.
     * @param analogPin     Analog input used to read the mux output.
     */
    PotentiometerManager(
        const uint8_t* primaryPins,
        const uint8_t* secondaryPins,
        uint8_t analogPin
    );

    /** Callback invoked when a pot value changes. */
    void setMidiCallback(std::function<void(
    uint8_t /*data1*/,
    uint8_t /*mappedValue*/,
    uint8_t /*midiChannel*/,
    uint8_t /*slotIndex*/
    )> callback);

    /** Read pot/channel settings from EEPROM. */
    void loadFromEEPROM();
    /** Persist current pot settings to EEPROM. */
    void saveToEEPROM();
    /** Reset EEPROM mappings to defaults. */
    void resetEEPROM();

    /** Return the last raw value read from a pot. */
    int getLastValue(int potIndex) const;

    /** Set the MIDI channel for a pot. */
    void setChannel(int potIndex, uint8_t channel);
    /** Set the CC number for a pot. */
    void setCCNumber(int potIndex, uint8_t ccNumber);

    uint8_t getChannel(int potIndex);
    uint8_t getCCNumber(int potIndex);

    /** Scan all pots, sending MIDI via the callback when values change. */
    void processPots(LEDManager& ledManager, std::vector<EnvelopeFollower>& envelopes);

    /** Specify the envelope pair used for ARG operations. */
    void setArgEnvelopePair(int a, int b);

    /** Retrieve the current envelope pair selection. */
    void getArgEnvelopePair(int &a, int &b) const;

    /** Direct raw ADC read helper used in tests. */
    int readRawPot(uint8_t potIndex);
};

#endif // POTENTIOMETER_MANAGER_H
