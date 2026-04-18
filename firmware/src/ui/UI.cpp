#include "UI.h"

#include <Arduino.h>
#include <algorithm>
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

    SlotEnvelopePayload tailPayload{};
    tailPayload.filterType = static_cast<uint8_t>(context.envelopes[efIndex].getFilterType());
    tailPayload.frequency = freq;
    tailPayload.q = q;
    SlotEnvelopePayload sanitizedTail = configManager.persistFilterTail(tailPayload);
    freq = sanitizedTail.frequency;
    q = sanitizedTail.q;
    context.envelopes[efIndex].configureFilter(freq, q);
    if (context.activePot < NUM_SLOTS) {
        // Persist the user-tuned filter tail while the UI context still owns the slot metadata.
        SlotEnvelopePayload payload =
            configManager.getSlotEnvelopePayload(static_cast<uint8_t>(context.activePot));
        payload.filterType = static_cast<uint8_t>(context.envelopes[efIndex].getFilterType());
        payload.frequency = freq;
        payload.q = q;
        configManager.setSlotEnvelopePayload(static_cast<uint8_t>(context.activePot), payload);
        WebSerial::sendSlotPatch(configManager, static_cast<uint8_t>(context.activePot));
    }
    displayManager.showFilterTuning("Freq", freq, "Q", q);
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
        char line2[24];
        char line3[24];
        snprintf(line2, sizeof(line2), "Gate %u%%", gatePercent);
        snprintf(line3, sizeof(line3), "Oct +%u", octaveRange);
        displayManager.showText("Arp Edit", line2, line3);
        return;
    }

    uint8_t lengthTicks = map(raw1, 0, 1023, 1, Arpeggiator::MAX_LENGTH);
    int shapeIdx = map(raw2, 0, 1023, 0, 5);
    static const char *names[] = {"Up", "Down", "Up-Down", "Random", "Drunk", "Euclid"};
    Arpeggiator::Shape shapes[] = {Arpeggiator::UP,     Arpeggiator::DOWN,  Arpeggiator::UPDOWN,
                                   Arpeggiator::RANDOM, Arpeggiator::DRUNK, Arpeggiator::EUCLIDEAN};

    arpeggiator.setLength(lengthTicks);
    arpeggiator.setShape(shapes[shapeIdx]);

    displayManager.showArpSettings(lengthTicks, names[shapeIdx]);
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

    String line2 = String("Vel ") + String(velocityShift);
    String line3 = String("Prob ") + String(changeProbability) + "%";
    displayManager.showText("Note Dyn", line2.c_str(), line3.c_str());
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
