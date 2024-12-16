#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <Arduino.h>

class PotentiometerManager;

#define MAX_STEPS 128
#define ACTIVE_TIMEOUT 3000  // 3 seconds of activity timeout
#define NUM_POTS 42 //42 pots on this unit

class Sequencer {
private:
    uint8_t steps[NUM_POTS][MAX_STEPS]; // Step values for each pot
    uint8_t numActiveSteps;            // Number of active steps in the sequence
    uint8_t currentStep;               // Current step index
    unsigned long lastInteractionTime; // Timestamp of the last interaction

public:
    Sequencer();
    void setStepValue(uint8_t potIndex, uint8_t stepIndex, uint8_t value);
    uint8_t getStepValue(uint8_t potIndex) const;
    void advanceStep();
    void setNumActiveSteps(uint8_t steps);
    uint8_t getNumActiveSteps() const;
    void resetSequencer();
    void updateLastInteraction();
    bool isActive() const;
};

#endif