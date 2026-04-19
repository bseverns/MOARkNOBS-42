#include "UI.h"

#include <Arduino.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "FirmwareState.h"
#include "Globals.h"
#include "Arpeggiator.h"
#include "Log.h"
#include "WebSerial.h"

namespace {

constexpr unsigned long kStartupRandomDelayMs = 2000UL;
constexpr unsigned long kStartupRandomWindowMs = 1000UL;
constexpr unsigned long kStartupRandomFlashInMs = 200UL;
constexpr unsigned long kStartupRandomFlashOutMs = 300UL;
constexpr unsigned long kStartupWhiteSweepStartMs = 4000UL;
constexpr unsigned long kStartupWhiteStepMs = 50UL;
constexpr unsigned long kStartupHoldMs = 500UL;
constexpr unsigned long kControlOverlayHoldMs = 900UL;
constexpr unsigned long kFilterPersistIdleMs = 900UL;
constexpr unsigned long kFilterPersistMinIntervalMs = 500UL;
constexpr float kFilterPersistFreqThresholdHz = 2.0f;
constexpr float kFilterPersistQThreshold = 0.03f;
constexpr float kFilterDisplayFreqThresholdHz = 0.5f;
constexpr float kFilterDisplayQThreshold = 0.01f;
constexpr int kNoteDynamicsDisplayThreshold = 1;
constexpr uint8_t kNoteProbabilityDisplayThreshold = 1;
constexpr uint8_t kArpLengthDisplayThreshold = 1;
constexpr uint8_t kArpOctaveDisplayThreshold = 1;
constexpr uint8_t kArpGateDisplayThreshold = 1;
constexpr uint8_t kArpShapeDisplayThreshold = 1;
constexpr float kFilterMinFreqHz = 20.0f;
constexpr float kFilterMaxFreqHz = 5000.0f;
constexpr float kFilterMinQ = 0.5f;
constexpr float kFilterMaxQ = 4.0f;
constexpr size_t kArpShapeCount = 6;
const char *kArpShapeNames[kArpShapeCount] = {"Up", "Down", "Up-Down", "Random", "Drunk", "Euclid"};

enum class StartupRandomPhase : uint8_t { Unknown = 0xFF, Off = 0, Half = 1, Full = 2 };

struct StartupSequenceState {
    bool active = false;
    bool displayReady = false;
    bool started = false;
    unsigned long startMs = 0;
    StartupRandomPhase randomPhase = StartupRandomPhase::Unknown;
    uint16_t whiteLit = 0;
    std::vector<CRGB> randomColors;
};

StartupSequenceState gStartupSequence;
enum class ControlUiMode : uint8_t { Filter, Arp, NoteDynamics };
enum class ControlOverlayKind : uint8_t { None, Filter, Arp, ArpEdit, NoteDynamics };

struct ControlOverlayState {
    ControlOverlayKind kind = ControlOverlayKind::None;
    unsigned long expiresAt = 0;
    uint32_t revision = 0;
    float freq = 0.0f;
    float q = 0.0f;
    uint8_t lengthTicks = 0;
    uint8_t shapeIdx = 0;
    uint8_t gatePercent = 0;
    uint8_t octaveRange = 0;
    int8_t velocityShift = 0;
    uint8_t changeProbability = 0;
};

struct FilterPersistState {
    bool dirty = false;
    unsigned long lastInputMs = 0;
    unsigned long lastPersistMs = 0;
    SlotEnvelopePayload pending{};
};

ControlOverlayState gControlOverlay;
std::array<FilterPersistState, NUM_SLOTS> gFilterPersistStates{};
uint32_t gLastRenderedOverlayRevision = 0;

CRGB scaleColor(const CRGB &color, uint8_t scale255) {
    return CRGB(static_cast<uint8_t>((static_cast<uint16_t>(color.r) * scale255) / 255U),
                static_cast<uint8_t>((static_cast<uint16_t>(color.g) * scale255) / 255U),
                static_cast<uint8_t>((static_cast<uint16_t>(color.b) * scale255) / 255U));
}

void seedStartupColors(uint16_t ledCount) {
    gStartupSequence.randomColors.assign(ledCount, CRGB::Black);
    for (uint16_t i = 0; i < ledCount; ++i) {
        gStartupSequence.randomColors[i] = CHSV(static_cast<uint8_t>(random(0, 255)), 255, 255);
    }
}

void applyRandomPhase(StartupRandomPhase phase) {
    if (phase == gStartupSequence.randomPhase) {
        return;
    }
    gStartupSequence.randomPhase = phase;

    if (phase == StartupRandomPhase::Off) {
        ledManager.setAll(CRGB::Black);
        return;
    }

    const uint8_t scale = (phase == StartupRandomPhase::Half) ? 128 : 255;
    const uint16_t ledCount = static_cast<uint16_t>(gStartupSequence.randomColors.size());
    for (uint16_t i = 0; i < ledCount; ++i) {
        ledManager.setPixelColor(i, scaleColor(gStartupSequence.randomColors[i], scale));
    }
    ledManager.update();
}

void updateRandomLedStartup(unsigned long elapsedMs) {
    if (elapsedMs < kStartupRandomDelayMs) {
        applyRandomPhase(StartupRandomPhase::Off);
        return;
    }

    const unsigned long phaseElapsed = elapsedMs - kStartupRandomDelayMs;
    if (phaseElapsed >= kStartupRandomWindowMs) {
        applyRandomPhase(StartupRandomPhase::Off);
        return;
    }

    if (phaseElapsed < kStartupRandomFlashInMs) {
        applyRandomPhase(StartupRandomPhase::Half);
        return;
    }
    if (phaseElapsed < kStartupRandomFlashOutMs) {
        applyRandomPhase(StartupRandomPhase::Full);
        return;
    }
    applyRandomPhase(StartupRandomPhase::Half);
}

unsigned long startupSequenceTotalMs(uint16_t ledCount) {
    return kStartupWhiteSweepStartMs +
           (static_cast<unsigned long>(std::max<uint16_t>(ledCount, 1)) * kStartupWhiteStepMs) +
           kStartupHoldMs;
}

void updateWhiteSweep(unsigned long elapsedMs) {
    if (elapsedMs < kStartupWhiteSweepStartMs) {
        return;
    }
    const uint16_t ledCount = NUM_LEDS();
    if (ledCount == 0) {
        return;
    }

    const unsigned long sweepElapsed = elapsedMs - kStartupWhiteSweepStartMs;
    const unsigned long completed = (sweepElapsed / kStartupWhiteStepMs) + 1UL;
    const uint16_t targetLit = static_cast<uint16_t>(
        std::min<unsigned long>(completed, static_cast<unsigned long>(ledCount)));

    if (targetLit <= gStartupSequence.whiteLit) {
        return;
    }

    for (uint16_t i = gStartupSequence.whiteLit; i < targetLit; ++i) {
        ledManager.setPixelColor(i, CRGB::White);
    }
    ledManager.update();
    gStartupSequence.whiteLit = targetLit;
}

bool hasMeaningfulFloatDelta(float a, float b, float threshold) {
    return std::fabs(a - b) >= threshold;
}

bool hasMeaningfulIntDelta(int a, int b, int threshold) { return std::abs(a - b) >= threshold; }

void touchControlOverlay() { gControlOverlay.expiresAt = now() + kControlOverlayHoldMs; }

void setFilterOverlay(float freq, float q) {
    bool changed =
        gControlOverlay.kind != ControlOverlayKind::Filter ||
        hasMeaningfulFloatDelta(gControlOverlay.freq, freq, kFilterDisplayFreqThresholdHz) ||
        hasMeaningfulFloatDelta(gControlOverlay.q, q, kFilterDisplayQThreshold);
    if (changed) {
        gControlOverlay.kind = ControlOverlayKind::Filter;
        gControlOverlay.freq = freq;
        gControlOverlay.q = q;
        ++gControlOverlay.revision;
    }
    touchControlOverlay();
}

void setArpOverlay(uint8_t lengthTicks, uint8_t shapeIdx) {
    bool changed =
        gControlOverlay.kind != ControlOverlayKind::Arp ||
        hasMeaningfulIntDelta(gControlOverlay.lengthTicks, lengthTicks,
                              kArpLengthDisplayThreshold) ||
        hasMeaningfulIntDelta(gControlOverlay.shapeIdx, shapeIdx, kArpShapeDisplayThreshold);
    if (changed) {
        gControlOverlay.kind = ControlOverlayKind::Arp;
        gControlOverlay.lengthTicks = lengthTicks;
        gControlOverlay.shapeIdx = shapeIdx;
        ++gControlOverlay.revision;
    }
    touchControlOverlay();
}

void setArpEditOverlay(uint8_t gatePercent, uint8_t octaveRange) {
    bool changed =
        gControlOverlay.kind != ControlOverlayKind::ArpEdit ||
        hasMeaningfulIntDelta(gControlOverlay.gatePercent, gatePercent, kArpGateDisplayThreshold) ||
        hasMeaningfulIntDelta(gControlOverlay.octaveRange, octaveRange, kArpOctaveDisplayThreshold);
    if (changed) {
        gControlOverlay.kind = ControlOverlayKind::ArpEdit;
        gControlOverlay.gatePercent = gatePercent;
        gControlOverlay.octaveRange = octaveRange;
        ++gControlOverlay.revision;
    }
    touchControlOverlay();
}

void setNoteDynamicsOverlay(int8_t velocity, uint8_t probability) {
    bool changed = gControlOverlay.kind != ControlOverlayKind::NoteDynamics ||
                   hasMeaningfulIntDelta(gControlOverlay.velocityShift, velocity,
                                         kNoteDynamicsDisplayThreshold) ||
                   hasMeaningfulIntDelta(gControlOverlay.changeProbability, probability,
                                         kNoteProbabilityDisplayThreshold);
    if (changed) {
        gControlOverlay.kind = ControlOverlayKind::NoteDynamics;
        gControlOverlay.velocityShift = velocity;
        gControlOverlay.changeProbability = probability;
        ++gControlOverlay.revision;
    }
    touchControlOverlay();
}

bool envelopePayloadChangedMeaningfully(const SlotEnvelopePayload &a,
                                        const SlotEnvelopePayload &b) {
    return a.filterType != b.filterType ||
           hasMeaningfulFloatDelta(a.frequency, b.frequency, kFilterPersistFreqThresholdHz) ||
           hasMeaningfulFloatDelta(a.q, b.q, kFilterPersistQThreshold);
}

SlotEnvelopePayload sanitizeEnvelopePayload(uint8_t filterType, float freq, float q) {
    SlotEnvelopePayload payload{};
    payload.filterType = filterType;
    payload.frequency = constrain(freq, kFilterMinFreqHz, kFilterMaxFreqHz);
    payload.q = constrain(q, kFilterMinQ, kFilterMaxQ);
    return payload;
}

void flushPendingFilterPersists() {
    const unsigned long nowMs = now();
    for (uint8_t slot = 0; slot < NUM_SLOTS; ++slot) {
        FilterPersistState &persist = gFilterPersistStates[slot];
        if (!persist.dirty) {
            continue;
        }

        if (nowMs - persist.lastInputMs < kFilterPersistIdleMs) {
            continue;
        }
        if (nowMs - persist.lastPersistMs < kFilterPersistMinIntervalMs) {
            continue;
        }

        configManager.setSlotEnvelopePayload(slot, persist.pending);
        configManager.persistFilterTail(persist.pending);
        WebSerial::sendSlotPatch(configManager, slot);
        persist.dirty = false;
        persist.lastPersistMs = nowMs;
    }
}

ControlUiMode resolveControlUiMode(const ButtonManagerContext &context) {
    if (arpeggiator.isActive()) {
        return ControlUiMode::Arp;
    }
    if (context.envelopeFollowMode &&
        context.potToEnvelopeMap.find(context.activePot) != context.potToEnvelopeMap.end()) {
        return ControlUiMode::Filter;
    }
    return ControlUiMode::NoteDynamics;
}

} // namespace

