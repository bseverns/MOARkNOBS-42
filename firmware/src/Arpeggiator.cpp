// The arpeggiator is our looping storyteller: take one slot, advance through a
// pattern, spit MIDI on the beat. This file doubles as a workshop on timing,
// state machines, and data ownership. We lean on ConfigManager for slot state,
// PotentiometerManager for live knobs, and MIDIHandler for the actual note
// shouts. Follow the breadcrumbs—every pointer, reference, and struct handoff
// is annotated so students can see exactly who owns what and why it matters.

#include "Arpeggiator.h"
#include "MIDIHandler.h"
#include "ConfigManager.h"
#include "Utility.h"
#include "PotentiometerManager.h"
#include "PerlinNoise.h"
#include "Globals.h"

constexpr uint8_t MAX_STEPS = 16;
// Longest span between notes, in MIDI clock ticks. Anything longer loses the groove.
constexpr uint8_t MAX_LENGTH = Arpeggiator::MAX_LENGTH;

// Handles per-slot arpeggiation. The constructor only seeds defaults; real work
// happens once start() is called from firmware_main.cpp. We store primitive
// types instead of dynamic allocations so there’s nothing sneaky to clean up
// when the arp idles.

Arpeggiator::Arpeggiator()
    : _active(false), _slotIdx(0), _lengthTicks(12), _tickCounter(0), _shape(UP), _step(0),
      _patternLength(4), _baseNote(0), _baseNoteSrc(BaseNoteSource::Pot), _baseNoteIsSet(false),
      _baseNoteCb(nullptr), _lastClockTickCount(0), _clockSynced(false) {}

// Begin generating an arpeggio for the given slot. The slot index refers to the
// entry stored by ConfigManager and determines both MIDI type and channel.

void Arpeggiator::start(uint8_t slotIdx) {
    _slotIdx = slotIdx;
    _active = true;
    _tickCounter = 0;
    _step = 0;
    _clockSynced = false;
}

// Stop arpeggiation immediately. update() will simply return once inactive.
void Arpeggiator::stop() { _active = false; }

bool Arpeggiator::isActive() const { return _active; }

uint8_t Arpeggiator::getSlot() const { return _slotIdx; }

void Arpeggiator::setLength(uint8_t ticks) { _lengthTicks = constrain(ticks, 1, MAX_LENGTH); }

void Arpeggiator::setShape(Shape s) { _shape = s; }

void Arpeggiator::setPatternLength(uint8_t steps) {
    steps = constrain(steps, 2, MAX_STEPS);
    _patternLength = steps;
}

void Arpeggiator::setBaseNoteSource(BaseNoteSource src) { _baseNoteSrc = src; }

void Arpeggiator::setBaseNote(uint8_t note) {
    _baseNote = note;
    _baseNoteIsSet = true;
}

void Arpeggiator::setBaseNoteCallback(std::function<uint8_t()> cb) { _baseNoteCb = cb; }

namespace {
float jitterRateFromSmoothness(float smoothness) {
    float clamped = constrain(smoothness, 0.0f, 1.0f);
    return Utility::scale(clamped, 0.0f, 1.0f, 2.0f, 0.05f);
}
} // namespace

static int8_t noteOffset(Arpeggiator::Shape shape, uint8_t step, uint8_t patternLen) {
    // Semitone offsets now derive from simple math rather than pre-baked tables.
    // This lets pattern length drive the range while shapes dictate direction.
    // The helper is `static` on purpose: no free-floating functions in the
    // global namespace and the compiler can inline it if it feels spicy.
    uint8_t pos = step % patternLen;
    switch (shape) {
    case Arpeggiator::UP:
        return pos; // climb from root
    case Arpeggiator::DOWN:
        return patternLen - 1 - pos; // descend back to root
    case Arpeggiator::UPDOWN:
        // walk up then mirror back down over the pattern range
        return (step < patternLen ? pos : (patternLen - 1 - pos));
    case Arpeggiator::RANDOM:
    default: {
        float depth = constrain(g_jitterSettings.depth, 0.0f, 1.0f);
        float rate = jitterRateFromSmoothness(g_jitterSettings.smoothness);
        float n = perlinNoise1D(static_cast<float>(step) * rate);
        float jitter = (n * depth * 0.5f) + 0.5f;
        int val = static_cast<int>(jitter * patternLen);
        val = constrain(val, 0, patternLen - 1);
        return static_cast<int8_t>(val);
    }
    }
}

