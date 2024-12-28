#include "PotentiometerManager.h"
#include "EnvelopeFollower.h" // Include full definition here
#include <EEPROM.h>

bool dirtyFlags[NUM_POTS] = {false};

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
    for (int i = 0; i < PRIMARY_MUX_PINS; i++) {
        digitalWrite(primaryMuxPins[i], (bank >> i) & 1);
    }
}

void PotentiometerManager::selectPotBank(uint8_t pot) {
    for (int i = 0; i < SECONDARY_MUX_PINS; i++) {
        digitalWrite(secondaryMuxPins[i], (pot >> i) & 1);
    }
}

void PotentiometerManager::loadFromEEPROM() {
    for (int i = 0; i < NUM_POTS; i++) {
        int address = i * 2;
        potChannels[i] = EEPROM.read(address);
        potCCNumbers[i] = EEPROM.read(address + 1);
    }
}

void PotentiometerManager::saveToEEPROM() {
    for (int i = 0; i < NUM_POTS; i++) {
        int address = i * 2;
        EEPROM.update(address, potChannels[i]);
        EEPROM.update(address + 1, potCCNumbers[i]);
    }
}

void PotentiometerManager::resetEEPROM() {
    for (int i = 0; i < NUM_POTS; i++) {
        potChannels[i] = 1;
        potCCNumbers[i] = i;
    }
    saveToEEPROM();
}

void PotentiometerManager::setChannel(int potIndex, uint8_t channel) {
    if (potIndex < NUM_POTS) {
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
    for (int i = 0; i < NUM_POTS; i++) {
        int currentValue = analogRead(analogPin); // Simulated pot value
        if (abs(currentValue - potLastValues[i]) > 2) { // Threshold to avoid jitter
            potLastValues[i] = currentValue;
            dirtyFlags[i] = true;

            // Update LEDs if dirty
            ledManager.setPotValue(i, currentValue);
        }
    }
}