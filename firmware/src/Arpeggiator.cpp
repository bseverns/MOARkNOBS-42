#include "Arpeggiator.h"
#include "MIDIHandler.h"
#include "ConfigManager.h"
#include "Utility.h"

Arpeggiator::Arpeggiator()
    : _active(false), _slotIdx(0), _intervalMs(250), _shape(UP),
      _lastStep(0), _step(0) {}

void Arpeggiator::start(uint8_t slotIdx) {
    _slotIdx = slotIdx;
    _active = true;
    _lastStep = millis();
    _step = 0;
}

void Arpeggiator::stop() {
    _active = false;
}

bool Arpeggiator::isActive() const { return _active; }

uint8_t Arpeggiator::getSlot() const { return _slotIdx; }

void Arpeggiator::setLength(float ms) { _intervalMs = ms; }

void Arpeggiator::setShape(Shape s) { _shape = s; }

static int8_t noteOffset(Arpeggiator::Shape shape, uint8_t step) {
    static const int8_t up[]     = {0, 4, 7, 12};
    static const int8_t down[]   = {12, 7, 4, 0};
    static const int8_t updown[] = {0, 4, 7, 12, 7, 4};
    switch (shape) {
        case Arpeggiator::UP:      return up[step % 4];
        case Arpeggiator::DOWN:    return down[step % 4];
        case Arpeggiator::UPDOWN:  return updown[step % 6];
        case Arpeggiator::RANDOM:
        default: {
            const int8_t choices[] = {0,4,7,12};
            return choices[random(0,4)];
        }
    }
}

void Arpeggiator::update(MIDIHandler& midi, ConfigManager& cfg) {
    if (!_active) return;
    unsigned long now = millis();
    if (now - _lastStep < _intervalMs) return;
    _lastStep = now;

    const MIDISlot& slot = cfg.getSlots()[_slotIdx];
    if (slot.type != MIDIMessageType::Note) return;

    uint8_t base = slot.data1;
    int8_t offset = noteOffset(_shape, _step++);
    uint8_t note = constrain(base + offset, 0, 127);
    midi.sendNoteOn(note, 100, slot.midiChannel);
    Utility::schedulerHigh.addTask([note, ch=slot.midiChannel, &midi](){
        midi.sendNoteOff(note, 0, ch);
    }, (unsigned long)(_intervalMs / 2), false);
}
