// Updated EnvelopeFollower.h
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
    int assignedPot; // Index of the associated potentiometer
    int readEnvelopeLevel();     // Helper to read and calculate the envelope level from audio input

public:
    EnvelopeFollower(int pin, PotentiometerManager* pm);

    void setModulationTarget(int cc);  // Set the MIDI CC target
    void toggleActive(bool state);     // Set active state explicitly
    bool getActiveState() const;       // Check if the envelope follower is active
    void setAssignedPot(int pot) { assignedPot = pot; }
    int getAssignedPot() const { return assignedPot; }
    void update();                     // Update the envelope level
    void applyToCC(int potIndex, uint8_t& ccValue);  // Apply modulation to a specific CC value
    int getEnvelopeLevel() const;      // Retrieve the current envelope level
};

#endif // ENVELOPE_FOLLOWER_H