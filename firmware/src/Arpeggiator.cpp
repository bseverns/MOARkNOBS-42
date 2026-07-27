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

// Longest span between notes, in MIDI clock ticks. Anything longer loses the groove.
constexpr uint8_t MAX_LENGTH = Arpeggiator::MAX_LENGTH;

// Handles per-slot arpeggiation. The constructor only seeds defaults; real work
// happens once start() is called from firmware_main.cpp. We store primitive
// types instead of dynamic allocations so there’s nothing sneaky to clean up
// when the arp idles.

Arpeggiator::Arpeggiator()
    : _lengthTicks(12), _shape(UP), _patternLength(DEFAULT_PATTERN_LENGTH), _swingPercent(0.0f),
      _gatePercent(50.0f), _octaveRange(0), _baseNote(0), _baseNoteSrc(BaseNoteSource::Pot),
      _baseNoteIsSet(false), _baseNoteCb(nullptr), _slots{}, _primarySlot(0) {}

// Begin generating an arpeggio for the given slot. The slot index refers to the
// entry stored by ConfigManager and determines both MIDI type and channel.

void Arpeggiator::start(uint8_t slotIdx) {
    if (slotIdx >= NUM_SLOTS) {
        return;
    }
    SlotState &state = _slots[slotIdx];
    state.active = true;
    state.tickCounter = 0;
    state.step = 0;
    state.clock.reset();
    state.rngState = 0x12345678u ^ static_cast<uint32_t>(slotIdx);
    state.drunkPosition = 0;
    _primarySlot = slotIdx;
}

// Stop arpeggiation immediately. update() will simply return once inactive.
void Arpeggiator::stop() {
    for (SlotState &state : _slots) {
        state.active = false;
        state.tickCounter = 0;
        state.step = 0;
        state.drunkPosition = 0;
    }
}

bool Arpeggiator::queueEvent(const PendingEvent &event) {
    for (PendingEvent &entry : _pendingEvents) {
        if (!entry.active) {
            entry = event;
            entry.active = true;
            return true;
        }
    }
    ++g_systemDiagnostics.midiDropCount;
    return false;
}

bool Arpeggiator::queueNotePair(uint8_t note, uint8_t velocity, uint8_t channel, uint32_t onDue,
                                uint32_t offDue) {
    PendingEvent *first = nullptr;
    PendingEvent *second = nullptr;
    for (PendingEvent &entry : _pendingEvents) {
        if (!entry.active) {
            if (!first) first = &entry;
            else { second = &entry; break; }
        }
    }
    if (!first || !second) {
        ++g_systemDiagnostics.midiDropCount;
        return false;
    }
    *first = {onDue, PendingEventType::NoteOn, channel, note, velocity, 0, true};
    *second = {offDue, PendingEventType::NoteOff, channel, note, 0, 0, true};
    return true;
}

void Arpeggiator::processPendingEvents(MIDIHandler &midi) {
    const uint32_t current = now();
    for (PendingEvent &event : _pendingEvents) {
        if (!event.active || static_cast<long>(current - event.due) < 0) continue;
        switch (event.type) {
        case PendingEventType::NoteOn: midi.sendNoteOn(event.data1, event.data2, event.channel); break;
        case PendingEventType::NoteOff: midi.sendNoteOff(event.data1, 0, event.channel); break;
        case PendingEventType::CC: midi.sendControlChange(event.data1, event.data2, event.channel); break;
        case PendingEventType::PitchBend: midi.sendPitchBend(event.bend, event.channel); break;
        case PendingEventType::Program: midi.sendProgramChange(event.data1, event.channel); break;
        case PendingEventType::Aftertouch: midi.sendAftertouch(event.data1, event.channel); break;
        case PendingEventType::ModWheel: midi.sendModWheel(event.data1, event.channel); break;
        }
        event.active = false;
    }
}

void Arpeggiator::stop(uint8_t slotIdx) {
    if (slotIdx >= NUM_SLOTS) {
        return;
    }
    SlotState &state = _slots[slotIdx];
    state.active = false;
    state.tickCounter = 0;
    state.step = 0;
    state.drunkPosition = 0;
    _primarySlot = resolvePrimarySlot();
}

