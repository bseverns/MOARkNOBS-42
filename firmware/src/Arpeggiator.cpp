// Controls the simple arpeggiator used by the MN42.
// Reads slot settings from ConfigManager and triggers notes via MIDIHandler.
// Started from firmware_main.cpp and reacts to ButtonManager.

#include "Arpeggiator.h"
#include "MIDIHandler.h"
#include "ConfigManager.h"
#include "Utility.h"
#include "PotentiometerManager.h"

constexpr uint8_t MAX_STEPS = 16;

// Handles per-slot arpeggiation. This component ties into ConfigManager to
// fetch slot settings, reads the most recent pot value from
// PotentiometerManager and issues MIDI messages through MIDIHandler. The
// update() routine is scheduled from the main firmware loop.

Arpeggiator::Arpeggiator()
    : _active(false), _slotIdx(0), _intervalMs(250), _shape(UP),
      _lastStep(0), _step(0), _patternLength(4) {}

// Begin generating an arpeggio for the given slot. The slot index refers to the
// entry stored by ConfigManager and determines both MIDI type and channel.

void Arpeggiator::start(uint8_t slotIdx) {
    _slotIdx = slotIdx;
    _active = true;
    _lastStep = millis();
    _step = 0;
}

// Stop arpeggiation immediately. update() will simply return once inactive.
void Arpeggiator::stop() {
    _active = false;
}

bool Arpeggiator::isActive() const { return _active; }

uint8_t Arpeggiator::getSlot() const { return _slotIdx; }

void Arpeggiator::setLength(float ms) { _intervalMs = ms; }

void Arpeggiator::setShape(Shape s) { _shape = s; }

void Arpeggiator::setPatternLength(uint8_t steps) {
    steps = constrain(steps, 2, MAX_STEPS);
    _patternLength = steps;
}

static int8_t noteOffset(Arpeggiator::Shape shape, uint8_t step, uint8_t patternLen) {
    // Semitone offsets now derive from simple math rather than pre-baked tables.
    // This lets pattern length drive the range while shapes dictate direction.
    uint8_t pos = step % patternLen;
    switch (shape) {
        case Arpeggiator::UP:
            return pos;                       // climb from root
        case Arpeggiator::DOWN:
            return patternLen - 1 - pos;      // descend back to root
        case Arpeggiator::UPDOWN:
            // walk up then mirror back down over the pattern range
            return (step < patternLen ? pos : (patternLen - 1 - pos));
        case Arpeggiator::RANDOM:
        default:
            return static_cast<int8_t>(random(0, patternLen));
    }
}

// Called frequently from the scheduler. When active, this checks the slot's
// settings and emits the next MIDI event via MIDIHandler.
void Arpeggiator::update(MIDIHandler& midi, ConfigManager& cfg, PotentiometerManager& pots) {
    if (!_active) return;
    unsigned long now = millis();
    if (now - _lastStep < _intervalMs) return;
    _lastStep = now;

    const MIDISlot& slot = cfg.getSlots()[_slotIdx];
    if (!slot.active) return;

    int8_t offset = noteOffset(_shape, _step++, _patternLength);
    uint8_t potVal = Utility::mapToMidiValue(pots.getLastValue(_slotIdx));

    switch (slot.type) {
        case MIDIMessageType::CC:
            midi.sendControlChange(slot.data1,
                                   constrain(potVal + offset, 0, 127),
                                   slot.midiChannel);
            break;
        case MIDIMessageType::Note: {
            uint8_t note = constrain(slot.data1 + offset, 0, 127);
            midi.sendNoteOn(note, potVal, slot.midiChannel);
            // Schedule a note-off at half the interval so each note gets a
            // quick release without blocking the main loop.
            Utility::schedulerHigh.addTask([note, ch=slot.midiChannel, &midi](){
                midi.sendNoteOff(note, 0, ch);
            }, (unsigned long)(_intervalMs / 2), false);
            break;
        }
        case MIDIMessageType::PitchBend: {
            int raw = pots.getLastValue(_slotIdx);
            int16_t bend = map(raw, 0, 1023, -8192, 8191) + offset * 128;
            bend = constrain(bend, -8192, 8191);
            midi.sendPitchBend(bend, slot.midiChannel);
            break;
        }
        case MIDIMessageType::ProgramChange:
            midi.sendProgramChange(constrain(slot.data1 + offset, 0, 127),
                                   slot.midiChannel);
            break;
        case MIDIMessageType::Aftertouch:
            midi.sendAftertouch(constrain(potVal + offset, 0, 127),
                                slot.midiChannel);
            break;
        default:
            break;
    }
}
