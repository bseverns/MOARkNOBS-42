#include "ClockDiscipline.h"

#include <cmath>

namespace {
constexpr float kDriftThreshold = 0.25f; // 25% change in tempo triggers resync
constexpr float kSmoothing = 0.2f;       // smoothing factor for ms-per-tick tracking
} // namespace

ClockDiscipline::ClockDiscipline()
    : lastTickCount_(0), lastTickTimeMs_(0), msPerTick_(0.0f), running_(false), resumed_(false),
      drifted_(false), pendingTicks_(0) {}

// Reset all observed clock state back to the "no external clock yet" baseline.
void ClockDiscipline::reset() {
    lastTickCount_ = 0;
    lastTickTimeMs_ = 0;
    msPerTick_ = 0.0f;
    running_ = false;
    resumed_ = false;
    drifted_ = false;
    pendingTicks_ = 0;
}

// Observe external MIDI clock progress and detect resumes/drift for the arpeggiator/runtime.
void ClockDiscipline::observe(uint32_t tickCount, unsigned long timestampMs, bool running) {
    if (!running) {
        running_ = false;
        pendingTicks_ = 0;
        return;
    }
    if (!running_) {
        running_ = true;
        resumed_ = true;
        pendingTicks_ = 0;
        lastTickCount_ = tickCount;
        lastTickTimeMs_ = timestampMs;
        return;
    }

    uint32_t delta = tickCount - lastTickCount_;
    if (delta == 0) {
        return;
    }

    if (timestampMs <= lastTickTimeMs_) {
        pendingTicks_ += delta;
        lastTickCount_ = tickCount;
        return;
    }

    unsigned long dt = timestampMs - lastTickTimeMs_;
    float candidate = static_cast<float>(dt) / static_cast<float>(delta);
    if (msPerTick_ > 0.0f) {
        float diff = fabsf(candidate - msPerTick_);
        if (diff / msPerTick_ > kDriftThreshold) {
            drifted_ = true;
        }
        msPerTick_ = (msPerTick_ * (1.0f - kSmoothing)) + (candidate * kSmoothing);
    } else {
        msPerTick_ = candidate;
    }

    pendingTicks_ += delta;
    lastTickCount_ = tickCount;
    lastTickTimeMs_ = timestampMs;
    resumed_ = false;
}

// Return and clear the number of newly observed ticks since the last consumer check.
uint32_t ClockDiscipline::consumeTicks() {
    uint32_t ticks = pendingTicks_;
    pendingTicks_ = 0;
    return ticks;
}

// Tell callers whether the clock just transitioned from stopped to running.
bool ClockDiscipline::justResumed() const { return resumed_; }

// Clear the one-shot resume flag after the runtime has reacted to it.
void ClockDiscipline::clearResumeFlag() { resumed_ = false; }

// Tell callers whether a significant tempo jump was detected.
bool ClockDiscipline::driftDetected() const { return drifted_; }

// Clear the one-shot drift flag after resync logic runs.
void ClockDiscipline::clearDriftFlag() { drifted_ = false; }

// Return the smoothed milliseconds-per-tick estimate for the current clock.
float ClockDiscipline::msPerTick() const { return msPerTick_; }
