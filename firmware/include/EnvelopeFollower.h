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

    // Calibration values
    float baseline = 0.0f;
    float gain = 1.0f;

    PotentiometerManager* potManager;
    BiquadFilter filter;          // Existing custom filter
    /**
     * Read the raw envelope level from the configured analog pin
     * and map it to the 0-127 MIDI range.
     */
    int readEnvelopeLevel();

public:
    /**
     * Create an envelope follower attached to the given analog input.
     * The PotentiometerManager reference allows the processed value to
     * be applied back to a CC slot.
     */
    EnvelopeFollower(int pin, PotentiometerManager* pm);

    /**
     * Choose which MIDI CC this follower will control. Call when a pot
     * is assigned an envelope.
     */
    void setModulationTarget(int cc);

    /** Enable or disable processing, typically from a button event. */
    void toggleActive(bool state);

    /** Return true if update() is currently processing new values. */
    bool getActiveState() const;

    /** Apply the selected shaping/filter algorithm to a raw level. */
    int processEnvelopeLevel(int level);

    /** Select which shaping/filter algorithm is active in SEF mode. */
    void setFilterType(FilterType type);
    /** Update the cutoff frequency and Q for the biquad filter modes. */
    void configureFilter(float frequency, float q);
    /** Query the current filter type. */
    FilterType getFilterType() const;

    /**
     * Read the analog pin, process the value and store it. Call every loop
     * when the follower is active.
     */
    void update();

    /**
     * Modulate the provided CC value with the current envelope level and
     * send the resulting MIDI message. Avoids sending duplicates.
     */
    void applyToCC(int potIndex, uint8_t& ccValue);

    /** Return the last processed envelope level (0-127). */
    int getEnvelopeLevel() const;

    /** Switch between SEF and ARG operating modes. */
    void setMode(Mode newMode);
    Mode getMode() const {
        return mode;
    };

    /** Select which arithmetic method to use in ARG mode. */
    void setARGMethod(ARG_Method method);

    /**
     * Specify which two inputs feed the ARG calculations. Call together
     * with setARGMethod when configuring the follower.
     */
    void setEnvelopePair(int envA, int envB);

    /** Take a short average of the input to calculate the noise baseline. */
    void calibrateBaseline();

    /** Adjust the scaling factor applied before mapping to MIDI. */
    void setGain(float g) { gain = g; }
};

#endif // ENVELOPE_FOLLOWER_H
