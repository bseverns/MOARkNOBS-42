#include "UI.h"

#include <Arduino.h>

#include "FirmwareState.h"
#include "Globals.h"
#include "Arpeggiator.h"
#include "WebSerial.h"

// Bring up the physical UI modules and play the startup identity sequence.
void initializeUI() {
    pinMode(hwConfig.statusLedPin, OUTPUT);
    digitalWrite(hwConfig.statusLedPin, LOW);

    ledManager.begin();
    uint8_t ledB;
    CRGB ledC;
    configManager.loadLEDSettings(ledB, ledC);
    ledManager.setBrightness(ledB);
    ledManager.setColor(ledC);

    displayManager.begin();
    displayManager.showText("Initializing...");

    buttonManager.initButtons();
    delay(1000);
    displayManager.clear();
    displayManager.showText("MOAR");
    ledManager.blinkStatusLED(2, 100);
    displayManager.runStartupAnimation();
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
