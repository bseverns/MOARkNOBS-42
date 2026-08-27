#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "ButtonGestureTiming.h"

namespace {
struct Counts {
    int singles = 0;
    int doubles = 0;
};

void applyDecision(const DeferredPressDecision &decision, Counts &counts) {
    if (decision.fireSingle) ++counts.singles;
    if (decision.fireDouble) ++counts.doubles;
}

Counts runReleaseSequence(const std::vector<unsigned long> &releases) {
    bool pending = false;
    unsigned long lastRelease = 0;
    Counts counts;
    for (unsigned long release : releases) {
        if (pending && release > lastRelease + DOUBLE_PRESS_DELAY) {
            if (flushDeferredPress(pending, lastRelease, lastRelease + DOUBLE_PRESS_DELAY)) {
                ++counts.singles;
            }
        }
        applyDecision(registerDeferredRelease(pending, lastRelease, release), counts);
    }
    if (pending && flushDeferredPress(pending, lastRelease, lastRelease + DOUBLE_PRESS_DELAY)) {
        ++counts.singles;
    }
    return counts;
}

void testThresholdBoundaries() {
    bool pending = false;
    unsigned long lastRelease = 0;
    Counts counts;
    applyDecision(registerDeferredRelease(pending, lastRelease, 1000), counts);
    applyDecision(registerDeferredRelease(pending, lastRelease, 1299), counts);
    assert(counts.singles == 0 && counts.doubles == 1);

    pending = false;
    lastRelease = 0;
    counts = {};
    applyDecision(registerDeferredRelease(pending, lastRelease, 2000), counts);
    applyDecision(registerDeferredRelease(pending, lastRelease, 2300), counts);
    assert(counts.singles == 1 && counts.doubles == 0 && pending);
    assert(flushDeferredPress(pending, lastRelease, 2600));
    assert(!pending);

    pending = false;
    lastRelease = 0;
    counts = {};
    applyDecision(registerDeferredRelease(pending, lastRelease, 3000), counts);
    if (flushDeferredPress(pending, lastRelease, 3300)) ++counts.singles;
    applyDecision(registerDeferredRelease(pending, lastRelease, 3301), counts);
    assert(counts.singles == 1 && counts.doubles == 0 && pending);
}

void testRepeatedFastCtrl3AndCtrl4SinglesCollideWithDoubles() {
    for (uint8_t control : {3, 4}) {
        (void)control;
        const Counts counts = runReleaseSequence({1000, 1200, 1400, 1600});
        assert(counts.singles == 0);
        assert(counts.doubles == 2);
    }
}

void testTapTempoIntervals() {
    assert(tappedBpmFromInterval(1000) == 60.0f);
    assert(tappedBpmFromInterval(500) == 120.0f);
    assert(tappedBpmFromInterval(300) == 200.0f);
    assert(tappedBpmFromInterval(250) == 240.0f);
}

void testChordConsumptionClearsPendingSoloActions() {
    bool pending = false;
    unsigned long lastRelease = 0;
    registerDeferredRelease(pending, lastRelease, 1000);
    consumeDeferredPress(pending, lastRelease);
    assert(!pending && lastRelease == 0);
    assert(!flushDeferredPress(pending, lastRelease, 1400));
}

void testReleaseOrderingAroundChordConsumption() {
    bool pending = false;
    unsigned long lastRelease = 0;
    Counts counts;

    // Solo release first, then a chord forms: the pending solo must be consumed.
    applyDecision(registerDeferredRelease(pending, lastRelease, 1000), counts);
    consumeDeferredPress(pending, lastRelease);
    assert(!flushDeferredPress(pending, lastRelease, 1300));

    // Chord consumed before release: onRelease skips classification; the next
    // independent release starts a fresh pending single.
    consumeDeferredPress(pending, lastRelease);
    applyDecision(registerDeferredRelease(pending, lastRelease, 2000), counts);
    assert(flushDeferredPress(pending, lastRelease, 2300));
    assert(counts.doubles == 0);
}
} // namespace

int main() {
    testThresholdBoundaries();
    testRepeatedFastCtrl3AndCtrl4SinglesCollideWithDoubles();
    testTapTempoIntervals();
    testChordConsumptionClearsPendingSoloActions();
    testReleaseOrderingAroundChordConsumption();
    std::cout << "button gesture timing tests passed\n";
    return 0;
}
