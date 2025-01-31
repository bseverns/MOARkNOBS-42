#ifndef ENVELOPE_FOLLOWER_H
#define ENVELOPE_FOLLOWER_H

#include <Arduino.h>
#include <MIDIHandler.h>
#include <PotentiometerManager.h>

// Forward declare PotentiometerManager
class PotentiometerManager;

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
    PotentiometerManager* potManager; // Pointer to PotentiometerManager
    int assignedPot;             // Index of the associated potentiometer

    int readEnvelopeLevel();     // Helper to read and calculate the envelope level from audio input
    int processEnvelopeLevel(int level); // Process the envelope level based on filter type

public:
    EnvelopeFollower(int pin, PotentiometerManager* pm);

    void setModulationTarget(int cc);
    void toggleActive(bool state);
    bool getActiveState() const;
    void setAssignedPot(int pot) { assignedPot = pot; }
    int getAssignedPot() const { return assignedPot; }
    void setFilterType(FilterType type);
    FilterType getFilterType() const;
    void update();
    void applyToCC(int potIndex, uint8_t& ccValue);
    int getEnvelopeLevel() const;
};

#endif // ENVELOPE_FOLLOWER_H