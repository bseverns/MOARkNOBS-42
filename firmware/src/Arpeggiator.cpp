// Controls the simple arpeggiator used by the MN42.
// Reads slot settings from ConfigManager and triggers notes via MIDIHandler.
// Started from firmware_main.cpp and reacts to ButtonManager.

#include "Arpeggiator.h"
#include "MIDIHandler.h"
#include "ConfigManager.h"
#include "Utility.h"
#include "PotentiometerManager.h"
#include "PerlinNoise.h"

constexpr uint8_t MAX_STEPS  = 16;
// Longest span between notes, in MIDI clock ticks. Anything longer loses the groove.
constexpr uint8_t MAX_LENGTH = Arpeggiator::MAX_LENGTH;

// Handles per-slot arpeggiation. This component ties into ConfigManager to
// fetch slot settings, reads the most recent pot value from
// PotentiometerManager and issues MIDI messages through MIDIHandler. The
// update() routine is scheduled from the main firmware loop.

Arpeggiator::Arpeggiator()
    : _active(false), _slotIdx(0), _lengthTicks(12), _tickCounter(0),
      _shape(UP), _step(0), _patternLength(4), _baseNote(0),
      _baseNoteSrc(BaseNoteSource::Slot), _baseNoteCb(nullptr) {}

// Begin generating an arpeggio for the given slot. The slot index refers to the
// entry stored by ConfigManager and determines both MIDI type and channel.

void Arpeggiator::start(uint8_t slotIdx) {
    _slotIdx = slotIdx;
    _active = true;
    _tickCounter = 0;
    _step = 0;
}

// Stop arpeggiation immediately. update() will simply return once inactive.
void Arpeggiator::stop() {
    _active = false;
}

bool Arpeggiator::isActive() const { return _active; }

uint8_t Arpeggiator::getSlot() const { return _slotIdx; }

void Arpeggiator::setLength(uint8_t ticks) {
    _lengthTicks = constrain(ticks, 1, MAX_LENGTH);
}

void Arpeggiator::setShape(Shape s) { _shape = s; }

void Arpeggiator::setPatternLength(uint8_t steps) {
    steps = constrain(steps, 2, MAX_STEPS);
    _patternLength = steps;
}

void Arpeggiator::setBaseNoteSource(BaseNoteSource src) { _baseNoteSrc = src; }

void Arpeggiator::setBaseNote(uint8_t note) { _baseNote = note; }

void Arpeggiator::setBaseNoteCallback(std::function<uint8_t()> cb) {
    _baseNoteCb = cb;
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
        default: {
            float n = perlinNoise1D(static_cast<float>(step));
            int val = static_cast<int>((n * 0.5f + 0.5f) * patternLen);
            val = constrain(val, 0, patternLen - 1);
            return static_cast<int8_t>(val);
        }
    }
}

// Called frequently from the scheduler. When active, this checks the slot's
// settings and emits the next MIDI event via MIDIHandler.
void Arpeggiator::update(MIDIHandler& midi, ConfigManager& cfg, PotentiometerManager& pots) {
    if (!_active) return;

    // Ride the global MIDI clock. No tick, no note.
    if (!midi.isClockTick()) return;
    midi.clearClockTick(); // consume the pulse so we don't double-dip

    if (++_tickCounter < _lengthTicks) return; // not time yet
    _tickCounter = 0; // reset for the next hit

    MIDISlot& slot = cfg.getSlots()[_slotIdx];
    if (!slot.active) return;

    int8_t offset = noteOffset(_shape, _step, _patternLength);
    _step = (_step + 1) % _patternLength; // advance and wrap within the pattern

    uint8_t root = Utility::mapToMidiValue(pots.getLastValue(_slotIdx)) % 128;
    slot.arpNote = root; // keep last root around for anyone else who cares
    uint8_t potVal = root;

    switch (slot.type) {
        case MIDIMessageType::CC:
            midi.sendControlChange(slot.data1,
                                   constrain(potVal + offset, 0, 127),
                                   slot.midiChannel);
            break;
        case MIDIMessageType::Note: {
            uint8_t note = constrain(root + offset, 0, 127);
            midi.sendNoteOn(note, potVal, slot.midiChannel);
            // Clock-synced release: fire a note-off halfway to the next tick hit.
            unsigned long noteOffDelay = (hwConfig.midiTaskInterval * _lengthTicks) / 2;
            Utility::schedulerHigh.addTask([note, ch=slot.midiChannel, &midi]() {
                midi.sendNoteOff(note, 0, ch);
            }, noteOffDelay, false);
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
            midi.sendProgramChange(constrain(root + offset, 0, 127),
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