// Bring up the physical UI modules and arm the startup sequence state machine.
void initializeUI() {
    pinMode(hwConfig.statusLedPin, OUTPUT);
    digitalWrite(hwConfig.statusLedPin, LOW);

    ledManager.begin();
    uint8_t ledB;
    CRGB ledC;
    configManager.loadLEDSettings(ledB, ledC);
    ledManager.setBrightness(ledB);
    ledManager.setColor(ledC);

    const bool displayReady = displayManager.begin();
    if (displayReady) {
        displayManager.showText("Initializing...");
    } else {
        LOG_PRINTLN("{\"warning\":\"display_init_failed\"}");
    }

    buttonManager.initButtons();
    gStartupSequence = {};
    gStartupSequence.active = displayReady;
    gStartupSequence.displayReady = displayReady;
    gControlOverlay = {};
    gFilterPersistStates = {};

    if (displayReady) {
        displayManager.clear(); // Start from a blank canvas per startup sequence design.
    }
}

bool isStartupSequenceActive() { return gStartupSequence.active; }

bool runStartupSequenceStep() {
    if (!gStartupSequence.active || !gStartupSequence.displayReady) {
        return false;
    }

    if (!gStartupSequence.started) {
        gStartupSequence.started = true;
        gStartupSequence.startMs = millis();
        gStartupSequence.randomPhase = StartupRandomPhase::Unknown;
        gStartupSequence.whiteLit = 0;
        seedStartupColors(NUM_LEDS());
        ledManager.setAll(CRGB::Black);
        displayManager.clear();
    }

    const unsigned long elapsedMs = millis() - gStartupSequence.startMs;
    updateRandomLedStartup(elapsedMs);
    updateWhiteSweep(elapsedMs);
    displayManager.runStartupAnimation();

    const uint16_t ledCount = NUM_LEDS();
    const unsigned long totalMs = startupSequenceTotalMs(ledCount);
    const bool finished = displayManager.isStartupAnimationDone() && (elapsedMs >= totalMs) &&
                          (gStartupSequence.whiteLit >= ledCount);
    if (finished) {
        gStartupSequence.active = false;
    }
    return gStartupSequence.active;
}

