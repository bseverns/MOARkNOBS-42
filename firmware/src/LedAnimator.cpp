#include "LedAnimator.h"
#include "LEDManager.h"
#include <algorithm>
#include <cstddef>

namespace {
constexpr unsigned long kPeakHoldMs = 300;
constexpr float kPeakDecay = 1.5f;
constexpr float kTrailDecay = 1.2f;
constexpr float kClockPulseDecay = 0.12f;
constexpr unsigned long kDiagnosticCycleMs = 2500;
constexpr LedMode kModeOrder[] = {LedMode::Static, LedMode::PeakHold, LedMode::Trail,
                                  LedMode::ClockPulse};
constexpr size_t kModeCount = sizeof(kModeOrder) / sizeof(kModeOrder[0]);
} // namespace

LedAnimator::LedAnimator(LEDManager &leds) : ledManager(leds) {}

void LedAnimator::setMode(LedMode newMode) {
    mode = newMode;
    clockPulseStrength = 0.0f;
}

LedMode LedAnimator::getMode() const { return mode; }

void LedAnimator::cycleMode() {
    for (size_t idx = 0; idx < kModeCount; ++idx) {
        if (kModeOrder[idx] == mode) {
            mode = kModeOrder[(idx + 1) % kModeCount];
            clockPulseStrength = 0.0f;
            return;
        }
    }
    mode = kModeOrder[0];
}

void LedAnimator::setPotTarget(uint8_t index, uint8_t value) {
    if (index >= potStates.size())
        return;
    potStates[index].target = value;
}

void LedAnimator::setEnvelopeTarget(uint8_t index, uint8_t value) {
    if (index >= envelopeStates.size())
        return;
    envelopeStates[index].target = value;
}

uint8_t LedAnimator::computeLevel(ChannelState &state, uint8_t raw, unsigned long nowMs) {
    state.target = raw;
    switch (mode) {
    case LedMode::Static:
        state.peak = raw;
        state.trail = raw;
        return raw;
    case LedMode::PeakHold:
        if (raw >= state.peak) {
            state.peak = raw;
            state.holdUntil = nowMs + kPeakHoldMs;
        } else if (nowMs >= state.holdUntil && state.peak > raw) {
            float next = static_cast<float>(state.peak) - kPeakDecay;
            state.peak = static_cast<uint8_t>(std::max<float>(raw, next));
        }
        return state.peak;
    case LedMode::Trail:
        if (raw >= state.trail) {
            state.trail = raw;
        } else {
            state.trail = std::max<float>(raw, state.trail - kTrailDecay);
        }
        return static_cast<uint8_t>(state.trail);
    case LedMode::ClockPulse:
        return raw;
    default:
        return raw;
    }
}

void LedAnimator::paintPot(uint8_t index, uint8_t level) {
    if (index >= NUM_POTS)
        return;
    ledManager.setPotValue(index, level);
}

void LedAnimator::paintEnvelope(uint8_t index, uint8_t level) {
    if (index >= NUM_ENVELOPES)
        return;
    ledManager.setEnvelopeLevel(index, level);
}

void LedAnimator::tick(unsigned long nowMs, bool clockTick, bool diagnosticMode) {
    if (clockTick) {
        clockPulseStrength = 1.0f;
    } else {
        clockPulseStrength = std::max(0.0f, clockPulseStrength - kClockPulseDecay);
    }

    if (diagnosticMode) {
        if (diagLastCycle == 0) {
            diagLastCycle = nowMs;
        }
        if (nowMs - diagLastCycle >= kDiagnosticCycleMs) {
            diagLastCycle = nowMs;
            cycleMode();
        }
    } else {
        diagLastCycle = nowMs;
    }

    for (size_t idx = 0; idx < potStates.size(); ++idx) {
        uint8_t base = computeLevel(potStates[idx], potStates[idx].target, nowMs);
        uint8_t level = base;
        if (mode == LedMode::ClockPulse && clockPulseStrength > 0.0f) {
            uint8_t pulse = static_cast<uint8_t>(std::min(127.0f, clockPulseStrength * 127.0f));
            level = std::max(level, pulse);
        }
        paintPot(idx, level);
    }

    for (size_t idx = 0; idx < envelopeStates.size(); ++idx) {
        uint8_t base = computeLevel(envelopeStates[idx], envelopeStates[idx].target, nowMs);
        uint8_t level = base;
        if (mode == LedMode::ClockPulse && clockPulseStrength > 0.0f) {
            uint8_t pulse = static_cast<uint8_t>(std::min(127.0f, clockPulseStrength * 127.0f));
            level = std::max(level, pulse);
        }
        paintEnvelope(idx, level);
    }
}
