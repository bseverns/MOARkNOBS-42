#include "PotentiometerManager.h"
#include "EnvelopeFollower.h" // Include full definition here
#include <EEPROM.h>
#include "ConfigManager.h"

bool dirtyFlags[configManager.getNumPots()] = {false};
const float alpha = 0.1; // Smoothing factor
static int smoothedValue[configManager.getNumPots()] = {0};
#define CHANGE_THRESHOLD 2  // Adjust based on your noise tolerance

PotentiometerManager::PotentiometerManager(
    const uint8_t* primaryPins, 
    const uint8_t* secondaryPins, 
    uint8_t analogPin
) : primaryMuxPins(primaryPins), secondaryMuxPins(secondaryPins), analogPin(analogPin) {
    // Initialize pot default values
    for (int i = 0; i < NUM_POTS; i++) {
        potChannels[i] = 1;       // Default MIDI channel
        potCCNumbers[i] = i;      // Default MIDI CC number
        potLastValues[i] = -1;    // Ensure the first read updates
    }
}

void PotentiometerManager::setMidiCallback(std::function<void(uint8_t, uint8_t, uint8_t)> callback) {
    midiCallback = callback;
}

void PotentiometerManager::selectMuxBank(uint8_t bank) {
    static uint8_t lastBank = 255; // Track the last selected bank
    if (bank != lastBank) {
        for (int i = 0; i < PRIMARY_MUX_PINS; i++) {
            digitalWrite(primaryMuxPins[i], (bank >> i) & 1);
        }
        lastBank = bank;
    }
}

void PotentiometerManager::selectPotBank(uint8_t pot) {
    static uint8_t lastPot = 255; // Track the last selected pot
    if (pot != lastPot) {
        for (int i = 0; i < SECONDARY_MUX_PINS; i++) {
            digitalWrite(secondaryMuxPins[i], (pot >> i) & 1);
        }
        lastPot = pot;
    }
}

int PotentiometerManager::readAnalogFiltered(uint8_t pin) {
    int total = 0;
    const int numSamples = 4; // Number of samples for averaging

    for (int i = 0; i < numSamples; i++) {
        total += analogRead(pin); // Read analog value
        delayMicroseconds(10);    // Small delay for stability
    }

    return total / numSamples; // Return the averaged value
}

void PotentiometerManager::setChannel(int potIndex, uint8_t channel) {
    if (potIndex < configManager.getNumPots()) {
        potChannels[potIndex] = channel;
    }
}

int PotentiometerManager::getLastValue(int potIndex) const {
    if (potIndex >= 0 && potIndex < NUM_POTS) {
        return potLastValues[potIndex];
    } else {
        return -1; // Return a sentinel value for invalid index
    }
}

void PotentiometerManager::setCCNumber(int potIndex, uint8_t ccNumber) {
    if (potIndex < NUM_POTS) {
        potCCNumbers[potIndex] = ccNumber;
    }
}

uint8_t PotentiometerManager::getChannel(int potIndex) {
    return (potIndex < NUM_POTS) ? potChannels[potIndex] : 0;
}

uint8_t PotentiometerManager::getCCNumber(int potIndex) {
    return (potIndex < NUM_POTS) ? potCCNumbers[potIndex] : 0;
}


void PotentiometerManager::processPots(LEDManager& ledManager, std::vector<EnvelopeFollower>& envelopes) {
    for (uint8_t primaryBank = 0; primaryBank < (1 << PRIMARY_MUX_PINS); primaryBank++) {
        selectMuxBank(primaryBank);

        for (uint8_t secondaryBank = 0; secondaryBank < (1 << SECONDARY_MUX_PINS); secondaryBank++) {
            selectPotBank(secondaryBank);

            uint8_t potIndex = (primaryBank << SECONDARY_MUX_PINS) | secondaryBank;

            if (potIndex >= NUM_POTS) break;

            // Use filtered analog read
            int rawValue = readAnalogFiltered(analogPin);

            // Apply EWMA smoothing
           smoothedValue[potIndex] = Utility::exponentialMovingAverage(rawValue, smoothedValue[potIndex], alpha);

            // Smarter change detection
            if (abs(smoothedValue[potIndex] - potLastValues[potIndex]) > CHANGE_THRESHOLD) {
                potLastValues[potIndex] = smoothedValue[potIndex]; // Update last known value
                dirtyFlags[potIndex] = true;

                // Update LEDs to reflect the new value
                ledManager.setPotValue(potIndex, smoothedValue[potIndex]);

                // Send the MIDI update if a callback is set
                if (midiCallback) {
                    midiCallback(
                        potCCNumbers[potIndex],               // CC number for this pot
                        Utility::mapToMidiValue(smoothedValue[potIndex]), // Map value to MIDI range
                        potChannels[potIndex]                // Channel for this pot
                    );
                }
            }
        }
    }
}