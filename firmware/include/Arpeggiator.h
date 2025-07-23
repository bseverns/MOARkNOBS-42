#ifndef ARPEGGIATOR_H
#define ARPEGGIATOR_H

#include <Arduino.h>
#include "MIDITypes.h"

class MIDIHandler;
class ConfigManager;
class PotentiometerManager;

/**
 * @brief Simple step-based arpeggiator used by the firmware.
 *
 * The arpeggiator repeatedly triggers the currently selected MIDI slot.
 * Each step occurs every `_intervalMs` milliseconds and applies a note
 * offset based on the chosen `Shape` pattern.
 */
class Arpeggiator {
public:
    /**
     * Patterns describing how note offsets are generated.  The offsets are
     * defined in semitones relative to the slot's base note/CC value.
     */
    enum Shape {
        UP,     //!< 0, +4, +7, +12 (ascending)
        DOWN,   //!< +12, +7, +4, 0 (descending)
        UPDOWN, //!< 0, +4, +7, +12, +7, +4
        RANDOM  //!< Randomly chooses one of the above offsets each step
    };

    /** Create an idle arpeggiator instance. */
    Arpeggiator();

    /** Begin arpeggiating the given MIDI slot index. */
    void start(uint8_t slotIdx);
    /** Halt any running arpeggio. */
    void stop();
    /** Query whether the arpeggiator is currently running. */
    bool isActive() const;
    /** Return the slot index being arpeggiated. */
    uint8_t getSlot() const;

    /** Set the step interval in milliseconds. */
    void setLength(float ms);
    /** Choose the pattern used for note offsets. */
    void setShape(Shape s);

    /**
     * @brief Send the next step if the interval has elapsed.
     *
     * The method reads the active `MIDISlot` from `ConfigManager`,
     * applies the current shape to obtain a semitone offset and then
     * dispatches the appropriate MIDI message through `MIDIHandler`.
     * The slot's type (CC, Note, etc.) determines which message is sent.
     */
    void update(MIDIHandler& midi, ConfigManager& cfg, PotentiometerManager& pots);

private:
    bool          _active;      //!< True while the arpeggiator is running
    uint8_t       _slotIdx;     //!< Index of the MIDI slot being triggered
    float         _intervalMs;  //!< Delay between steps in milliseconds
    Shape         _shape;       //!< Current offset pattern
    unsigned long _lastStep;    //!< Timestamp of the previous step
    uint8_t       _step;        //!< Running step counter
};

#endif // ARPEGGIATOR_H
