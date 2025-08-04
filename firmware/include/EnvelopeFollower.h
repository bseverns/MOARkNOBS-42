// Provides envelope following for modulation.
// Can combine two inputs or filter a single input in various shapes.
// Called by firmware_main.cpp; ButtonManager adjusts its settings.
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
 *
 * ## Quick and dirty setup
 * ```cpp
 * // Wire the follower to analog pin A0 and hand it a pot manager
 * EnvelopeFollower env(A0, &potManager);
 * env.setModulationTarget(42);                 // spit out MIDI CC 42
 * env.setMode(EnvelopeFollower::SEF);          // roll with Single Envelope mode
 * env.toggleActive(true);                      // let it rip
 * ```
 * baseline and gain are the attitude knobs:
 * - **baseline**: measured noise floor subtracted before anything else.
 *   Crank it up if the input won't shut up.
 * - **gain**: multiplier applied *after* baseline, before mapping to 0–127.
 *   Push it past 1.0f when you want the envelope to hit harder.
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
    float baseline = 0.0f; // value subtracted from raw input to ditch noise
    float gain = 1.0f;     // scales the baseline-adjusted level before MIDI mapping

    // ADC and smoothing tweaks
    uint8_t oversampleCount = 4; // number of reads per update
    float smoothingAlpha = 0.2f; // EWMA weight for new samples
    int smoothedLevel = 0;       // running smoothed MIDI value
    float vref;                  // cached reference voltage

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

    /**
     * Sample the voltage reference and current input to stash baseline offsets.
     */
    void calibrate();

    /**
     * Take a short average of the input to calculate the noise baseline.
     * This offset gets subtracted from every raw read before scaling.
     */
    void calibrateBaseline();

    /**
     * Adjust the scaling factor applied after baseline removal.
     * Higher gain makes the envelope punchier before it's squeezed into 0–127.
     */
    void setGain(float g) { gain = g; }

    /** Directly set or query the stored baseline offset. */
    void setBaseline(float b) { baseline = b; }
    float getBaseline() const { return baseline; }

    /** Set how many ADC samples to average per update. */
    void setOversampleCount(uint8_t count);
    uint8_t getOversampleCount() const;

    /** Set the EWMA smoothing factor applied after oversampling. */
    void setSmoothingAlpha(float alpha);
    float getSmoothingAlpha() const;

    /** Override the reference voltage used during reads. */
    void setVref(float ref) { vref = ref; }
};

#endif // ENVELOPE_FOLLOWER_H
