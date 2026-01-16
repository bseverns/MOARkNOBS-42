#pragma once

#include <cstdint>

class MIDIHandler;

/**
 * Supported clock divisions/multipliers for sync mode.
 * Ratios assume a 24 PPQN MIDI clock.
 */
enum class LFOSyncRatio : uint8_t {
    Div1 = 0, //!< 1 bar per cycle (24 ticks per beat * 4 beats)
    Div2,     //!< 1/2 rate
    Div4,     //!< 1/4 rate
    Div8,     //!< 1/8 rate
    Div16,    //!< 1/16 rate
    Div32,    //!< 1/32 rate
    Mul2,     //!< 2x rate
    Mul4      //!< 4x rate
};

/**
 * Adapter that translates MIDI clock ticks into per-LFO phase advances.
 * It mirrors the handler's tick count so each update only advances once.
 */
class LFOClock {
  public:
    /** Attach to the global MIDI handler for tick reads. */
    void attach(MIDIHandler *midi);
    /** Reset the internal tick snapshot so sync restarts clean. */
    void reset();
    /** Consume tick delta since the last call. */
    uint32_t consumeTickDelta();

    /** Return the tick span for a full LFO cycle at the given ratio. */
    static uint32_t ticksPerCycle(LFOSyncRatio ratio);

  private:
    MIDIHandler *midi_ = nullptr; //!< Tick source (not owned)
    uint32_t lastTickCount_ = 0;  //!< Last tick count snapshot
};
