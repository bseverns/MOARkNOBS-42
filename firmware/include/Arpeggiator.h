// Simple note arpeggiator for a selected slot.
// Uses MIDIHandler to send notes and gets settings from ConfigManager.
// Controlled from firmware_main.cpp and ButtonManager.

#ifndef ARPEGGIATOR_H
#define ARPEGGIATOR_H

#include <Arduino.h>
#include <functional>
#include "ClockDiscipline.h"
#include "MIDITypes.h"

class MIDIHandler;
class ConfigManager;
class PotentiometerManager;

/**
 * Simple timed arpeggiator that plays notes from a configured slot.
 */
class Arpeggiator {
  public:
    enum Shape {
        UP,       //!< Ascending semitone steps
        DOWN,     //!< Descending semitone steps
        UPDOWN,   //!< Up then back down without repeating endpoints
        RANDOM,   //!< Perlin-noise driven random offsets
        DRUNK,    //!< Random walk ("drunk" step) offsets
        EUCLIDEAN //!< Euclidean-lite hit/rest pattern
    };
    enum class BaseNoteSource { Pot, Slot, External };

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
    /** Return the current step length in MIDI ticks. */
    uint8_t getLength() const { return _lengthTicks; }
    /**
     * Choose how the offsets are ordered.
     * @param s Pattern of note movement—UP, DOWN, UPDOWN or RANDOM—which
     *          changes the melodic contour of the arpeggio.
     */
    void setShape(Shape s);
    /** Return the current shape. */
    Shape getShape() const { return _shape; }
    /**
     * Define how many steps the riff walks through before looping.
     * @param steps Number of notes in the pattern; clamped to a safe range
     *              so it never trips over itself.
     */
    void setPatternLength(uint8_t steps);
    /** Return the current pattern length in steps. */
    uint8_t getPatternLength() const { return _patternLength; }
    /**
     * Set swing amount for off-beat steps, in percent of step duration.
     */
    /** Set swing amount for off-beat steps, in percent of step duration. */
    void setSwingPercent(float percent);
    /** Return the current swing percent. */
    float getSwingPercent() const { return _swingPercent; }
    /**
     * Set note gate length as a percent of the step duration.
     */
    /** Set note gate length as a percent of the step duration. */
    void setGatePercent(float percent);
    /** Return the current gate percent. */
    float getGatePercent() const { return _gatePercent; }
    /**
     * Set how many extra octaves the arp spans (0 = root octave only).
     */
    /** Set how many extra octaves the arp spans (0 = root octave only). */
    void setOctaveRange(uint8_t octaves);
    /** Return the current octave range. */
    uint8_t getOctaveRange() const { return _octaveRange; }

    /**
     * Pick where the root note comes from.
     * `Pot` reads the slot knob, `Slot` trusts saved memory, `External` calls a user hook.
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
    /** Compute semitone offset for the given step, honoring shape rules. */
    int8_t computeOffset(uint8_t stepIndex, uint8_t totalSteps, bool &stepEnabled);
    /** Return the total step count for the active shape. */
    uint8_t stepCountForShape(uint8_t totalSteps) const;
    /** Advance and return the deterministic RNG state. */
    uint32_t nextRng();
    /** Compute the next offset in the "drunk" random walk. */
    int8_t nextDrunkOffset(uint8_t totalSteps);

    bool _active;
    uint8_t _slotIdx;
    uint8_t _lengthTicks;
    uint8_t _tickCounter;
    Shape _shape;
    uint8_t _step;
    uint8_t _patternLength;
    float _swingPercent;                  //!< Swing in percent of step duration
    float _gatePercent;                   //!< Gate in percent of step duration
    uint8_t _octaveRange;                 //!< Number of extra octaves above the root
    uint8_t _baseNote;                    //!< Root note for the pattern
    BaseNoteSource _baseNoteSrc;          //!< Who owns the root
    bool _baseNoteIsSet;                  //!< True once setBaseNote() has been called
    std::function<uint8_t()> _baseNoteCb; //!< Optional external hook for fresh roots
    ClockDiscipline _clock;               //!< Tracks PPQN-derived ticks and drift
    uint32_t _rngState;                   //!< Deterministic RNG state
    int8_t _drunkPosition;                //!< Random walk position in steps
};

#endif // ARPEGGIATOR_H
