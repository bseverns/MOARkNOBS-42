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

    /** Begin playing notes for the given slot index. */
    void start(uint8_t slotIdx);
    /** Stop sending arpeggiated notes. */
    void stop();
    /** True while the arpeggiator is actively stepping. */
    bool isActive() const;
    /** Return the slot currently being arpeggiated. */
    uint8_t getSlot() const;

    /** Set the delay between note steps in milliseconds. */
    void setLength(float ms);
    /** Choose the pattern used to step through notes. */
    void setShape(Shape s);

    /** Call regularly to send notes when due. */
    void update(MIDIHandler& midi, ConfigManager& cfg, PotentiometerManager& pots);

private:
    bool          _active;
    uint8_t       _slotIdx;
    float         _intervalMs;
    Shape         _shape;
    unsigned long _lastStep;
    uint8_t       _step;
};

#endif // ARPEGGIATOR_H
