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
#include "LFO/LFOManager.h"

namespace {

constexpr unsigned long kStartupRandomDelayMs = 2000UL;
constexpr unsigned long kStartupRandomWindowMs = 1000UL;
constexpr unsigned long kStartupRandomFlashInMs = 200UL;
constexpr unsigned long kStartupRandomFlashOutMs = 300UL;
constexpr unsigned long kStartupWhiteSweepStartMs = 4000UL;
constexpr unsigned long kStartupWhiteStepMs = 50UL;
constexpr unsigned long kStartupHoldMs = 500UL;
#if (defined(MN42_DIAG_DISABLE_LED_UI) && (MN42_DIAG_DISABLE_LED_UI != 0)) ||                      \
    (defined(MN42_DIAG_DISABLE_LED_INIT) && (MN42_DIAG_DISABLE_LED_INIT != 0))
constexpr bool kLedHardwareEnabled = false;
#else
constexpr bool kLedHardwareEnabled = true;
#endif

#if (defined(MN42_DIAG_DISABLE_LED_UI) && (MN42_DIAG_DISABLE_LED_UI != 0)) ||                      \
    (defined(MN42_DIAG_DISABLE_DISPLAY_INIT) && (MN42_DIAG_DISABLE_DISPLAY_INIT != 0)) ||          \
    (defined(MN42_DISABLE_DISPLAY_BOOT_PROBE) && (MN42_DISABLE_DISPLAY_BOOT_PROBE != 0))
constexpr bool kDisplayHardwareEnabled = false;
#else
constexpr bool kDisplayHardwareEnabled = true;
#endif

#if defined(MN42_DIAG_DISABLE_LED_UI) && (MN42_DIAG_DISABLE_LED_UI != 0)
constexpr bool kStartupLedAnimationEnabled = false;
#else
constexpr bool kStartupLedAnimationEnabled = true;
#endif
constexpr unsigned long kDisplayRetryIntervalMs = 5000UL;
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

const char *configSlotTypeShortName(MIDIMessageType type) {
    switch (type) {
    case MIDIMessageType::OFF:
        return "OFF";
    case MIDIMessageType::CC:
        return "CC";
    case MIDIMessageType::Note:
        return "NOTE";
    case MIDIMessageType::PitchBend:
        return "BEND";
    case MIDIMessageType::ProgramChange:
        return "PROG";
    case MIDIMessageType::Aftertouch:
        return "AFT";
    case MIDIMessageType::ModWheel:
        return "MOD";
    case MIDIMessageType::NRPN:
        return "NRPN";
    case MIDIMessageType::RPN:
        return "RPN";
    case MIDIMessageType::SysEx:
        return "SYX";
    }
    return "?";
}

const char *syncRatioLabel(LFOSyncRatio ratio) {
    switch (ratio) {
    case LFOSyncRatio::Div1:
        return "1/1";
    case LFOSyncRatio::Div2:
        return "1/2";
    case LFOSyncRatio::Div4:
        return "1/4";
    case LFOSyncRatio::Div8:
        return "1/8";
    case LFOSyncRatio::Div16:
        return "1/16";
    case LFOSyncRatio::Div32:
        return "1/32";
    case LFOSyncRatio::Mul2:
        return "x2";
    case LFOSyncRatio::Mul4:
        return "x4";
    }
    return "?";
}

const char *lfoShapeShortLabel(LFOShape shape) {
    switch (shape) {
    case LFOShape::Sine:
        return "SIN";
    case LFOShape::Triangle:
        return "TRI";
    case LFOShape::Saw:
        return "SAW";
    case LFOShape::Square:
        return "SQR";
    case LFOShape::SampleHold:
        return "S&H";
    case LFOShape::RandomSlew:
        return "RSL";
    }
    return "?";
}

const char *lfoTargetShortLabel(LFOInternalTarget target) {
    switch (target) {
    case LFOInternalTarget::EfGainTrim:
        return "EFG";
    case LFOInternalTarget::ArpSwing:
        return "ARS";
    case LFOInternalTarget::VelocityShift:
        return "VEL";
    case LFOInternalTarget::NoteChance:
        return "CHN";
    case LFOInternalTarget::ArpGate:
        return "GAT";
    case LFOInternalTarget::JitterDepth:
        return "JDP";
    case LFOInternalTarget::JitterSmoothness:
        return "JSM";
    }
    return "?";
}

bool activeInternalTargetForLfo(uint8_t lfoIndex, LFOInternalTarget &targetOut) {
    const size_t count = lfoManager.routeCount();
    LFOManager::Route route{};
    for (size_t i = 0; i < count; ++i) {
        if (!lfoManager.getRoute(i, route)) {
            continue;
        }
        if (route.type == LFOManager::Route::Type::Internal && route.lfoIndex == lfoIndex) {
            targetOut = route.target;
            return true;
        }
    }
    return false;
}

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
        touchControlOverlay();
    }
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
        touchControlOverlay();
    }
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
        touchControlOverlay();
    }
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
        touchControlOverlay();
    }
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

} // namespace

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