void updateControlUi(ButtonManagerContext &context) {
    if (gStartupSequence.active) {
        return;
    }

    switch (resolveControlUiMode(context)) {
    case ControlUiMode::Filter:
        updateFilterTuning(context);
        break;
    case ControlUiMode::Arp:
        updateArpTuning();
        break;
    case ControlUiMode::NoteDynamics:
        updateNoteDynamics();
        break;
    }
    flushPendingFilterPersists();
}

// Read the two control pots as filter-tail tuning for the currently active slot/follower.
void updateFilterTuning(ButtonManagerContext &context) {
    if (g_jitterTuningActive) {
        return;
    }
    int rawFreq = buttonManager.getControlPotValue(1);
    int rawQ = buttonManager.getControlPotValue(2);

    float freq = map(rawFreq, 0, 1023, 20, 5000);
    float q = map(rawQ, 0, 1023, 50, 400) / 100.0f;

    auto it = context.potToEnvelopeMap.find(context.activePot);
    if (it == context.potToEnvelopeMap.end()) {
        return;
    }
    MIDISlot::EfSettings &settings = it->second;
    int efIndex = settings.followerIndex;
    if (efIndex < 0 || efIndex >= static_cast<int>(context.envelopes.size())) {
        return;
    }

    const uint8_t filterType = static_cast<uint8_t>(context.envelopes[efIndex].getFilterType());
    SlotEnvelopePayload desired = sanitizeEnvelopePayload(filterType, freq, q);
    freq = desired.frequency;
    q = desired.q;
    context.envelopes[efIndex].configureFilter(freq, q);

    if (context.activePot < NUM_SLOTS) {
        // Live filter response is immediate; persistence is idle-debounced to avoid flash churn.
        const uint8_t slotIndex = static_cast<uint8_t>(context.activePot);
        SlotEnvelopePayload current = configManager.getSlotEnvelopePayload(slotIndex);
        if (envelopePayloadChangedMeaningfully(current, desired)) {
            FilterPersistState &persist = gFilterPersistStates[slotIndex];
            persist.pending = desired;
            persist.lastInputMs = now();
            persist.dirty = true;
        }
    }
    setFilterOverlay(freq, q);
}

