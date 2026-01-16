#include "LFO/LFOClock.h"

#include "MIDIHandler.h"

// Attach to the MIDI handler and reset tick tracking.
void LFOClock::attach(MIDIHandler *midi) {
    midi_ = midi;
    reset();
}

// Snap the current tick count so next delta is zero.
void LFOClock::reset() { lastTickCount_ = midi_ ? midi_->clockTickCount() : 0; }

// Return ticks since last call and update the snapshot.
uint32_t LFOClock::consumeTickDelta() {
    if (!midi_)
        return 0;
    uint32_t tickCount = midi_->clockTickCount();
    uint32_t delta = tickCount - lastTickCount_;
    lastTickCount_ = tickCount;
    return delta;
}

// Convert a sync ratio to the number of MIDI ticks per full LFO cycle.
uint32_t LFOClock::ticksPerCycle(LFOSyncRatio ratio) {
    switch (ratio) {
    case LFOSyncRatio::Div2:
        return 48;
    case LFOSyncRatio::Div4:
        return 96;
    case LFOSyncRatio::Div8:
        return 192;
    case LFOSyncRatio::Div16:
        return 384;
    case LFOSyncRatio::Div32:
        return 768;
    case LFOSyncRatio::Mul2:
        return 12;
    case LFOSyncRatio::Mul4:
        return 6;
    case LFOSyncRatio::Div1:
    default:
        return 24;
    }
}
