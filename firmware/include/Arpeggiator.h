// Simple note arpeggiator for a selected slot.
// Uses MIDIHandler to send notes and gets settings from ConfigManager.
// Controlled from firmware_main.cpp and ButtonManager.

#ifndef ARPEGGIATOR_H
#define ARPEGGIATOR_H

#include <Arduino.h>
#include <functional>
#include "MIDITypes.h"

class MIDIHandler;
class ConfigManager;
class PotentiometerManager;

/**
 * Simple timed arpeggiator that plays notes from a configured slot.
 */
class Arpeggiator {
  public:
    enum Shape { UP, DOWN, UPDOWN, RANDOM };
    enum class BaseNoteSource { Slot, External };

    /** Max ticks allowed between note hits so the riff never drifts past a beat. */
    static constexpr uint8_t MAX_LENGTH = 24;

    /** Construct a stopped arpeggiator with default settings. */
    Arpeggiator();

    /**
     * Kick off the arpeggiator on a slot.
     * @param slotIdx Index of the slot in ConfigManager whose notes should be
     *                ripped through. Once called, the arpeggiator will march
     *                through that slot's pitches until `stop()` is invoked.
     */
    void start(uint8_t slotIdx);
    /**
     * Slam the brakes and silence any further steps.
     * Calling this halts the groove immediately and prevents more MIDI events
     * from being triggered.
     */
    void stop();
    /** True while the arpeggiator is actively stepping. */
    bool isActive() const;
    /** Return the slot currently being arpeggiated. */
    uint8_t getSlot() const;

    /**
     * Set the gap between note triggers in global MIDI clock ticks.
     * @param ticks Number of 24 PPQN ticks to wait; clamped so the beat stays tight.
     */
    void setLength(uint8_t ticks);
    /**
     * Choose how the offsets are ordered.
     * @param s Pattern of note movement—UP, DOWN, UPDOWN or RANDOM—which
     *          changes the melodic contour of the arpeggio.
     */
    void setShape(Shape s);
    /**
     * Define how many steps the riff walks through before looping.
     * @param steps Number of notes in the pattern; clamped to a safe range
     *              so it never trips over itself.
     */
    void setPatternLength(uint8_t steps);

    /**
     * Pick where the root note comes from.
     * `Slot` grabs the slot's stored arp note; `External` calls a user hook.
     */
    void setBaseNoteSource(BaseNoteSource src);
    /**
     * Directly poke a new base note (0-127) when using `External` sourcing.
     */
    void setBaseNote(uint8_t note);
    /**
     * Provide a callback that coughs up the current base note on demand.
     */
    void setBaseNoteCallback(std::function<uint8_t()> cb);

    /**
     * Call every loop; notes only fire on MIDI clock ticks.
     * Keeps the groove glued to the global tempo.
     */
    void update(MIDIHandler &midi, ConfigManager &cfg, PotentiometerManager &pots);

  private:
    bool _active;
    uint8_t _slotIdx;
    uint8_t _lengthTicks;
    uint8_t _tickCounter;
    Shape _shape;
    uint8_t _step;
    uint8_t _patternLength;
    uint8_t _baseNote;                    //!< Root note for the pattern
    BaseNoteSource _baseNoteSrc;          //!< Who owns the root
    std::function<uint8_t()> _baseNoteCb; //!< Optional external hook for fresh roots
};

#endif // ARPEGGIATOR_H
