#include "EnvelopeFollower.h"

EnvelopeFollower::EnvelopeFollower(int pin, PotentiometerManager* pm)
    : audioInputPin(pin), currentEnvelopeLevel(0), modulationTargetCC(-1),
      isActive(false), filterType(LINEAR), potManager(pm) {}

int EnvelopeFollower::readEnvelopeLevel() {
    // Read and process the envelope from the audio input pin
    int rawValue = analogRead(audioInputPin);
    // Map the raw value to an appropriate range (e.g., 0–127 for MIDI)
    return map(rawValue, 0, 1023, 0, 127);
}

int EnvelopeFollower::processEnvelopeLevel(int level) {
    switch (filterType) {
        case LINEAR:
            return level; // No changes for linear
        case OPPOSITE_LINEAR:
            return 127 - level; // Invert the value
        case EXPONENTIAL:
            return pow(level / 127.0, 2) * 127; // Quadratic scaling
        case RANDOM:
            return random(0, 127); // Random envelope level
        default:
            return level;
    }
}

void EnvelopeFollower::update() {
    if (isActive) {
        int rawLevel = readEnvelopeLevel(); // Read raw analog input
        currentEnvelopeLevel = processEnvelopeLevel(rawLevel); // Apply filter
    }
}

void EnvelopeFollower::applyToCC(int potIndex, uint8_t& ccValue) {
    if (isActive && modulationTargetCC >= 0) {
        int modulatedValue = ccValue + currentEnvelopeLevel;
        ccValue = constrain(modulatedValue, 0, 127); // Ensure within MIDI range
    }
}

void EnvelopeFollower::toggleActive(bool state) {
    if (isActive != state) {
        isActive = state;
    }
}

bool EnvelopeFollower::getActiveState() const {
    return isActive;
}

void EnvelopeFollower::setModulationTarget(int cc) {
    modulationTargetCC = cc;
}

void EnvelopeFollower::setFilterType(FilterType type) {
    filterType = type;
}

EnvelopeFollower::FilterType EnvelopeFollower::getFilterType() const {
    return filterType;
}

int EnvelopeFollower::getEnvelopeLevel() const {
    return currentEnvelopeLevel;
}