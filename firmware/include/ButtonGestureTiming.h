#ifndef BUTTON_GESTURE_TIMING_H
#define BUTTON_GESTURE_TIMING_H

inline constexpr unsigned long DOUBLE_PRESS_DELAY = 300;

struct DeferredPressDecision {
    bool fireSingle = false;
    bool fireDouble = false;
};

// Resolve one release for controls whose single action waits for an exclusive
// double-press window. Exact-threshold releases commit the older single and arm
// the new release; only intervals strictly below the threshold are doubles.
inline DeferredPressDecision registerDeferredRelease(bool &pending,
                                                      unsigned long &lastRelease,
                                                      unsigned long releasedAt) {
    if (pending && (releasedAt - lastRelease) < DOUBLE_PRESS_DELAY) {
        pending = false;
        lastRelease = 0;
        return {false, true};
    }

    const bool firePreviousSingle = pending;
    pending = true;
    lastRelease = releasedAt;
    return {firePreviousSingle, false};
}

inline bool flushDeferredPress(bool &pending,
                               unsigned long &lastRelease,
                               unsigned long currentTime) {
    if (!pending || (currentTime - lastRelease) < DOUBLE_PRESS_DELAY) return false;
    pending = false;
    lastRelease = 0;
    return true;
}

inline void consumeDeferredPress(bool &pending, unsigned long &lastRelease) {
    pending = false;
    lastRelease = 0;
}

inline float tappedBpmFromInterval(unsigned long intervalMs) {
    return intervalMs == 0 ? 0.0f : 60000.0f / static_cast<float>(intervalMs);
}

#endif // BUTTON_GESTURE_TIMING_H