// Reinterpret the two control pots as arpeggiator timing/shape or gate/octave edits.
void updateArpTuning() {
    if (g_jitterTuningActive) {
        return;
    }
    if (!arpeggiator.isActive())
        return;

    int raw1 = buttonManager.getControlPotValue(1);
    int raw2 = buttonManager.getControlPotValue(2);

    if (g_arpEditActive) {
        uint8_t gatePercent = map(raw1, 0, 1023, 5, 100);
        uint8_t octaveRange = map(raw2, 0, 1023, 0, 3);
        arpeggiator.setGatePercent(gatePercent);
        arpeggiator.setOctaveRange(octaveRange);
        setArpEditOverlay(gatePercent, octaveRange);
        return;
    }

    uint8_t lengthTicks = map(raw1, 0, 1023, 1, Arpeggiator::MAX_LENGTH);
    int shapeIdx = map(raw2, 0, 1023, 0, static_cast<int>(kArpShapeCount - 1));
    Arpeggiator::Shape shapes[] = {Arpeggiator::UP,     Arpeggiator::DOWN,  Arpeggiator::UPDOWN,
                                   Arpeggiator::RANDOM, Arpeggiator::DRUNK, Arpeggiator::EUCLIDEAN};

    arpeggiator.setLength(lengthTicks);
    arpeggiator.setShape(shapes[shapeIdx]);
    setArpOverlay(lengthTicks, static_cast<uint8_t>(shapeIdx));
}

