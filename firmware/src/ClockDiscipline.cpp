#include "ClockDiscipline.h"

#include <cmath>

namespace {
constexpr float kDriftThreshold = 0.25f; // 25% change in tempo triggers resync
constexpr float kSmoothing = 0.2f;      // smoothing factor for ms-per-tick tracking
} // namespace

ClockDiscipline::ClockDiscipline()
    : lastTickCount_(0), lastTickTimeMs_(0), msPerTick_(0.0f), running_(false), resumed_(false),
      drifted_(false), pendingTicks_(0) {}

void ClockDiscipline::reset() {
    lastTickCount_ = 0;
    lastTickTimeMs_ = 0;
    msPerTick_ = 0.0f;
    running_ = false;
    resumed_ = false;
    drifted_ = false;
    pendingTicks_ = 0;
}

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

uint32_t ClockDiscipline::consumeTicks() {
    uint32_t ticks = pendingTicks_;
    pendingTicks_ = 0;
    return ticks;
}

bool ClockDiscipline::justResumed() const { return resumed_; }

void ClockDiscipline::clearResumeFlag() { resumed_ = false; }

bool ClockDiscipline::driftDetected() const { return drifted_; }

void ClockDiscipline::clearDriftFlag() { drifted_ = false; }

float ClockDiscipline::msPerTick() const { return msPerTick_; }
