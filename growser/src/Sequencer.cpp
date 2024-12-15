#include "SequenceManager.h"

Sequencer::Sequencer() : numActiveSteps(42), currentStep(0), lastInteractionTime(0) {
    // Initialize all step values to 0
    memset(steps, 0, sizeof(steps));
}

void Sequencer::setStepValue(uint8_t potIndex, uint8_t stepIndex, uint8_t value) {
    if (potIndex < NUM_POTS && stepIndex < MAX_STEPS) {
        steps[potIndex][stepIndex] = value;
    }
}

uint8_t Sequencer::getStepValue(uint8_t potIndex) const {
    if (potIndex < NUM_POTS) {
        return steps[potIndex][currentStep];
    }
    return 0;
}

void Sequencer::advanceStep() {
    currentStep = (currentStep + 1) % numActiveSteps;
}

void Sequencer::setNumActiveSteps(uint8_t steps) {
    if (steps > 0 && steps <= MAX_STEPS) {
        numActiveSteps = steps;
    }
}

uint8_t Sequencer::getNumActiveSteps() const {
    return numActiveSteps;
}

void Sequencer::resetSequencer() {
    currentStep = 0;
}

void Sequencer::updateLastInteraction() {
    lastInteractionTime = millis();
}

bool Sequencer::isActive() const {
    return (millis() - lastInteractionTime) < ACTIVE_TIMEOUT;
}