#ifndef ARPEGGIATOR_H
#define ARPEGGIATOR_H

#include <Arduino.h>
#include "MIDITypes.h"

class MIDIHandler;
class ConfigManager;
class PotentiometerManager;

class Arpeggiator {
public:
    enum Shape { UP, DOWN, UPDOWN, RANDOM };

    Arpeggiator();

    void start(uint8_t slotIdx);
    void stop();
    bool isActive() const;
    uint8_t getSlot() const;

    void setLength(float ms);
    void setShape(Shape s);

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
