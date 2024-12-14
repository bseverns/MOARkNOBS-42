#include <PotentiometerManager.h>
#include <Arduino.h>
#include <EEPROM.h>

PotentiometerManager::PotentiometerManager(uint8_t primaryPins[], uint8_t secondaryPins[], uint8_t analogPin)
    : analogPin(analogPin) {
    // Initialize mux control pins
    for (int i = 0; i < PRIMARY_MUX_PINS; i++) {
        primaryMuxPins[i] = primaryPins[i];
        pinMode(primaryMuxPins[i], OUTPUT);
        digitalWrite(primaryMuxPins[i], LOW);
    }
    for (int i = 0; i < SECONDARY_MUX_PINS; i++) {
        secondaryMuxPins[i] = secondaryPins[i];
        pinMode(secondaryMuxPins[i], OUTPUT);
        digitalWrite(secondaryMuxPins[i], LOW);
    }

    // Initialize pots
    for (int i = 0; i < NUM_POTS; i++) {
        potChannels[i] = 1; // Default to channel 1
        potCCNumbers[i] = i; // Default to sequential CC numbers
        potLastValues[i] = -1; // Ensure the first read sends values
    }
}

void PotentiometerManager::selectMuxBank(uint8_t bank) {
    for (int i = 0; i < PRIMARY_MUX_PINS; i++) {
        digitalWrite(primaryMuxPins[i], (bank >> i) & 0x01);
    }
}

void PotentiometerManager::selectPotBank(uint8_t pot) {
    for (int i = 0; i < SECONDARY_MUX_PINS; i++) {
        digitalWrite(secondaryMuxPins[i], (pot >> i) & 0x01);
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
    int potIndex = 0;

    for (uint8_t primaryBank = 0; primaryBank < (1 << PRIMARY_MUX_PINS); primaryBank++) {
        selectMuxBank(primaryBank);
        for (uint8_t potBank = 0; potBank < (1 << SECONDARY_MUX_PINS); potBank++) {
            selectPotBank(potBank);

            if (potIndex >= NUM_POTS) return; // Avoid overflow

            int currentValue = analogRead(analogPin) >> 3; // Scale to MIDI range (0–127)

            if (abs(currentValue - potLastValues[potIndex]) > 2) { // Threshold to avoid jitter
                potLastValues[potIndex] = currentValue;

                // Send MIDI CC message
                midiHandler.sendControlChange(
                    potCCNumbers[potIndex], currentValue, potChannels[potIndex]
                );

                // Update corresponding LED
                ledManager.setPotValue(potIndex, currentValue);
            }

            potIndex++;
        }
    }
}