// Called frequently from the scheduler. When active, this checks the slot's
// settings and emits the next MIDI event via MIDIHandler.
//
// The trio of parameters spells out the data flow:
//  * `midi`  → outbound messages + access to the MIDI clock tick counter.
//  * `cfg`   → slot configuration, persisted in EEPROM.
//  * `pots`  → cached ADC readings so we don’t block on fresh analog reads.
// Each is passed by reference so we mutate the shared, long-lived singletons
// instead of cloning state each frame.
void Arpeggiator::update(MIDIHandler &midi, ConfigManager &cfg, PotentiometerManager &pots) {
    if (!_active)
        return;

    uint32_t tickCount = midi.clockTickCount();
    if (!_clockSynced) {
        _lastClockTickCount = tickCount;
        _clockSynced = true;
        return; // latch to the current beat and wait for the next pulse
    }

    uint32_t elapsed = tickCount - _lastClockTickCount;
    if (elapsed == 0)
        return; // nothing new from the clock
    _lastClockTickCount = tickCount;

    uint16_t ticks = static_cast<uint16_t>(_tickCounter) + static_cast<uint16_t>(elapsed);
    uint16_t events = ticks / _lengthTicks;
    _tickCounter = static_cast<uint8_t>(ticks % _lengthTicks);
    if (events == 0)
        return;

    MIDISlot &slot = cfg.getSlot(_slotIdx);
    if (!slot.active)
        return;

    auto clampMidi = [](int value) { return static_cast<uint8_t>(constrain(value, 0, 127)); };

    uint8_t root = 0;
    bool haveRoot = false;
    bool usedPot = false;
    int potRaw = 0;

    auto readPot = [&]() {
        potRaw = pots.getLastValue(_slotIdx);
        if (potRaw < 0)
            potRaw = 0;
        root = Utility::mapToMidiValue(potRaw) % 128;
        usedPot = true;
        haveRoot = true;
    };

    switch (_baseNoteSrc) {
    case BaseNoteSource::Pot:
        readPot();
        break;
    case BaseNoteSource::Slot:
        root = clampMidi(slot.arpNote);
        haveRoot = true;
        break;
    case BaseNoteSource::External:
        if (_baseNoteCb) {
            root = clampMidi(_baseNoteCb());
            haveRoot = true;
        } else if (_baseNoteIsSet) {
            root = clampMidi(_baseNote);
            haveRoot = true;
        }
        break;
    default:
        break;
    }

    if (!haveRoot)
        readPot();

    slot.arpNote = root; // keep last root around for anyone else who cares
    uint8_t potVal = root;
    unsigned long noteOffDelay = (hwConfig.midiTaskInterval * _lengthTicks) / 2;

    for (uint16_t i = 0; i < events; ++i) {
        int8_t offset = noteOffset(_shape, _step, _patternLength);
        _step = (_step + 1) % _patternLength; // advance and wrap within the pattern

        switch (slot.type) {
        case MIDIMessageType::CC:
            midi.sendControlChange(slot.data1, constrain(potVal + offset, 0, 127),
                                   slot.midiChannel);
            break;
        case MIDIMessageType::Note: {
            uint8_t note = constrain(root + offset, 0, 127);
            midi.sendNoteOn(note, potVal, slot.midiChannel);
            // Clock-synced release: fire a note-off halfway to the next tick hit.
            Utility::schedulerHigh.addTask(
                [note, ch = slot.midiChannel, &midi]() { midi.sendNoteOff(note, 0, ch); },
                noteOffDelay, false);
            break;
        }
        case MIDIMessageType::PitchBend: {
            int raw = usedPot ? potRaw : Utility::mapToRange(root, 0, 127, 0, 1023);
            int16_t bend = map(raw, 0, 1023, -8192, 8191) + offset * 128;
            bend = constrain(bend, -8192, 8191);
            midi.sendPitchBend(bend, slot.midiChannel);
            break;
        }
        case MIDIMessageType::ProgramChange:
            midi.sendProgramChange(constrain(root + offset, 0, 127), slot.midiChannel);
            break;
        case MIDIMessageType::Aftertouch:
            midi.sendAftertouch(constrain(potVal + offset, 0, 127), slot.midiChannel);
            break;
        case MIDIMessageType::ModWheel:
            midi.sendModWheel(constrain(potVal + offset, 0, 127), slot.midiChannel);
            break;
        default:
            break;
        }
    }
}
