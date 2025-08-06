// Simple note arpeggiator for a selected slot.
// Uses MIDIHandler to send notes and gets settings from ConfigManager.
// Controlled from firmware_main.cpp and ButtonManager.

#ifndef ARPEGGIATOR_H
#define ARPEGGIATOR_H

#include <Arduino.h>
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
     * Set the time between note triggers.
     * @param ms Delay in milliseconds; shorter values make the riff blaze
     *           faster while longer ones chill it out.
     */
    void setLength(float ms);
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

    /** Call regularly to send notes when due. */
    void update(MIDIHandler& midi, ConfigManager& cfg, PotentiometerManager& pots);

private:
    bool          _active;
    uint8_t       _slotIdx;
    float         _intervalMs;
    Shape         _shape;
    unsigned long _lastStep;
    uint8_t       _step;
    uint8_t       _patternLength;
};

#endif // ARPEGGIATOR_H
