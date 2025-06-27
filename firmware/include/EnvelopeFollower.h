#ifndef ENVELOPE_FOLLOWER_H
#define ENVELOPE_FOLLOWER_H

#include <Arduino.h>
#include "PotentiometerManager.h"
#include "BiquadFilter.h"
#include "Globals.h"

// Forward declaration
class PotentiometerManager;

/**
 * @brief Audio envelope follower with optional ARG combination mode.
 */
class EnvelopeFollower {
public:
    /** Available shaping/filter modes. */
    enum FilterType {
        LINEAR,
        OPPOSITE_LINEAR,
        EXPONENTIAL,
        RANDOM,
        LOWPASS,
        HIGHPASS,
        BANDPASS
    };

    /** Operating modes for the follower. */
    enum Mode {
        SEF,
        ARG
    };

    /** Methods used when in ARG mode. */
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
    /**
     * Read the raw envelope level from the configured analog pin
     * and map it to the 0-127 MIDI range.
     */
    int readEnvelopeLevel();

public:
    /**
     * @param pin Analog pin from which to read the audio envelope.
     * @param pm  Reference to the PotentiometerManager for CC routing.
     */
    EnvelopeFollower(int pin, PotentiometerManager* pm);

    /** MIDI CC that will be modulated by this envelope. */
    void setModulationTarget(int cc);

    /** Enable or disable the follower. */
    void toggleActive(bool state);

    /** Query whether the follower is currently active. */
    bool getActiveState() const;

    /** Apply the selected filter/curve to a raw level value. */
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
    /** Read the input pin and update the internal envelope value. */
    void update();

    /**
     * Original applyToCC method.
     */
    /** Modulate the provided CC value with the current envelope level. */
    void applyToCC(int potIndex, uint8_t& ccValue);

    /**
     * Get the current envelope level.
     */
    /** Return the last processed envelope level (0‑127). */
    int getEnvelopeLevel() const;

    /**
     *Switch between SEF and ARG modes.
     */
    /** Switch between SEF and ARG operating modes. */
    void setMode(Mode newMode);
    Mode getMode() const {
        return mode;
    };

    /**
     *Choose which method (A+B, etc.) for ARG mode.
     */
    /** Select which arithmetic method to use in ARG mode. */
    void setARGMethod(ARG_Method method);

    /**
     *Select the two analog inputs for ARG calculations.
     */
    /** Specify which two inputs feed the ARG calculations. */
    void setEnvelopePair(int envA, int envB);
};

#endif // ENVELOPE_FOLLOWER_H