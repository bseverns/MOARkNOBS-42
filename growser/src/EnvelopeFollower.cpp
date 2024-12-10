#include "EnvelopeFollower.h"

int EnvelopeFollower::readEnvelopeLevel() {
    // Read and process the envelope from the audio input pin
    int rawValue = analogRead(audioInputPin);
    // Map the raw value to an appropriate range (e.g., 0–127 for MIDI)
    return map(rawValue, 0, 1023, 0, 127);
}

void EnvelopeFollower::setModulationTarget(int cc) {
    modulationTargetCC = cc;
}

void EnvelopeFollower::toggleActive() {
    isActive = !isActive;
}

bool EnvelopeFollower::getActiveState() const {
    return isActive;
}

void EnvelopeFollower::update() {
    if (isActive) {
        currentEnvelopeLevel = readEnvelopeLevel();
    }
}

void EnvelopeFollower::applyToCC(int potIndex, uint8_t& ccValue) {
    if (isActive && modulationTargetCC >= 0) {
        // Use the current envelope level to modulate the given CC value
        int modulatedValue = ccValue + currentEnvelopeLevel;
        ccValue = constrain(modulatedValue, 0, 127); // Ensure the value stays within the MIDI range
    }
}