bool Arpeggiator::isActive() const {
    for (const SlotState &state : _slots) {
        if (state.active) {
            return true;
        }
    }
    return false;
}

bool Arpeggiator::isActive(uint8_t slotIdx) const {
    return slotIdx < NUM_SLOTS && _slots[slotIdx].active;
}

uint8_t Arpeggiator::getSlot() const { return resolvePrimarySlot(); }

void Arpeggiator::setLength(uint8_t ticks) { _lengthTicks = constrain(ticks, 1, MAX_LENGTH); }

void Arpeggiator::setShape(Shape s) { _shape = s; }

void Arpeggiator::setPatternLength(uint8_t steps) {
    steps = constrain(steps, MIN_PATTERN_LENGTH, MAX_PATTERN_LENGTH);
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
// Convert the user-facing jitter "smoothness" knob into the sampling rate used
// by the Perlin-noise walker that powers RANDOM mode.
float jitterRateFromSmoothness(float smoothness) {
    float clamped = constrain(smoothness, 0.0f, 1.0f);
    return Utility::scale(clamped, 0.0f, 1.0f, 2.0f, 0.05f);
}

float effectiveJitterDepth() {
    float base = constrain(g_jitterSettings.depth, 0.0f, 1.0f);
    return constrain(base + (g_lfoJitterDepth * 0.5f), 0.0f, 1.0f);
}

float effectiveJitterSmoothness() {
    float base = constrain(g_jitterSettings.smoothness, 0.0f, 1.0f);
    return constrain(base + (g_lfoJitterSmoothness * 0.5f), 0.0f, 1.0f);
}

uint8_t euclideanPulseCount(uint8_t totalSteps) {
    if (totalSteps <= 1) {
        return 1;
    }
    uint8_t pulses = static_cast<uint8_t>((static_cast<uint16_t>(totalSteps) * 5U + 11U) / 12U);
    return static_cast<uint8_t>(constrain(pulses, 1, totalSteps - 1));
}

bool euclideanHit(uint8_t stepIndex, uint8_t totalSteps, uint8_t pulses) {
    return totalSteps > 0 && ((stepIndex * pulses) % totalSteps) < pulses;
}

uint8_t euclideanHitOrdinal(uint8_t stepIndex, uint8_t totalSteps, uint8_t pulses) {
    uint8_t ordinal = 0;
    for (uint8_t i = 0; i < stepIndex; ++i) {
        if (euclideanHit(i, totalSteps, pulses)) {
            ++ordinal;
        }
    }
    return ordinal;
}
} // namespace

// Deterministic RNG used for DRUNK steps.
uint8_t Arpeggiator::resolvePrimarySlot() const {
    if (_primarySlot < NUM_SLOTS && _slots[_primarySlot].active) {
        return _primarySlot;
    }
    for (uint8_t slotIdx = 0; slotIdx < NUM_SLOTS; ++slotIdx) {
        if (_slots[slotIdx].active) {
            return slotIdx;
        }
    }
    return 0;
}

uint32_t Arpeggiator::nextRng(uint8_t slotIdx) {
    SlotState &state = _slots[slotIdx];
    state.rngState = (state.rngState * 1664525u) + 1013904223u;
    return state.rngState;
}

// Random walk that nudges the step position by a jitter-scaled amount each hit.
int8_t Arpeggiator::nextDrunkOffset(uint8_t slotIdx, uint8_t totalSteps) {
    if (totalSteps <= 1) {
        return 0;
    }
    SlotState &state = _slots[slotIdx];
    uint8_t pos = static_cast<uint8_t>(state.drunkPosition);
    uint32_t r = nextRng(slotIdx);

    const uint8_t maxUsefulJump = static_cast<uint8_t>((totalSteps - 1 < 6) ? totalSteps - 1 : 6);
    const uint8_t maxJump = static_cast<uint8_t>(
        1 + lroundf(effectiveJitterDepth() * static_cast<float>(maxUsefulJump - 1)));
    const uint8_t magnitude = static_cast<uint8_t>(((r >> 16) % maxJump) + 1);
    const int direction = (r & 0x80000000UL) ? 1 : -1;

    int nextPos = static_cast<int>(pos) + direction * static_cast<int>(magnitude);
    while (nextPos < 0) {
        nextPos += totalSteps;
    }
    nextPos %= totalSteps;
    state.drunkPosition = static_cast<int8_t>(nextPos);
    return state.drunkPosition;
}

