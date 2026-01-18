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
 * EnvelopeFollower env(A0, &potManager, 0); // index 0 so ConfigManager knows where to stash
 * offsets env.setModulationTarget(42);                 // spit out MIDI CC 42
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
    enum FilterType { LINEAR, OPPOSITE_LINEAR, EXPONENTIAL, RANDOM, LOWPASS, HIGHPASS, BANDPASS };

    /** Operating modes for the follower. */
    enum Mode { SEF, ARG };

    /** Envelope detection modes. */
    enum class EFMode : uint8_t {
        Peak = 0, //!< Half-wave rectified peak + RC smoothing
        RMS,      //!< RMS-ish leaky integration for energy detection
        Gate,     //!< Threshold gate with hysteresis
        Follower  //!< Fast attack/release follower curve
    };

    /** Math tricks available when blending two envelopes in ARG mode. */
    enum ARG_Method {
        PLUS,
        MIN,
        PECK,
        SHAV,
        SQAR,
        BABS,
        TABS,
        MULT,
        DIVI,
        AVG,
        XABS,
        MAXX,
        MINN,
        XORR
    };

    /**
     * Snapshot of the envelope follower state for diagnostics.
     */
    struct EfStats {
        float baseline = 0.0f;      //!< Current baseline used for subtraction
        float gain = 1.0f;          //!< Current total gain multiplier
        int value = 0;              //!< Latest scaled MIDI value (0..127-ish)
        EFMode mode = EFMode::Peak; //!< Active detection mode
    };

    /**
     * Per-mode configuration data used by the envelope engine.
     */
    struct EfModeSettings {
        EFMode mode = EFMode::Peak;    //!< Detection mode to apply
        uint16_t attackMs = 5;         //!< Attack time for follower/gate
        uint16_t releaseMs = 20;       //!< Release time for follower/gate
        uint16_t rmsWindowMs = 50;     //!< Integration window for RMS-ish mode
        uint16_t baselineTauMs = 2000; //!< Time constant for baseline tracker
        uint16_t gainTauMs = 3000;     //!< Time constant for auto-gain
        uint8_t gateThreshold = 16;    //!< Gate threshold (0..127)
        uint8_t gateHysteresis = 4;    //!< Gate hysteresis band
        uint8_t activityThreshold = 4; //!< Activity detect threshold
        uint8_t gainTarget = 102;      //!< Auto-gain target (~80% FS)
        bool autoBaseline = true;      //!< Enable baseline tracking
        bool autoGain = true;          //!< Enable auto-gain tracking
    };

  private:
    float shapingFreq = 1000.0f; // Frequency or shaping parameter
    float shapingQ = 0.707f;     // Resonance or secondary shaping parameter
    int audioInputPin;           // Pin for audio input
    uint8_t index;               // Which follower we are; used for EEPROM writes
    int currentEnvelopeLevel;    // Current envelope value
    int modulationTargetCC;      // Target MIDI CC
    bool isActive;               // Is envelope follower active?

    // Existing filter type
    FilterType filterType;
    // Track whether we're in SEF or ARG mode
    Mode mode;
    EFMode efMode = EFMode::Peak; // Current detection mode
    // Which ARG method is selected
    ARG_Method argMethod;
    // Envelope indices used by ARG mode (store as Teensy analog pins)
    int envelopeA;
    int envelopeB;

    // Calibration values
    float baseline = 0.0f;         // value subtracted from raw input to ditch noise
    float gain = 1.0f;             // scales the baseline-adjusted level before MIDI mapping
    float externalGainTrim = 1.0f; // Extra multiplier for modulation sources
    float autoGain = 1.0f;         // Auto-calculated gain multiplier
    EfModeSettings efSettings{};   // Per-mode parameters for detection/auto-cal

    // ADC and smoothing tweaks
    uint8_t oversampleCount = 4;    // number of reads per update
    float smoothingAlpha = 0.2f;    // EWMA weight for new samples
    int smoothedLevel = 0;          // running smoothed MIDI value
    float vref;                     // cached reference voltage
    float peakState = 0.0f;         //!< Smoothed peak detector state
    float rmsState = 0.0f;          //!< RMS integrator state
    bool gateOpen = false;          //!< Gate hysteresis state
    unsigned long lastUpdateMs = 0; //!< Timestamp of last update call

    PotentiometerManager *potManager;
    BiquadFilter filter; // Existing custom filter
    /**
     * Read the raw envelope level from the configured analog pin
     * and map it to the 0-127 MIDI range.
     */
    int readEnvelopeLevel(float dtSeconds);

  public:
    /**
     * Create an envelope follower attached to the given analog input.
     * The PotentiometerManager reference allows the processed value to
     * be applied back to a CC slot.
     */
    EnvelopeFollower(int pin, PotentiometerManager *pm, uint8_t id);

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
    /** Sneak out the shaping frequency knob (Hz for filters, scaler for curves). */
    float getShapingFrequency() const;
    /** Show the stored shaping Q / resonance parameter. */
    float getShapingQ() const;

    /**
     * Read the analog pin, process the value and store it. Call every loop
     * when the follower is active.
     */
    void update();

    /**
     * Modulate the provided CC value with the current envelope level. The
     * caller handles deduping and transmission.
     */
    void applyToCC(int potIndex, uint8_t &ccValue);

    /** Return the last processed envelope level (0-127). */
    int getEnvelopeLevel() const;

    /** Switch between SEF and ARG operating modes. */
    void setMode(Mode newMode);
    Mode getMode() const { return mode; }
    /** Set the envelope detection mode (Peak/RMS/Gate/Follower). */
    void setMode(EFMode newMode);
    /** Return the active envelope detection mode. */
    EFMode getEfMode() const { return efMode; }

    /** Select which arithmetic method to use in ARG mode. */
    void setARGMethod(ARG_Method method);
    /** Report which ARG math trick we currently have armed. */
    ARG_Method getARGMethod() const;

    /** Specify which two inputs feed the ARG calculations. */
    void setEnvelopePair(int envA, int envB);
    /** Return the configured A input pin for the ARG blender. */
    int getEnvelopeA() const;
    /** Return the configured B input pin for the ARG blender. */
    int getEnvelopeB() const;

    /**
     * Sample Vref and the current input, then burn the baseline to EEPROM so
     * the follower remembers where "silence" lives.
     */
    void calibrate();

    /**
     * Take a short average of the input to calculate the noise baseline.
     * This offset gets subtracted from every raw read before scaling.
     */
    void calibrateBaseline();

    /** Manually stash a baseline offset (e.g., from EEPROM). */
    void setBaseline(float b) { baseline = b; }
    /** Read back the current baseline. */
    float getBaseline() const { return baseline; }
    /** Force a new Vref without recalibrating. */
    void setVref(float v) { vref = v; }

    /**
     * Adjust the scaling factor applied after baseline removal.
     * Higher gain makes the envelope punchier before it's squeezed into 0–127.
     */
    void setGain(float g) { gain = g; }
    /** Read back the current gain multiplier. */
    float getGain() const { return gain; }
    /** Apply a temporary gain trim (e.g. from modulators). */
    void setExternalGainTrim(float trim);
    /** Configure the detection mode and its parameters. */
    /** Apply a new mode settings bundle (includes auto-cal flags). */
    void setModeSettings(const EfModeSettings &settings);
    /** Read back diagnostics. */
    /** Return a diagnostics snapshot of the current EF state. */
    EfStats getStats() const;

    /** Set how many ADC samples to average per update. */
    void setOversampleCount(uint8_t count);
    uint8_t getOversampleCount() const;

    /** Set the EWMA smoothing factor applied after oversampling. */
    void setSmoothingAlpha(float alpha);
    float getSmoothingAlpha() const;
    /** Apply a stored EF settings snapshot to this follower in one call. */
    void configureFromEfSettings(const MIDISlot::EfSettings &settings);
    /** Convert persisted filter enums into the runtime filter type. */
    static FilterType filterFromEfType(MIDISlot::EfSettings::FilterType type);

    /** Read back the current detection mode settings. */
    /** Return the current mode settings bundle. */
    EfModeSettings getModeSettings() const;

  private:
    float detectPeak(float level, float dtSeconds);
    float detectRms(float level, float dtSeconds);
    float detectGate(float level);
    float detectFollower(float level, float dtSeconds);
    float updateAutoGain(float inputLevel, float dtSeconds);
    void updateAutoBaseline(float inputLevel, float dtSeconds);
    float modeOutput(float inputLevel, float dtSeconds);
    float gainScale() const;
};

#endif // ENVELOPE_FOLLOWER_H
