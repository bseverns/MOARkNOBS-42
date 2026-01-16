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
#include <cmath>

constexpr uint8_t MAX_STEPS = 16;
// Longest span between notes, in MIDI clock ticks. Anything longer loses the groove.
constexpr uint8_t MAX_LENGTH = Arpeggiator::MAX_LENGTH;

// Handles per-slot arpeggiation. The constructor only seeds defaults; real work
// happens once start() is called from firmware_main.cpp. We store primitive
// types instead of dynamic allocations so there’s nothing sneaky to clean up
// when the arp idles.

Arpeggiator::Arpeggiator()
    : _active(false), _slotIdx(0), _lengthTicks(12), _tickCounter(0), _shape(UP), _step(0),
      _patternLength(4), _swingPercent(0.0f), _gatePercent(50.0f), _octaveRange(0), _baseNote(0),
      _baseNoteSrc(BaseNoteSource::Pot), _baseNoteIsSet(false), _baseNoteCb(nullptr),
      _lastClockTickCount(0), _clockSynced(false), _lastTickTimeMs(0), _msPerTick(0.0f),
      _rngState(0x12345678u), _drunkPosition(0) {}

// Begin generating an arpeggio for the given slot. The slot index refers to the
// entry stored by ConfigManager and determines both MIDI type and channel.

