#ifndef ARPEGGIATOR_H
#define ARPEGGIATOR_H

#include <Arduino.h>
#include "MIDITypes.h"

class MIDIHandler;
class ConfigManager;
class PotentiometerManager;

/**
 * @brief Simple arpeggiator for cycling through MIDI notes.
 */
class Arpeggiator {
public:
    /** Shapes for the arpeggio cycle. */
    enum Shape { UP, DOWN, UPDOWN, RANDOM };

    /** Construct an inactive arpeggiator. */
    Arpeggiator();

    /** Begin arpeggiating the given slot. */
    void start(uint8_t slotIdx);

    /** Stop any active arpeggio. */
    void stop();

    /** Check if the arpeggiator is running. */
    bool isActive() const;

    /** Index of the slot currently being arpeggiated. */
    uint8_t getSlot() const;

    /** Set the interval between steps in milliseconds. */
    void setLength(float ms);

    /** Choose the arpeggio shape. */
    void setShape(Shape s);

    /** Update state and send MIDI notes if needed. */
    void update(MIDIHandler& midi, ConfigManager& cfg, PotentiometerManager& pots);

private:
    bool          _active;     //!< Whether an arpeggio is running
    uint8_t       _slotIdx;    //!< Active slot index
    float         _intervalMs; //!< Step duration in milliseconds
    Shape         _shape;      //!< Current arpeggio shape
    unsigned long _lastStep;   //!< Timestamp of the last step
    uint8_t       _step;       //!< Current step position
};

#endif // ARPEGGIATOR_H
