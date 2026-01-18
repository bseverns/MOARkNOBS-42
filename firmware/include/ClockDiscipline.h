#ifndef CLOCK_DISCIPLINE_H
#define CLOCK_DISCIPLINE_H

#include <Arduino.h>
#include <cstdint>

class ClockDiscipline {
  public:
    ClockDiscipline();
    void reset();

    /** Feed the latest raw clock counter and running flag. */
    void observe(uint32_t tickCount, unsigned long timestampMs, bool running);

    /** Consume ticks that arrived since the last call. */
    uint32_t consumeTicks();

    /** Return true once a running state has just resumed (start/continue). */
    bool justResumed() const;

    /** Clear the resume flag once the caller has handled the resync. */
    void clearResumeFlag();

    /** Return true when a large tempo drift should trigger a reset. */
    bool driftDetected() const;

    /** Clear the drift flag after the caller has reset its state. */
    void clearDriftFlag();

    /** Estimated milliseconds per MIDI tick based on the recent stream. */
    float msPerTick() const;

  private:
    uint32_t lastTickCount_;
    unsigned long lastTickTimeMs_;
    float msPerTick_;
    bool running_;
    bool resumed_;
    bool drifted_;
    uint32_t pendingTicks_;
};

#endif // CLOCK_DISCIPLINE_H