namespace {

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

#if defined(MN42_DIAG_DISABLE_LED_UI) && (MN42_DIAG_DISABLE_LED_UI != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"ui_startup_disabled\"}");
#endif
#if defined(MN42_DIAG_DISABLE_LED_INIT) && (MN42_DIAG_DISABLE_LED_INIT != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"ui_led_init_disabled\"}");
#endif
#if defined(MN42_DIAG_DISABLE_DISPLAY_INIT) && (MN42_DIAG_DISABLE_DISPLAY_INIT != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"ui_display_init_disabled\"}");
#endif
#if defined(MN42_DISABLE_DISPLAY_BOOT_PROBE) && (MN42_DISABLE_DISPLAY_BOOT_PROBE != 0)
    LOG_PRINTLN("{\"type\":\"warning\",\"code\":\"display_init_deferred\",\"reason\":\"boot_probe_"
                "disabled\","
                "\"display_present\":false,\"display_ok\":false,\"display_init_failures\":0}");
#endif
#if defined(MN42_DIAG_DISABLE_BUTTON_INIT) && (MN42_DIAG_DISABLE_BUTTON_INIT != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"ui_button_init_disabled\"}");
#endif

    if (kLedHardwareEnabled) {
        ledManager.begin();
    }
    uint8_t ledB;
    CRGB ledC;
    configManager.loadLEDSettings(ledB, ledC);
    constexpr uint8_t kUsbSafeStartupBrightness = 8;
    ledManager.setBrightness(std::min<uint8_t>(ledB, kUsbSafeStartupBrightness));
    ledManager.setColor(ledC);

