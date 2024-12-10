#ifndef ENVELOPE_FOLLOWER_H
#define ENVELOPE_FOLLOWER_H

#include <Arduino.h>
#include "PotentiometerManager.h"

class EnvelopeFollower {
private:
    int audioInputPin;           // Pin for audio input
    int currentEnvelopeLevel;    // Current envelope value
    int modulationTargetCC;      // Target MIDI CC to modulate
    bool isActive;               // Is the envelope follower active?
    PotentiometerManager* potManager;

    int readEnvelopeLevel();     // Helper to read and calculate the envelope level from audio input

public:
    EnvelopeFollower(int pin, PotentiometerManager* pm)
        : audioInputPin(pin), potManager(pm), currentEnvelopeLevel(0), modulationTargetCC(-1), isActive(false) {}

    void setModulationTarget(int cc);  // Set the MIDI CC target
    void toggleActive();               // Toggle active state of the envelope follower
    bool getActiveState() const;       // Check if the envelope follower is active

    void update();                     // Update the envelope level
    void applyToCC(int potIndex, uint8_t& ccValue);  // Apply modulation to a specific CC value
};

#endif // ENVELOPE_FOLLOWER_H
