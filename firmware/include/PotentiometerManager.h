#ifndef POTENTIOMETER_MANAGER_H
#define POTENTIOMETER_MANAGER_H

#include <Arduino.h>
#include <functional>
#include <vector> // For std::vector
#include "LEDManager.h"
#include "Utility.h"
#include "ConfigManager.h"

// Forward declaration to avoid circular dependency
class EnvelopeFollower;

#define NUM_POTS 42
#define PRIMARY_MUX_PINS 3
#define SECONDARY_MUX_PINS 3

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
    std::function<void(uint8_t, uint8_t, uint8_t)> midiCallback;

    // Helper for filtered analog reads
    int readAnalogFiltered(uint8_t pin); // New function for analog filtering

    int argEnvA;
    int argEnvB;

public:
    PotentiometerManager(
        const uint8_t* primaryPins, 
        const uint8_t* secondaryPins, 
        uint8_t analogPin
    );

    void setMidiCallback(std::function<void(uint8_t, uint8_t, uint8_t)> callback);

    void loadFromEEPROM();
    void saveToEEPROM();
    void resetEEPROM();
    int getLastValue(int potIndex) const;
    void setChannel(int potIndex, uint8_t channel);
    void setCCNumber(int potIndex, uint8_t ccNumber);
    uint8_t getChannel(int potIndex);
    uint8_t getCCNumber(int potIndex);

    // Updated to accept envelopes
    void processPots(LEDManager& ledManager, std::vector<EnvelopeFollower>& envelopes);

    void setArgEnvelopePair(int a, int b);
    void getArgEnvelopePair(int &a, int &b) const;
};

#endif // POTENTIOMETER_MANAGER_H