void Arpeggiator::start(uint8_t slotIdx) {
    _slotIdx = slotIdx;
    _active = true;
    _tickCounter = 0;
    _step = 0;
    _clockSynced = false;
    _lastTickTimeMs = 0;
    _msPerTick = 0.0f;
    // Seed per-slot RNG so DRUNK mode stays deterministic.
    _rngState = 0x12345678u ^ static_cast<uint32_t>(slotIdx);
    _drunkPosition = 0;
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

void Arpeggiator::setSwingPercent(float percent) {
    // Cap swing to a musically sane range.
    _swingPercent = constrain(percent, 0.0f, 80.0f);
}

void Arpeggiator::setGatePercent(float percent) {
    // Gate length must leave a little gap so notes can release.
    _gatePercent = constrain(percent, 5.0f, 100.0f);
}

void Arpeggiator::setOctaveRange(uint8_t octaves) { _octaveRange = constrain(octaves, 0, 3); }

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

// Deterministic RNG used for DRUNK steps.
uint32_t Arpeggiator::nextRng() {
    _rngState = (_rngState * 1664525u) + 1013904223u;
    return _rngState;
}

// Random walk that nudges the step position by +/-1 each hit.
int8_t Arpeggiator::nextDrunkOffset(uint8_t totalSteps) {
    if (totalSteps == 0) {
        return 0;
    }
    uint8_t pos = static_cast<uint8_t>(_drunkPosition);
    uint32_t r = nextRng();
    int delta = (r & 0x1u) ? 1 : -1;
    int nextPos = static_cast<int>(pos) + delta;
    if (nextPos < 0) {
        nextPos = 1;
    } else if (nextPos >= static_cast<int>(totalSteps)) {
        nextPos = static_cast<int>(totalSteps) - 2;
    }
    if (nextPos < 0) {
        nextPos = 0;
    }
    _drunkPosition = static_cast<int8_t>(constrain(nextPos, 0, totalSteps - 1));
    return _drunkPosition;
}

// UPDOWN walks up then back down, so we extend the step count.
uint8_t Arpeggiator::stepCountForShape(uint8_t totalSteps) const {
    if (totalSteps <= 1) {
        return 1;
    }
    if (_shape == UPDOWN) {
        return static_cast<uint8_t>(totalSteps * 2 - 2);
    }
    return totalSteps;
}

// Compute the semitone offset for a given step and shape.
int8_t Arpeggiator::computeOffset(uint8_t stepIndex, uint8_t totalSteps, bool &stepEnabled) {
    stepEnabled = true;
    if (totalSteps == 0) {
        stepEnabled = false;
        return 0;
    }

    uint8_t pos = stepIndex % totalSteps;
    // Shape-specific step logic.
    switch (_shape) {
    case Arpeggiator::UP:
        break;
    case Arpeggiator::DOWN:
        pos = totalSteps - 1 - pos;
        break;
    case Arpeggiator::UPDOWN: {
        uint8_t maxStep = stepCountForShape(totalSteps);
        if (stepIndex >= totalSteps) {
            pos = static_cast<uint8_t>(maxStep - stepIndex);
        }
        break;
    }
    case Arpeggiator::DRUNK:
        pos = static_cast<uint8_t>(nextDrunkOffset(totalSteps));
        break;
    case Arpeggiator::EUCLIDEAN: {
        // Euclidean-lite hit/rest: fire on evenly distributed steps.
        uint8_t pulses = static_cast<uint8_t>(totalSteps / 2);
        if (pulses == 0) {
            pulses = 1;
        }
        bool hit = ((stepIndex * pulses) % totalSteps) < pulses;
        if (!hit) {
            stepEnabled = false;
            return 0;
        }
        break;
    }
    case Arpeggiator::RANDOM:
    default: {
        float depth = constrain(g_jitterSettings.depth, 0.0f, 1.0f);
        float rate = jitterRateFromSmoothness(g_jitterSettings.smoothness);
        float n = perlinNoise1D(static_cast<float>(stepIndex) * rate);
        float jitter = (n * depth * 0.5f) + 0.5f;
        int val = static_cast<int>(jitter * totalSteps);
        val = constrain(val, 0, totalSteps - 1);
        pos = static_cast<uint8_t>(val);
        break;
    }
    }

    // Map the step index into semitone + octave offsets.
    uint8_t octave = pos / _patternLength;
    uint8_t semitone = pos % _patternLength;
    int offset = static_cast<int>(semitone) + static_cast<int>(octave) * 12;
    return static_cast<int8_t>(offset);
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
    unsigned long nowMs = now();
    if (!_clockSynced) {
        _lastClockTickCount = tickCount;
        _lastTickTimeMs = nowMs;
        _clockSynced = true;
        return; // latch to the current beat and wait for the next pulse
    }

    uint32_t elapsed = tickCount - _lastClockTickCount;
    if (elapsed == 0)
        return; // nothing new from the clock

    // Estimate ms per tick to support swing/gate timing in milliseconds.
    if (_lastTickTimeMs > 0 && nowMs > _lastTickTimeMs) {
        float deltaMs = static_cast<float>(nowMs - _lastTickTimeMs);
        float newMsPerTick = deltaMs / static_cast<float>(elapsed);
        if (_msPerTick > 0.0f) {
            float drift = fabsf(newMsPerTick - _msPerTick) / _msPerTick;
            if (drift > 0.25f) {
                // Large drift implies tempo jump; reset counters for clean resync.
                _tickCounter = 0;
                _step = 0;
                _drunkPosition = 0;
            }
            _msPerTick = (_msPerTick * 0.8f) + (newMsPerTick * 0.2f);
        } else {
            _msPerTick = newMsPerTick;
        }
    }

    _lastClockTickCount = tickCount;
    _lastTickTimeMs = nowMs;

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
    // Derive per-step timing in ms; fall back to tapped BPM when needed.
    float msPerTick = _msPerTick;
    if (msPerTick <= 0.0f && g_tappedBPM > 0.0f) {
        msPerTick = 60000.0f / (g_tappedBPM * 24.0f);
    }
    if (msPerTick <= 0.0f) {
        msPerTick = static_cast<float>(hwConfig.midiTaskInterval);
    }
    float stepDurationMs = msPerTick * static_cast<float>(_lengthTicks);
    float gatePercent = constrain(_gatePercent, 5.0f, 100.0f);
    // LFO swing modulation nudges swing percent up to +/-30%.
    float swingPercent = constrain(_swingPercent + (g_lfoArpSwing * 30.0f), 0.0f, 80.0f);
    unsigned long baseGateMs =
        static_cast<unsigned long>(constrain(stepDurationMs * (gatePercent / 100.0f), 1.0f,
                                             stepDurationMs));
    // Total steps include octave span.
    uint8_t totalSteps = static_cast<uint8_t>(_patternLength * (_octaveRange + 1));
    uint8_t stepCount = stepCountForShape(totalSteps);

    for (uint16_t i = 0; i < events; ++i) {
        uint8_t stepIndex = _step;
        _step = (stepCount > 0) ? static_cast<uint8_t>((_step + 1) % stepCount) : 0;

        bool stepEnabled = true;
        int8_t offset = computeOffset(stepIndex, totalSteps, stepEnabled);
        if (!stepEnabled) {
            continue;
        }

        unsigned long delayMs = 0;
        if (swingPercent > 0.0f && (stepIndex % 2 == 1)) {
            // Delay off-beat steps by swing percent.
            delayMs = static_cast<unsigned long>(stepDurationMs * (swingPercent / 100.0f));
        }

        switch (slot.type) {
        case MIDIMessageType::CC:
            // CC messages can be delayed to preserve swing feel.
            if (delayMs == 0) {
                midi.sendControlChange(slot.data1, constrain(potVal + offset, 0, 127),
                                       slot.midiChannel);
            } else {
                uint8_t value = constrain(potVal + offset, 0, 127);
                Utility::schedulerHigh.addTask(
                    [cc = slot.data1, value, ch = slot.midiChannel, &midi]() {
                        midi.sendControlChange(cc, value, ch);
                    },
                    delayMs, false);
            }
            break;
        case MIDIMessageType::Note: {
            // Notes schedule a NoteOff based on gate length.
            uint8_t note = constrain(root + offset, 0, 127);
            if (delayMs == 0) {
                midi.sendNoteOn(note, potVal, slot.midiChannel);
                Utility::schedulerHigh.addTask(
                    [note, ch = slot.midiChannel, &midi]() { midi.sendNoteOff(note, 0, ch); },
                    baseGateMs, false);
            } else {
                Utility::schedulerHigh.addTask(
                    [note, vel = potVal, ch = slot.midiChannel, &midi]() {
                        midi.sendNoteOn(note, vel, ch);
                    },
                    delayMs, false);
                Utility::schedulerHigh.addTask(
                    [note, ch = slot.midiChannel, &midi]() { midi.sendNoteOff(note, 0, ch); },
                    delayMs + baseGateMs, false);
            }
            break;
        }
        case MIDIMessageType::PitchBend: {
            int raw = usedPot ? potRaw : Utility::mapToRange(root, 0, 127, 0, 1023);
            int16_t bend = map(raw, 0, 1023, -8192, 8191) + offset * 128;
            bend = constrain(bend, -8192, 8191);
            if (delayMs == 0) {
                midi.sendPitchBend(bend, slot.midiChannel);
            } else {
                Utility::schedulerHigh.addTask(
                    [bend, ch = slot.midiChannel, &midi]() { midi.sendPitchBend(bend, ch); },
                    delayMs, false);
            }
            break;
        }
        case MIDIMessageType::ProgramChange:
            if (delayMs == 0) {
                midi.sendProgramChange(constrain(root + offset, 0, 127), slot.midiChannel);
            } else {
                uint8_t program = constrain(root + offset, 0, 127);
                Utility::schedulerHigh.addTask(
                    [program, ch = slot.midiChannel, &midi]() {
                        midi.sendProgramChange(program, ch);
                    },
                    delayMs, false);
            }
            break;
        case MIDIMessageType::Aftertouch:
            if (delayMs == 0) {
                midi.sendAftertouch(constrain(potVal + offset, 0, 127), slot.midiChannel);
            } else {
                uint8_t pressure = constrain(potVal + offset, 0, 127);
                Utility::schedulerHigh.addTask(
                    [pressure, ch = slot.midiChannel, &midi]() {
                        midi.sendAftertouch(pressure, ch);
                    },
                    delayMs, false);
            }
            break;
        case MIDIMessageType::ModWheel:
            if (delayMs == 0) {
                midi.sendModWheel(constrain(potVal + offset, 0, 127), slot.midiChannel);
            } else {
                uint8_t mod = constrain(potVal + offset, 0, 127);
                Utility::schedulerHigh.addTask(
                    [mod, ch = slot.midiChannel, &midi]() { midi.sendModWheel(mod, ch); },
                    delayMs, false);
            }
            break;
        default:
            break;
        }
    }
}
