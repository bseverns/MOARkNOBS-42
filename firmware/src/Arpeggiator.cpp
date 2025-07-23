#include "Arpeggiator.h"
#include "MIDIHandler.h"
#include "ConfigManager.h"
#include "Utility.h"
#include "PotentiometerManager.h"

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

/**
 * Translate a step index and shape into a semitone offset.
 *
 * The tables below correspond to a simple major arpeggio:
 *   - `UP`     : 0, +4, +7, +12  (root, 3rd, 5th, octave)
 *   - `DOWN`   : +12, +7, +4, 0  (reverse order)
 *   - `UPDOWN` : 0, +4, +7, +12, +7, +4
 *   - `RANDOM` : choose any of the above offsets randomly
 */
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

void Arpeggiator::update(MIDIHandler& midi, ConfigManager& cfg, PotentiometerManager& pots) {
    if (!_active) return;

    // Check whether our step interval has expired.  The interval is
    // configured in milliseconds via setLength() and stored in
    // `_intervalMs`.  A simple millis()-based timer is sufficient
    // because the scheduler calls this function regularly.
    unsigned long now = millis();
    if (now - _lastStep < _intervalMs) return;
    _lastStep = now;

    // Retrieve the MIDI slot to arpeggiate.  Slots hold a base value
    // (note number, CC number, etc.) and the message type to send.
    const MIDISlot& slot = cfg.getSlots()[_slotIdx];
    if (!slot.active) return; // skip if the slot is disabled

    // Determine the note/CC offset for this step and read the last
    // pot value associated with the slot.  The pot provides velocity
    // or CC values while the offset adjusts pitch-related parameters.
    int8_t offset = noteOffset(_shape, _step++);
    uint8_t potVal = Utility::mapToMidiValue(pots.getLastValue(_slotIdx));

    // Dispatch the appropriate MIDI message.  The slot determines
    // whether we send a CC, Note, Program Change, etc.  Offsets are
    // applied directly to the stored value.
    switch (slot.type) {
        case MIDIMessageType::CC:
            midi.sendControlChange(slot.data1,
                                   constrain(potVal + offset, 0, 127),
                                   slot.midiChannel);
            break;
        case MIDIMessageType::Note: {
            uint8_t note = constrain(slot.data1 + offset, 0, 127);
            midi.sendNoteOn(note, potVal, slot.midiChannel);
            // Schedule the note-off at half the step length so each
            // note remains distinct without bleeding into the next.
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
