#ifndef ENVELOPE_FOLLOWER_H
#define ENVELOPE_FOLLOWER_H

#include <Arduino.h>
#include "PotentiometerManager.h"

class EnvelopeFollower {
public:
 enum FilterType {
        LINEAR,
        OPPOSITE_LINEAR,
        EXPONENTIAL,
        RANDOM
    };
private:
    int audioInputPin;           // Pin for audio input
    int currentEnvelopeLevel;    // Current envelope value
    int modulationTargetCC;      // Target MIDI CC to modulate
    bool isActive;               // Is the envelope follower active?
    FilterType filterType;       // Current filter type
    PotentiometerManager* potManager;
    int assignedPot;             // Index of the associated potentiometer

    int readEnvelopeLevel();     // Helper to read and calculate the envelope level from audio input
    int processEnvelopeLevel(int level); // Process the envelope level based on filter type

public:
    EnvelopeFollower(int pin, PotentiometerManager* pm);
   
    void setModulationTarget(int cc);         // Set the MIDI CC target
    void toggleActive(bool state);            // Set active state explicitly
    bool getActiveState() const;              // Check if the envelope follower is active
    void setAssignedPot(int pot) { assignedPot = pot; }
    int getAssignedPot() const { return assignedPot; }
    void setFilterType(FilterType type);      // Set the filter type
    FilterType getFilterType() const;         // Get the current filter type
    void update();                            // Update the envelope level
    void applyToCC(int potIndex, uint8_t& ccValue); // Apply modulation to a specific CC value
    int getEnvelopeLevel() const;             // Retrieve the current envelope level
};

#endif // ENVELOPE_FOLLOWER_H
