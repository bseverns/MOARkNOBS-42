#include "PotentiometerManager.h"
#include <EEPROM.h>

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

void PotentiometerManager::processPots(LEDManager &ledManager) {
    int potIndex = 0;

    for (uint8_t primaryBank = 0; primaryBank < (1 << PRIMARY_MUX_PINS); primaryBank++) {
        selectMuxBank(primaryBank);
        for (uint8_t potBank = 0; potBank < (1 << SECONDARY_MUX_PINS); potBank++) {
            selectPotBank(potBank);

            if (potIndex >= NUM_POTS) return;

            int currentValue = analogRead(analogPin) >> 3; // Scale to MIDI range (0–127)

            if (abs(currentValue - potLastValues[potIndex]) > 2) { // Threshold to avoid jitter
                potLastValues[potIndex] = currentValue;

                // Send MIDI CC message via callback
                if (midiCallback) {
                    midiCallback(potCCNumbers[potIndex], currentValue, potChannels[potIndex]);
                }

                // Update corresponding LED
                ledManager.setPotValue(potIndex, currentValue);
            }

            potIndex++;
        }
    }
}
