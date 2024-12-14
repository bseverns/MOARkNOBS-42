#ifndef POTENTIOMETER_MANAGER_H
#define POTENTIOMETER_MANAGER_H

#include <Arduino.h>
#include <MIDIHandler.h>
#include <LEDManager.h>

#define NUM_POTS 42
#define PRIMARY_MUX_PINS 3 // Number of pins for primary mux (to select mux bank)
#define SECONDARY_MUX_PINS 3 // Number of pins for secondary mux (to select pot bank in each mux)

class PotentiometerManager {
private:
    uint8_t primaryMuxPins[PRIMARY_MUX_PINS];   // Pins to select the primary mux bank
    uint8_t secondaryMuxPins[SECONDARY_MUX_PINS]; // Pins to select the pot bank
    uint8_t analogPin;                          // Analog pin connected to the mux output
    uint8_t potChannels[NUM_POTS];              // MIDI channel for each pot
    uint8_t potCCNumbers[NUM_POTS];             // MIDI CC number for each pot
    int potLastValues[NUM_POTS];                // Last read values for each pot
    void selectMuxBank(uint8_t bank);           // Select the primary mux bank
    void selectPotBank(uint8_t pot);            // Select the secondary mux pot

public:
    PotentiometerManager(uint8_t primaryPins[], uint8_t secondaryPins[], uint8_t analogPin);
    void loadFromEEPROM();
    void saveToEEPROM();
    void resetEEPROM();
    void setChannel(int potIndex, uint8_t channel);
    void setCCNumber(int potIndex, uint8_t ccNumber);
    uint8_t getChannel(int potIndex);
    uint8_t getCCNumber(int potIndex);
    void processPots(MIDIHandler &midiHandler, LEDManager &ledManager);
};

#endif // POTENTIOMETER_MANAGER_H