    const bool displayReady = kDisplayHardwareEnabled && displayManager.begin();
    if (displayReady) {
        displayManager.showText("Initializing...");
    }

#if defined(MN42_DIAG_DISABLE_BUTTON_INIT) && (MN42_DIAG_DISABLE_BUTTON_INIT != 0)
#else
    buttonManager.initButtons();
#endif
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

void serviceDisplayDegradedMode() {
    if (!kDisplayHardwareEnabled || displayManager.isReady()) {
        return;
    }

    const bool displayNeverAcked =
        !displayManager.isPresent() && strcmp(displayManager.getLastInitCode(), "no_i2c_ack") == 0;
    if (displayNeverAcked) {
        return;
    }

    const unsigned long currentMs = now();
    const unsigned long lastAttemptMs = displayManager.getLastInitAttemptMs();
    if (lastAttemptMs != 0 &&
        static_cast<unsigned long>(currentMs - lastAttemptMs) < kDisplayRetryIntervalMs) {
        return;
    }

    if (!displayManager.begin()) {
        return;
    }

    gStartupSequence = {};
    gStartupSequence.displayReady = true;
    displayManager.clear();
    displayManager.displayStatus("Display restored", 1500);
}

bool runStartupSequenceStep() {
    if (!gStartupSequence.active || !gStartupSequence.displayReady) {
        return false;
    }

    if (!gStartupSequence.started) {
        gStartupSequence.started = true;
        gStartupSequence.startMs = millis();
        gStartupSequence.randomPhase = StartupRandomPhase::Unknown;
        gStartupSequence.whiteLit = kStartupLedAnimationEnabled ? 0 : NUM_LEDS();
        if (kStartupLedAnimationEnabled) {
            seedStartupColors(NUM_LEDS());
            ledManager.setAll(CRGB::Black);
        }
        displayManager.clear();
    }

    const unsigned long elapsedMs = millis() - gStartupSequence.startMs;
    if (kStartupLedAnimationEnabled) {
        updateRandomLedStartup(elapsedMs);
        updateWhiteSweep(elapsedMs);
    }
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
    if (buttonManager.isOnDeviceConfigModeActive()) {
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
    const int mappedShift = map(rawShift, 0, 1023, -64, 63);
    const uint8_t mappedProbability = static_cast<uint8_t>(map(rawProb, 0, 1023, 0, 100));

    if (g_noteDynamicsRemoteControlActive) {
        if (!g_noteDynamicsShiftLatched &&
            abs(mappedShift - static_cast<int>(velocityShift)) <= 2) {
            g_noteDynamicsShiftLatched = true;
        }
        if (!g_noteDynamicsProbabilityLatched &&
            abs(static_cast<int>(mappedProbability) - static_cast<int>(changeProbability)) <= 2) {
            g_noteDynamicsProbabilityLatched = true;
        }
        if (!(g_noteDynamicsShiftLatched && g_noteDynamicsProbabilityLatched)) {
            setNoteDynamicsOverlay(static_cast<int8_t>(velocityShift), changeProbability);
            return;
        }
        g_noteDynamicsRemoteControlActive = false;
    }

    velocityShift = static_cast<int8_t>(mappedShift);
    changeProbability = mappedProbability;
    setNoteDynamicsOverlay(static_cast<int8_t>(velocityShift), changeProbability);
}

bool renderControlOverlayIfActive() {
    if (displayManager.isStatusOverlayActive()) {
        return false;
    }

    const unsigned long nowMs = now();
    if (gControlOverlay.kind == ControlOverlayKind::None || nowMs >= gControlOverlay.expiresAt) {
        gControlOverlay.kind = ControlOverlayKind::None;
        return false;
    }

    switch (gControlOverlay.kind) {
    case ControlOverlayKind::Filter:
        displayManager.drawFilterTuning("F", gControlOverlay.freq, "Q", gControlOverlay.q);
        break;
    case ControlOverlayKind::Arp: {
        const uint8_t shapeIdx = static_cast<uint8_t>(gControlOverlay.shapeIdx % kArpShapeCount);
        displayManager.drawArpSettings(gControlOverlay.lengthTicks, kArpShapeNames[shapeIdx]);
        break;
    }
    case ControlOverlayKind::ArpEdit: {
        char line2[16];
        char line3[16];
        snprintf(line2, sizeof(line2), "G%u%%", gControlOverlay.gatePercent);
        snprintf(line3, sizeof(line3), "O+%u", gControlOverlay.octaveRange);
        displayManager.drawText("Arp", line2, line3);
        break;
    }
    case ControlOverlayKind::NoteDynamics: {
        char line2[16];
        char line3[16];
        snprintf(line2, sizeof(line2), "V%+d", gControlOverlay.velocityShift);
        snprintf(line3, sizeof(line3), "P%u%%", gControlOverlay.changeProbability);
        displayManager.drawText("Note", line2, line3);
        break;
    }
    case ControlOverlayKind::None:
    default:
        return false;
    }

    return true;
}

bool renderOnDeviceConfigViewIfActive(const ButtonManagerContext &context) {
    if (!buttonManager.isOnDeviceConfigModeActive()) {
        return false;
    }
    if (context.activePot >= NUM_SLOTS) {
        return false;
    }

    const uint8_t slotIndex = context.activePot;
    const MIDISlot &slot = context.configManager.getSlot(slotIndex);
    const uint8_t channel = context.configManager.getPotChannel(slotIndex);
    uint8_t data1 = context.configManager.getSlotData1(slotIndex);
    if (slot.type == MIDIMessageType::CC) {
        data1 = context.configManager.getPotCCNumber(slotIndex);
    } else if (slot.type == MIDIMessageType::NRPN || slot.type == MIDIMessageType::RPN) {
        data1 = context.configManager.getSlotData1(slotIndex);
    }

    char line1[20];
    char line2[20];
    char line3[20];
    snprintf(line1, sizeof(line1), "Cfg Slot %u", slotIndex);
    snprintf(line2, sizeof(line2), "%s Ch%u D1%u", configSlotTypeShortName(slot.type), channel,
             data1);
    snprintf(line3, sizeof(line3), "C5 Exit+Save");
    displayManager.drawText(line1, line2, line3);
    return true;
}

bool renderLfoTuningViewIfActive() {
    if (!buttonManager.isLfoTuningModeActive()) {
        return false;
    }

    const uint8_t index = buttonManager.lfoTuningIndex() % LFOManager::kMaxLFOs;
    const LFO &lfo = lfoManager.lfo(index);
    const float value = lfoManager.normalizedValue(index);
    LFOInternalTarget target = LFOInternalTarget::EfGainTrim;
    const bool hasTarget = activeInternalTargetForLfo(index, target);

    char line1[20];
    char line2[24];
    char line3[24];
    snprintf(line1, sizeof(line1), "LFO%u T:%s", static_cast<unsigned>(index + 1),
             hasTarget ? lfoTargetShortLabel(target) : "NONE");
    snprintf(line2, sizeof(line2), "%s D%.2f V%.2f", lfoShapeShortLabel(lfo.getShape()),
             lfo.getDepth(), value);
    if (lfo.isSyncEnabled()) {
        snprintf(line3, sizeof(line3), "SYNC %s %s", syncRatioLabel(lfo.getSyncRatio()),
                 lfo.isBipolar() ? "BI" : "UNI");
    } else {
        snprintf(line3, sizeof(line3), "HZ %.2f %s", lfo.getFrequencyHz(),
                 lfo.isBipolar() ? "BI" : "UNI");
    }
    displayManager.drawText(line1, line2, line3);
    return true;
}

bool renderJitterTuningViewIfActive() {
    if (!g_jitterTuningActive) {
        return false;
    }

    const float baseDepth = constrain(g_jitterSettings.depth, 0.0f, 1.0f);
    const float baseSmooth = constrain(g_jitterSettings.smoothness, 0.0f, 1.0f);
    const float effectiveDepth = constrain(baseDepth + (g_lfoJitterDepth * 0.5f), 0.0f, 1.0f);
    const float effectiveSmooth =
        constrain(baseSmooth + (g_lfoJitterSmoothness * 0.5f), 0.0f, 1.0f);

    char line1[20];
    char line2[24];
    char line3[24];
    snprintf(line1, sizeof(line1), "Jitter Tune");
    snprintf(line2, sizeof(line2), "Base D%.2f S%.2f", baseDepth, baseSmooth);
    snprintf(line3, sizeof(line3), "Eff  D%.2f S%.2f", effectiveDepth, effectiveSmooth);
    displayManager.drawText(line1, line2, line3);
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
