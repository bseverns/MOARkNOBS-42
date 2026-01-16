#include "LFO/LFO.h"

#include <cmath>

namespace {
// 2*pi for sine wave generation without pulling in heavier math helpers.
constexpr float kTwoPi = 6.2831853071795864769f;
}

LFO::LFO() {
    // Prime the random-based shapes so they have stable initial values.
    sampleHold_ = nextRandom();
    slewStart_ = sampleHold_;
    slewTarget_ = nextRandom();
}

// Choose the waveform; applied on the next render.
void LFO::setShape(LFOShape shape) { shape_ = shape; }

LFOShape LFO::getShape() const { return shape_; }

// Free-run rate in Hz, clamped to non-negative.
void LFO::setFrequencyHz(float hz) { frequencyHz_ = (hz < 0.0f) ? 0.0f : hz; }

float LFO::getFrequencyHz() const { return frequencyHz_; }

// Depth scales the output, keeping it between 0 and 1.
void LFO::setDepth(float depth) {
    if (depth < 0.0f)
        depth_ = 0.0f;
    else if (depth > 1.0f)
        depth_ = 1.0f;
    else
        depth_ = depth;
}

float LFO::getDepth() const { return depth_; }

// Bipolar mode yields -1..1 output; unipolar yields 0..1.
void LFO::setBipolar(bool bipolar) { bipolar_ = bipolar; }

bool LFO::isBipolar() const { return bipolar_; }

// Restart the cycle at phase 0.
void LFO::resetPhase() { phase_ = 0.0f; }

// Force phase into the normalized 0..1 range.
void LFO::setPhase(float phase) {
    phase_ = phase;
    if (phase_ < 0.0f)
        phase_ = 0.0f;
    if (phase_ >= 1.0f)
        phase_ = std::fmod(phase_, 1.0f);
}

float LFO::getPhase() const { return phase_; }

// Seed the RNG and refresh the random-held state.
void LFO::setRandomSeed(uint32_t seed) {
    rngState_ = seed ? seed : 0x12345678u;
    sampleHold_ = nextRandom();
    slewStart_ = sampleHold_;
    slewTarget_ = nextRandom();
}

// Enable or disable clock-synced phase advance.
void LFO::setSyncEnabled(bool enabled) { syncEnabled_ = enabled; }

bool LFO::isSyncEnabled() const { return syncEnabled_; }

// Set the beat division/multiplication used for sync.
void LFO::setSyncRatio(LFOSyncRatio ratio) { syncRatio_ = ratio; }

LFOSyncRatio LFO::getSyncRatio() const { return syncRatio_; }

// Advance the phase using a free-run delta in seconds.
void LFO::advanceFreeRun(float deltaSeconds) {
    if (deltaSeconds <= 0.0f || frequencyHz_ <= 0.0f)
        return;

    float advance = frequencyHz_ * deltaSeconds;
    if (advance <= 0.0f)
        return;

    // Compute next phase and handle any wraps through the cycle.
    float next = phase_ + advance;
    if (next < 1.0f) {
        phase_ = next;
        return;
    }

    uint8_t wraps = static_cast<uint8_t>(next);
    phase_ = std::fmod(next, 1.0f);
    handleWrap(wraps);
}

// Advance the phase based on MIDI clock ticks.
void LFO::advanceClockTicks(uint32_t tickDelta, uint32_t ticksPerCycle) {
    if (tickDelta == 0 || ticksPerCycle == 0)
        return;

    float advance = static_cast<float>(tickDelta) / static_cast<float>(ticksPerCycle);
    // Convert ticks to normalized phase and wrap if needed.
    float next = phase_ + advance;
    if (next < 1.0f) {
        phase_ = next;
        return;
    }

    uint8_t wraps = static_cast<uint8_t>(next);
    phase_ = std::fmod(next, 1.0f);
    handleWrap(wraps);
}

// Return the depth-scaled output in bipolar or unipolar space.
float LFO::value() const {
    float raw = renderRaw(phase_);
    if (bipolar_) {
        return raw * depth_;
    }
    return (raw * 0.5f + 0.5f) * depth_;
}

// Render the raw waveform value for a normalized phase.
float LFO::renderRaw(float phase) const {
    switch (shape_) {
    case LFOShape::Triangle: {
        // Triangle wave (inverted to match existing expectations).
        float tri = 2.0f * std::fabs(2.0f * phase - 1.0f) - 1.0f;
        return -tri;
    }
    case LFOShape::Saw:
        return 2.0f * phase - 1.0f;
    case LFOShape::Square:
        return (phase < 0.5f) ? 1.0f : -1.0f;
    case LFOShape::SampleHold:
        // Latched once per cycle in handleWrap().
        return sampleHold_;
    case LFOShape::RandomSlew: {
        // Smooth transition from last random value to next target.
        float t = smoothStep(phase);
        return slewStart_ + (slewTarget_ - slewStart_) * t;
    }
    case LFOShape::Sine:
    default:
        return sinf(phase * kTwoPi);
    }
}

// Simple LCG for deterministic random values.
float LFO::nextRandom() {
    rngState_ = rngState_ * 1664525u + 1013904223u;
    uint32_t val = (rngState_ >> 9) & 0x7fffffu;
    float norm = static_cast<float>(val) / static_cast<float>(0x7fffffu);
    return norm * 2.0f - 1.0f;
}

// Smoothstep keeps the slew curve soft at the ends.
float LFO::smoothStep(float t) const { return t * t * (3.0f - 2.0f * t); }

// Update per-cycle state when the phase wraps.
void LFO::handleWrap(uint8_t wraps) {
    if (wraps == 0)
        return;
    if (shape_ == LFOShape::SampleHold) {
        sampleHold_ = nextRandom();
        return;
    }
    if (shape_ == LFOShape::RandomSlew) {
        slewStart_ = slewTarget_;
        slewTarget_ = nextRandom();
    }
}