// Reinterpret the two control pots as note velocity/probability shaping when arp is idle.
void updateNoteDynamics() {
    if (g_jitterTuningActive) {
        return;
    }
    if (arpeggiator.isActive())
        return;

    int rawShift = buttonManager.getControlPotValue(1);
    int rawProb = buttonManager.getControlPotValue(2);

    velocityShift = map(rawShift, 0, 1023, -64, 63);
    changeProbability = static_cast<uint8_t>(map(rawProb, 0, 1023, 0, 100));
    setNoteDynamicsOverlay(static_cast<int8_t>(velocityShift), changeProbability);
}

bool renderControlOverlayIfActive() {
    if (displayManager.isStatusOverlayActive()) {
        gLastRenderedOverlayRevision = 0;
        return false;
    }

    const unsigned long nowMs = now();
    if (gControlOverlay.kind == ControlOverlayKind::None || nowMs >= gControlOverlay.expiresAt) {
        gControlOverlay.kind = ControlOverlayKind::None;
        gLastRenderedOverlayRevision = 0;
        return false;
    }

    if (gLastRenderedOverlayRevision == gControlOverlay.revision) {
        return true;
    }

    switch (gControlOverlay.kind) {
    case ControlOverlayKind::Filter:
        displayManager.showFilterTuning("F", gControlOverlay.freq, "Q", gControlOverlay.q);
        break;
    case ControlOverlayKind::Arp: {
        const uint8_t shapeIdx = static_cast<uint8_t>(gControlOverlay.shapeIdx % kArpShapeCount);
        displayManager.showArpSettings(gControlOverlay.lengthTicks, kArpShapeNames[shapeIdx]);
        break;
    }
    case ControlOverlayKind::ArpEdit: {
        char line2[16];
        char line3[16];
        snprintf(line2, sizeof(line2), "G%u%%", gControlOverlay.gatePercent);
        snprintf(line3, sizeof(line3), "O+%u", gControlOverlay.octaveRange);
        displayManager.showText("Arp", line2, line3);
        break;
    }
    case ControlOverlayKind::NoteDynamics: {
        char line2[16];
        char line3[16];
        snprintf(line2, sizeof(line2), "V%+d", gControlOverlay.velocityShift);
        snprintf(line3, sizeof(line3), "P%u%%", gControlOverlay.changeProbability);
        displayManager.showText("Note", line2, line3);
        break;
    }
    case ControlOverlayKind::None:
    default:
        return false;
    }

    gLastRenderedOverlayRevision = gControlOverlay.revision;
    return true;
}

// Push the current runtime snapshot out over WebSerial for the browser/editor layer.
void streamWebSerialState() {
    if (!webSerialStreaming)
        return;
    const SystemDiagnostics diagSnapshot = captureDiagnosticsSnapshot();
    // WebSerial mirrors the same state snapshot the firmware_main loop would show students.
    WebSerial::sendStateSnapshot(potentiometerManager, envelopeFollowers, configManager,
                                 buttonContext.activePot, diagSnapshot);
}
