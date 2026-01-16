#pragma once

#include <cstdint>
#include "LFOShape.h"
#include "LFOClock.h"

/**
 * Lightweight low-frequency oscillator with free-run and MIDI clock sync.
 * The LFO caches its own phase so updates are incremental and CPU stays bounded.
 */
class LFO {
  public:
    /** Create an LFO with sane defaults (sine, 1 Hz, bipolar). */
    LFO();

    /** Choose which waveform to render. */
    void setShape(LFOShape shape);
    /** Return the currently selected waveform. */
    LFOShape getShape() const;

    /** Set free-run speed in Hz (ignored when sync enabled). */
    void setFrequencyHz(float hz);
    /** Read back the free-run frequency in Hz. */
    float getFrequencyHz() const;

    /** Set modulation depth in the 0..1 range. */
    void setDepth(float depth);
    /** Return the current depth in the 0..1 range. */
    float getDepth() const;

    /** Toggle bipolar (-1..1) vs unipolar (0..1) output mode. */
    void setBipolar(bool bipolar);
    /** Return true if bipolar output is enabled. */
    bool isBipolar() const;

    /** Reset phase to 0.0 (start of cycle). */
    void resetPhase();
    /** Force the phase to a specific 0..1 position. */
    void setPhase(float phase);
    /** Read the current 0..1 phase position. */
    float getPhase() const;

    /** Seed the internal RNG for Sample/Hold and Random Slew. */
    void setRandomSeed(uint32_t seed);

    /** Enable or disable clock sync mode. */
    void setSyncEnabled(bool enabled);
    /** Return true when sync mode is active. */
    bool isSyncEnabled() const;
    /** Set the division/multiplication ratio for sync mode. */
    void setSyncRatio(LFOSyncRatio ratio);
    /** Read back the current sync ratio. */
    LFOSyncRatio getSyncRatio() const;

    /**
     * Advance phase in free-run mode.
     * @param deltaSeconds Time delta since last update (seconds).
     */
    void advanceFreeRun(float deltaSeconds);
    /**
     * Advance phase based on MIDI tick count.
     * @param tickDelta      New ticks observed since last update.
     * @param ticksPerCycle  Number of ticks for a full LFO cycle.
     */
    void advanceClockTicks(uint32_t tickDelta, uint32_t ticksPerCycle);

    /** Current output value after depth and polarity. */
    float value() const;

  private:
    /** Render the raw waveform value for a normalized phase (0..1). */
    float renderRaw(float phase) const;
    /** Generate a new random value in 0..1 using an LCG. */
    float nextRandom();
    /** Smooth step helper for slew interpolation. */
    float smoothStep(float t) const;
    /** Handle cycle wraps so per-cycle state can update. */
    void handleWrap(uint8_t wraps);

    LFOShape shape_ = LFOShape::Sine; //!< Waveform to render
    float frequencyHz_ = 1.0f;        //!< Free-run frequency in Hz
    float depth_ = 1.0f;              //!< Output depth (0..1)
    bool bipolar_ = true;             //!< True => -1..1 output, false => 0..1
    float phase_ = 0.0f;              //!< Normalized phase (0..1)

    float sampleHold_ = 0.0f;         //!< Latched value for Sample/Hold
    float slewStart_ = 0.0f;          //!< Slew interpolation start
    float slewTarget_ = 0.0f;         //!< Slew interpolation target
    uint32_t rngState_ = 0x12345678u; //!< RNG state for random shapes
    bool syncEnabled_ = false;        //!< True when clock sync is active
    LFOSyncRatio syncRatio_ = LFOSyncRatio::Div1; //!< Clock sync ratio
};