// UPDOWN walks up then back down, so we extend the step count.
// Compute the logical step count after shape-specific path expansion.
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
        pos = static_cast<uint8_t>(nextDrunkOffset(resolvePrimarySlot(), totalSteps));
        break;
    case Arpeggiator::EUCLIDEAN: {
        // Rhythm chooses hit positions; melody advances only on those hits.
        const uint8_t pulses = euclideanPulseCount(totalSteps);
        if (!euclideanHit(stepIndex, totalSteps, pulses)) {
            stepEnabled = false;
            return 0;
        }
        pos = static_cast<uint8_t>(euclideanHitOrdinal(stepIndex, totalSteps, pulses) % totalSteps);
        break;
    }
    case Arpeggiator::RANDOM:
    default: {
        float depth = effectiveJitterDepth();
        float rate = jitterRateFromSmoothness(effectiveJitterSmoothness());
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
void Arpeggiator::updateSlot(uint8_t slotIdx, SlotState &state, MIDIHandler &midi,
                             ConfigManager &cfg, PotentiometerManager &pots) {
    unsigned long nowMs = now();
    state.clock.observe(midi.clockTickCount(), nowMs, midi.isClockRunning());

    if (state.clock.justResumed()) {
        state.tickCounter = 0;
        state.step = 0;
        state.drunkPosition = 0;
        state.clock.clearResumeFlag();
    }

    if (state.clock.driftDetected()) {
        state.tickCounter = 0;
        state.step = 0;
        state.drunkPosition = 0;
        state.clock.clearDriftFlag();
    }

    uint32_t elapsed = state.clock.consumeTicks();
    if (elapsed == 0) {
        return;
    }

    uint32_t ticks = static_cast<uint32_t>(state.tickCounter) + elapsed;
    uint32_t events = ticks / _lengthTicks;
    state.tickCounter = static_cast<uint8_t>(ticks % _lengthTicks);
    if (events == 0) {
        return;
    }
    if (events > MAX_CATCH_UP_EMISSIONS_PER_UPDATE) {
        events = MAX_CATCH_UP_EMISSIONS_PER_UPDATE;
    }

    MIDISlot &slot = cfg.getSlot(slotIdx);
    if (!slot.active)
        return;

    auto clampMidi = [](int value) { return static_cast<uint8_t>(constrain(value, 0, 127)); };

    uint8_t root = 0;
    bool haveRoot = false;
    bool usedPot = false;
    int potRaw = 0;

    auto readPot = [&]() {
        potRaw = pots.getLastValue(slotIdx);
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
    float msPerTick = state.clock.msPerTick();
    if (msPerTick <= 0.0f && g_tappedBPM > 0.0f) {
        msPerTick = 60000.0f / (g_tappedBPM * 24.0f);
    }
    if (msPerTick <= 0.0f) {
        msPerTick = static_cast<float>(hwConfig.midiTaskInterval);
    }
    float stepDurationMs = msPerTick * static_cast<float>(_lengthTicks);
    float gatePercent = constrain(_gatePercent + (g_lfoArpGate * 35.0f), 5.0f, 100.0f);
    // LFO swing modulation nudges swing percent up to +/-30%.
    float swingPercent = constrain(_swingPercent + (g_lfoArpSwing * 30.0f), 0.0f, 80.0f);
    unsigned long baseGateMs = static_cast<unsigned long>(
        constrain(stepDurationMs * (gatePercent / 100.0f), 1.0f, stepDurationMs));
    // Total steps include octave span.
    uint8_t totalSteps = static_cast<uint8_t>(_patternLength * (_octaveRange + 1));
    uint8_t stepCount = stepCountForShape(totalSteps);

    for (uint16_t i = 0; i < events; ++i) {
        uint8_t stepIndex = state.step;
        state.step = (stepCount > 0) ? static_cast<uint8_t>((state.step + 1) % stepCount) : 0;

        bool stepEnabled = true;
        const uint8_t priorPrimary = _primarySlot;
        _primarySlot = slotIdx;
        int8_t offset = computeOffset(stepIndex, totalSteps, stepEnabled);
        _primarySlot = priorPrimary;
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
                queueEvent({nowMs + delayMs, PendingEventType::CC, slot.midiChannel, slot.data1,
                            value, 0, false});
            }
            break;
        case MIDIMessageType::Note: {
            // Notes schedule a NoteOff based on gate length.
            uint8_t note = constrain(root + offset, 0, 127);
            int lfoVelocityOffset = static_cast<int>(lroundf(g_lfoVelocityShift * 32.0f));
            uint8_t velocity = static_cast<uint8_t>(
                constrain(static_cast<int>(potVal) + velocityShift + lfoVelocityOffset, 0, 127));
            int lfoChanceOffset = static_cast<int>(lroundf(g_lfoNoteChance * 40.0f));
            int effectiveChance =
                constrain(static_cast<int>(changeProbability) + lfoChanceOffset, 0, 100);
            if (random(100U) >= static_cast<uint8_t>(effectiveChance)) {
                break;
            }
            queueNotePair(note, velocity, slot.midiChannel, nowMs + delayMs,
                          nowMs + delayMs + baseGateMs);
            break;
        }
        case MIDIMessageType::PitchBend: {
            int raw = usedPot ? potRaw : Utility::mapToRange(root, 0, 127, 0, 1023);
            int16_t bend = map(raw, 0, 1023, -8192, 8191) + offset * 128;
            bend = constrain(bend, -8192, 8191);
            if (delayMs == 0) {
                midi.sendPitchBend(bend, slot.midiChannel);
            } else {
                queueEvent({nowMs + delayMs, PendingEventType::PitchBend, slot.midiChannel, 0, 0,
                            bend, false});
            }
            break;
        }
        case MIDIMessageType::ProgramChange:
            if (delayMs == 0) {
                midi.sendProgramChange(constrain(root + offset, 0, 127), slot.midiChannel);
            } else {
                uint8_t program = constrain(root + offset, 0, 127);
                queueEvent({nowMs + delayMs, PendingEventType::Program, slot.midiChannel, program,
                            0, 0, false});
            }
            break;
        case MIDIMessageType::Aftertouch:
            if (delayMs == 0) {
                midi.sendAftertouch(constrain(potVal + offset, 0, 127), slot.midiChannel);
            } else {
                uint8_t pressure = constrain(potVal + offset, 0, 127);
                queueEvent({nowMs + delayMs, PendingEventType::Aftertouch, slot.midiChannel,
                            pressure, 0, 0, false});
            }
            break;
        case MIDIMessageType::ModWheel:
            if (delayMs == 0) {
                midi.sendModWheel(constrain(potVal + offset, 0, 127), slot.midiChannel);
            } else {
                uint8_t mod = constrain(potVal + offset, 0, 127);
                queueEvent({nowMs + delayMs, PendingEventType::ModWheel, slot.midiChannel, mod, 0,
                            0, false});
            }
            break;
        default:
            break;
        }
    }
}

void Arpeggiator::update(MIDIHandler &midi, ConfigManager &cfg, PotentiometerManager &pots) {
    processPendingEvents(midi);
    if (!isActive()) return;
    for (uint8_t slotIdx = 0; slotIdx < NUM_SLOTS; ++slotIdx) {
        SlotState &state = _slots[slotIdx];
        if (!state.active) {
            continue;
        }
        updateSlot(slotIdx, state, midi, cfg, pots);
    }
    processPendingEvents(midi);
}
