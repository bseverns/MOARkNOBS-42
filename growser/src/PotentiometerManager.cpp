#include "PotentiometerManager.h"
#include <Arduino.h>

PotentiometerManager::PotentiometerManager() {
    // Initialize with defaults
    for (int i = 0; i < NUM_POTS; i++) {
        potChannels[i] = 1; // Default channel
        potCCNumbers[i] = i; // Default CC number
        potLastValues[i] = -1; // Set to -1 to ensure all pots send their first values
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
        EEPROM.write(address, potChannels[i]);
        EEPROM.write(address + 1, potCCNumbers[i]);
    }
    // Removed EEPROM.commit();
}

void PotentiometerManager::resetEEPROM() {
    for (int i = 0; i < NUM_POTS; i++) {
        potChannels[i] = 1;
        potCCNumbers[i] = i;
    }
    saveToEEPROM();
}

void PotentiometerManager::setChannel(int potIndex, uint8_t channel) {
    potChannels[potIndex] = channel;
}

void PotentiometerManager::setCCNumber(int potIndex, uint8_t ccNumber) {
    potCCNumbers[potIndex] = ccNumber;
}

uint8_t PotentiometerManager::getChannel(int potIndex) {
    return potChannels[potIndex];
}

uint8_t PotentiometerManager::getCCNumber(int potIndex) {
    return potCCNumbers[potIndex];
}

void PotentiometerManager::processPots(MIDIHandler &midiHandler, LEDManager &ledManager) {
    for (int i = 0; i < NUM_POTS; i++) {
        int currentValue = analogRead(i) >> 3; // Read and scale to 7-bit MIDI range (0–127)

        // Check if the value has changed significantly (debouncing threshold)
        if (abs(currentValue - potLastValues[i]) > 2) {
            potLastValues[i] = currentValue; // Update last value

            // Send MIDI CC message
            midiHandler.sendControlChange(
                potCCNumbers[i], currentValue, potChannels[i]
            );

            // Update corresponding LED
            ledManager.setPotValue(i, currentValue);
        }
    }
}
