#ifndef POTENTIOMETER_MANAGER_H
#define POTENTIOMETER_MANAGER_H

#include <EEPROM.h>
#include "MIDIHandler.h"
#include "LEDManager.h"

#define NUM_POTS 42
#define EEPROM_SIZE (NUM_POTS * 2)

class PotentiometerManager {
private:
    uint8_t potChannels[NUM_POTS];  // MIDI channel for each pot
    uint8_t potCCNumbers[NUM_POTS]; // MIDI CC for each pot
    int potLastValues[NUM_POTS];    // Last read values for change detection

public:
    PotentiometerManager();

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
