#ifndef ENVELOPE_FOLLOWER_H
#define ENVELOPE_FOLLOWER_H

#include <Arduino.h>
#include "PotentiometerManager.h"
#include "BiquadFilter.h"
#include "Globals.h"

// Forward declaration
class PotentiometerManager;

class EnvelopeFollower {
public:
    /**
     *FilterType enum
     */
    enum FilterType {
        LINEAR,
        OPPOSITE_LINEAR,
        EXPONENTIAL,
        RANDOM,
        LOWPASS,
        HIGHPASS,
        BANDPASS
    };

    /**
     * Distinguish between standard envelope follower (SEF) and ARG mode.
     */
    enum Mode {
        SEF,
        ARG
    };

    /**
     * Enumerate the argument-based methods (A+B, A-B, etc.).
     */
    enum ARG_Method {
        PLUS,
        MIN,
        PECK,
        SHAV,
        SQAR,
        BABS,
        TABS
    };


private:
    float shapingFreq = 1000.0f;  // Frequency or shaping parameter
    float shapingQ = 0.707f;      // Resonance or secondary shaping parameter
    int audioInputPin;            // Pin for audio input
    int currentEnvelopeLevel;     // Current envelope value
    int modulationTargetCC;       // Target MIDI CC
    bool isActive;                // Is envelope follower active?

    // Existing filter type
    FilterType filterType;
    // Track whether we're in SEF or ARG mode
    Mode mode;
    // Which ARG method is selected
    ARG_Method argMethod;
    // Envelope indices used by ARG mode
    int envelopeA;
    int envelopeB;

    PotentiometerManager* potManager;
    BiquadFilter filter;          // Existing custom filter

public:
    EnvelopeFollower(int pin, PotentiometerManager* pm);
    void setModulationTarget(int cc);
    void toggleActive(bool state);
    bool getActiveState() const;
    int processEnvelopeLevel(int level);

    /**
     * Filter handling.
     */
    void setFilterType(FilterType type);
    void configureFilter(float frequency, float q);
    FilterType getFilterType() const;

    /**
     * Primary update cycle.
     */
    void update();

    /**
     * Original applyToCC method.
     */
    void applyToCC(int potIndex, uint8_t& ccValue);

    /**
     * Get the current envelope level.
     */
    int getEnvelopeLevel() const;

    /**
     *Switch between SEF and ARG modes.
     */
    void setMode(Mode newMode);
    Mode getMode() const {
        return mode;
    };

    /**
     *Choose which method (A+B, etc.) for ARG mode.
     */
    void setARGMethod(ARG_Method method);

    /**
     *Select the two analog inputs for ARG calculations.
     */
    void setEnvelopePair(int envA, int envB);
};

#endif // ENVELOPE_FOLLOWER_H