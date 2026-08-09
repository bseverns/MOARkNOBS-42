/*
MOARkNOBS Hardware Smoke Test

Mirrors the production firmware stack: boot sequence, scheduler rhythm,
and per-manager checks happen in order so you can trace the same flow you
teach in `firmware_main.cpp` (protocol → modes → UI → runtime).
Each test phase simply exercises the managers that `FirmwareState.cpp`
keeps alive while the scheduler would normally choreograph their callbacks.

Validates LEDs, the button matrix, one slot pot plus two filter-tuning pots,
envelope followers and the OLED display. Use Control Button #0 to advance
through each phase.

Build and upload with PlatformIO environment `teensy40_full_system`
(e.g. `platformio run -e teensy40_full_system -t upload`).
Requires a Teensy 4.0 wired with the full MOARkNOBS hardware
(button matrix, LED strip, OLED, envelope circuits).

See `firmware/README.md` under "Test Philosophy (and Real Talk)"
for additional context and a list of all available test sketches.
*/

#include <Arduino.h>
#include "Arduino.h"
#include "ButtonManager.h"
#include "DisplayManager.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "LEDManager.h"
#include "LFO/LFOManager.h"
#include "MIDIHandler.h"
#include "Modes.h"
#include "PotentiometerManager.h"
#include "SystemTestShim.h"
#include "TestHelpers.h"

SystemTestSummary runSystemTests();

namespace {
const CRGB kBringUpLedTestColor = CRGB::Green;
constexpr unsigned long kControlSuiteTimeoutMs = 20000;

void showBringUpLed(uint16_t index) {
    ledManager.setColor(CRGB::Black);
    ledManager.setPixelColor(index, kBringUpLedTestColor);
    ledManager.update();
    delay(50);
}

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

void drawInstruction(const char *line1, const char *line2, const char *line3) {
    displayManager.beginDraw();
    displayManager.drawText(line1, line2, line3);
    displayManager.endDraw();
}

void drawOnDeviceConfigView() {
    const uint8_t slotIndex = activePot;
    const MIDISlot &slot = configManager.getSlot(slotIndex);
    const uint8_t channel = configManager.getPotChannel(slotIndex);
    uint8_t data1 = configManager.getPotCCNumber(slotIndex);
    if (slot.type == MIDIMessageType::NRPN || slot.type == MIDIMessageType::RPN) {
        data1 = configManager.getSlotData1(slotIndex);
    }

    char line1[20];
    char line2[24];
    snprintf(line1, sizeof(line1), "Cfg Slot %u", slotIndex);
    snprintf(line2, sizeof(line2), "%s Ch%u D1%u", configSlotTypeShortName(slot.type), channel,
             data1);

    displayManager.beginDraw();
    displayManager.drawText(line1, line2, "C5 Exit+Save");
    displayManager.endDraw();
}

void drawLfoTuningView() {
    const uint8_t index = buttonManager.lfoTuningIndex() % LFOManager::kMaxLFOs;
    const LFO &lfo = lfoManager.lfo(index);
    LFOInternalTarget target = LFOInternalTarget::EfGainTrim;
    const bool hasTarget = activeInternalTargetForLfo(index, target);

    char line1[20];
    char line2[24];
    char line3[24];
    snprintf(line1, sizeof(line1), "LFO%u T:%s", static_cast<unsigned>(index + 1),
             hasTarget ? lfoTargetShortLabel(target) : "NONE");
    snprintf(line2, sizeof(line2), "%s D%.2f", lfoShapeShortLabel(lfo.getShape()), lfo.getDepth());
    if (lfo.isSyncEnabled()) {
        snprintf(line3, sizeof(line3), "SYNC %s %s", syncRatioLabel(lfo.getSyncRatio()),
                 lfo.isBipolar() ? "BI" : "UNI");
    } else {
        snprintf(line3, sizeof(line3), "HZ %.2f %s", lfo.getFrequencyHz(),
                 lfo.isBipolar() ? "BI" : "UNI");
    }

    displayManager.beginDraw();
    displayManager.drawText(line1, line2, line3);
    displayManager.endDraw();
}

void serviceControlSuiteUi() {
    buttonManager.processButtons(buttonContext);
    if (buttonManager.isOnDeviceConfigModeActive()) {
        drawOnDeviceConfigView();
    } else if (buttonManager.isLfoTuningModeActive()) {
        drawLfoTuningView();
    }
    delay(2);
}
} // namespace

// Control button used to advance test phases
const uint8_t phaseButtonPin = 12; // Control Button #0

// Ordered test phases
enum class TestPhase { IDLE, LEDS, BUTTONS, POTS, ENVELOPES, CONTROL, DISP, SYSTEM, COMPLETE };
TestPhase currentPhase = TestPhase::IDLE;
bool phaseStarted = false;

// --- Utility ---
// Wait for Control Button #0 to be pressed and released.
void waitForButtonPress(const char *prompt = "Press Btn0 to continue") {
    Serial.println(prompt);
    displayManager.showText(prompt, "", "Btn0");
    while (digitalRead(phaseButtonPin))
        ; // wait press (active LOW)
    while (!digitalRead(phaseButtonPin))
        ; // wait release
    delay(50);
}

// --- Unit Tests ---
// Cycle each LED cluster and make sure FastLED actually spits bits.
// We don't block on every single diode; instead we run a sweep and ask the
// human if the glow looked good.  Punk rock hardware testing, basically.
void testLEDManager() {
    Serial.println("\n--- LEDManager Test ---");
    const uint8_t previousBrightness = ledManager.getBrightness();
    ledManager.setBrightness(MN42_SAFE_BENCH_LED_BRIGHTNESS);

    // --- Slot LEDs ---
    for (int i = 0; i < SLOT_LED_COUNT; i++) {
        showBringUpLed(static_cast<uint16_t>(i));
    }
    waitForButtonPress("Slots glow in order? Btn0");

    // --- Envelope follower LEDs ---
    for (int i = 0; i < EF_LED_COUNT; i++) {
        showBringUpLed(static_cast<uint16_t>(EF_LED_OFFSET() + i));
    }
    waitForButtonPress("Followers shine? Btn0");

    // --- Control button LED ---
    showBringUpLed(CONTROL_LED_INDEX());
    waitForButtonPress("Control LED pop? Btn0");

    // --- Pot halo LEDs ---
    for (int i = 0; i < POT_LED_COUNT; i++) {
        showBringUpLed(static_cast<uint16_t>(POT_LED_OFFSET() + i));
    }
    waitForButtonPress("Halos look righteous? Btn0");

    ledManager.setColor(CRGB::Black);
    ledManager.update();
    ledManager.setBrightness(previousBrightness);
    Serial.println("LEDManager test done.");
}

void testButtonManager() {
    Serial.println("\n--- ButtonManager Test ---");
    Serial.println("Press each virtual button (mux matrix) when prompted.");
    for (int b = 0; b < NUM_VIRTUAL_BUTTONS; b++) {
        Serial.printf("Press Virtual Button #%d...\n", b);
        while (!buttonManager.isMuxButtonPressed(b))
            ;
        Serial.printf("Button #%d OK!\n", b);
        delay(200);
    }

    Serial.println("Press each physical control button now.");
    // Pin 6 reserved for LED strip
    for (int i = 0; i < NUM_CONTROL_BUTTONS; i++) {
        Serial.printf("Press Control Button #%d (pin %d)...\n", i, TEST_CONTROL_PINS[i]);
        while (digitalRead(TEST_CONTROL_PINS[i]))
            ;
        Serial.printf("Control Button #%d OK!\n", i);
        delay(200);
    }
    Serial.println("ButtonManager test done.");
}

// Checks the lone slot pot plus the two filter-tuning rebels.
void testPotentiometerManager() {
    Serial.println("\n--- PotentiometerManager Test ---");

    // --- Slot pot -----------------------------------------------------------
    Serial.println("Slot Pot: crank to MIN, hit Btn0.");
    waitForButtonPress();
    int slotMin = potentiometerManager.readRawPot(0);

    Serial.println("Slot Pot: now twist to MAX, hit Btn0.");
    waitForButtonPress();
    int slotMax = potentiometerManager.readRawPot(0);

    int slotDelta = slotMax - slotMin;
    bool slotPass = slotDelta >= POT_RANGE_MIN;
    Serial.printf("Slot Pot sweep: MIN=%d MAX=%d Δ=%d [%s]\n", slotMin, slotMax, slotDelta,
                  slotPass ? "PASS" : "FAIL");

    // --- Filter frequency pot ----------------------------------------------
    Serial.println("Filter Freq: roll to MIN, Btn0.");
    waitForButtonPress();
    int freqMin = buttonManager.getControlPotValue(1);

    Serial.println("Filter Freq: peg it to MAX, Btn0.");
    waitForButtonPress();
    int freqMax = buttonManager.getControlPotValue(1);

    int freqDelta = freqMax - freqMin;
    bool freqPass = freqDelta >= POT_RANGE_MIN;
    Serial.printf("Filter Freq sweep: MIN=%d MAX=%d Δ=%d [%s]\n", freqMin, freqMax, freqDelta,
                  freqPass ? "PASS" : "FAIL");

    // --- Filter resonance/Q pot --------------------------------------------
    Serial.println("Filter Q: dive to MIN, Btn0.");
    waitForButtonPress();
    int qMin = buttonManager.getControlPotValue(2);

    Serial.println("Filter Q: hammer to MAX, Btn0.");
    waitForButtonPress();
    int qMax = buttonManager.getControlPotValue(2);

    int qDelta = qMax - qMin;
    bool qPass = qDelta >= POT_RANGE_MIN;
    Serial.printf("Filter Q sweep: MIN=%d MAX=%d Δ=%d [%s]\n", qMin, qMax, qDelta,
                  qPass ? "PASS" : "FAIL");

    Serial.println("PotentiometerManager test done.");
}

void testFilterPots() {
    Serial.println("\n--- Filter Pot Test ---");
    Serial.println("Sweep Freq & Q pots then press Btn0.");
    float minFreq = 1e6, maxFreq = 0;
    float minQ = 10, maxQ = 0;
    while (digitalRead(phaseButtonPin)) {
        int rawFreq = buttonManager.getControlPotValue(1);
        int rawQ = buttonManager.getControlPotValue(2);
        float freq = map(rawFreq, 0, 1023, 20, 5000);
        float q = map(rawQ, 0, 1023, 50, 400) / 100.0f;
        if (freq < minFreq)
            minFreq = freq;
        if (freq > maxFreq)
            maxFreq = freq;
        if (q < minQ)
            minQ = q;
        if (q > maxQ)
            maxQ = q;
        displayManager.showFilterTuning("Freq", freq, "Q", q);
        Serial.printf("Freq=%.1f Hz Q=%.2f\n", freq, q);
        delay(100);
    }
    while (!digitalRead(phaseButtonPin))
        ;
    bool freqPass = (minFreq <= 30 && maxFreq >= 4900);
    bool qPass = (minQ <= 0.6 && maxQ >= 3.9);
    Serial.printf("Freq range: %.1f-%.1f Hz [%s]\n", minFreq, maxFreq, freqPass ? "PASS" : "FAIL");
    Serial.printf("Q range: %.1f-%.1f [%s]\n", minQ, maxQ, qPass ? "PASS" : "FAIL");
    char line1[32], line2[32];
    snprintf(line1, sizeof(line1), "F %.0f-%.0f %s", minFreq, maxFreq, freqPass ? "OK" : "BAD");
    snprintf(line2, sizeof(line2), "Q %.1f-%.1f %s", minQ, maxQ, qPass ? "OK" : "BAD");
    displayManager.showText("Filter Pots", line1, line2);
    delay(1000);
    displayManager.clear();
}

void testEnvelopeFollowers() {
    Serial.println("\n--- EnvelopeFollower Test ---");
    int pins[] = {A0, A1, A2, A3, A6, A7};
    for (size_t i = 0; i < envelopeFollowers.size(); i++) {
        Serial.printf("Envelope #%d (Pin A%d): MIN input, Btn0.\n", i, pins[i]);
        waitForButtonPress();
        envelopeFollowers[i].update();
        int vmin = envelopeFollowers[i].getEnvelopeLevel();

        Serial.printf("Envelope #%d (Pin A%d): MAX input, Btn0.\n", i, pins[i]);
        waitForButtonPress();
        envelopeFollowers[i].update();
        int vmax = envelopeFollowers[i].getEnvelopeLevel();

        int delta = vmax - vmin;
        bool pass = delta >= ENV_RANGE_MIN;
        Serial.printf("Envelope #%d range: MIN=%d MAX=%d Δ=%d [%s]\n", i, vmin, vmax, delta,
                      pass ? "PASS" : "FAIL");
    }
    Serial.println("EnvelopeFollower test done.");
}

void testControlSuite() {
    Serial.println("\n--- On-Board Control Suite Test ---");
    Serial.println("Validates config mode, clock source toggle, and LFO quick-tune mode.");

    const uint8_t slotBefore = activePot;
    const uint8_t ccBefore = configManager.getPotCCNumber(slotBefore);
    const bool clockFollowBefore = g_followExternalClock;
    bool configEntered = false;
    bool configEdited = false;
    bool configExited = false;
    bool clockToggled = false;
    bool lfoEntered = false;
    bool lfoTargetCycled = false;
    bool lfoExited = false;

    profileRuntimeRequests.clear();

    drawInstruction("Control Suite", "Ctrl0+2+3+5", "Enter Config");
    unsigned long deadline = millis() + kControlSuiteTimeoutMs;
    while (millis() < deadline) {
        serviceControlSuiteUi();
        if (buttonManager.isOnDeviceConfigModeActive()) {
            configEntered = true;
            break;
        }
    }
    Serial.printf("Config mode enter: %s\n", configEntered ? "PASS" : "FAIL");

    if (configEntered) {
        waitForButtonPress("OLED shows Config view? Btn0");

        drawInstruction("Config Edit", "Tap Ctrl4", "Change CC/D1");
        deadline = millis() + kControlSuiteTimeoutMs;
        while (millis() < deadline) {
            serviceControlSuiteUi();
            const uint8_t ccNow = configManager.getPotCCNumber(activePot);
            if (ccNow != ccBefore) {
                configEdited = true;
                break;
            }
        }
        Serial.printf("Config edit detected: %s\n", configEdited ? "PASS" : "FAIL");

        drawInstruction("Config Exit", "Tap Ctrl5", "Exit+Save");
        deadline = millis() + kControlSuiteTimeoutMs;
        while (millis() < deadline) {
            serviceControlSuiteUi();
            if (!buttonManager.isOnDeviceConfigModeActive()) {
                configExited = true;
                break;
            }
        }
        Serial.printf("Config mode exit: %s\n", configExited ? "PASS" : "FAIL");
        Serial.printf("Config autosave request: %s\n",
                      (configEdited && profileRuntimeRequests.savePending()) ? "PASS" : "FAIL");
    }

    drawInstruction("Clock Source", "Ctrl1+4+5", "Toggle source");
    deadline = millis() + kControlSuiteTimeoutMs;
    while (millis() < deadline) {
        serviceControlSuiteUi();
        if (g_followExternalClock != clockFollowBefore) {
            clockToggled = true;
            break;
        }
    }
    Serial.printf("Clock source toggle: %s\n", clockToggled ? "PASS" : "FAIL");

    drawInstruction("LFO Tune", "Ctrl0+1+3", "Enter mode");
    deadline = millis() + kControlSuiteTimeoutMs;
    while (millis() < deadline) {
        serviceControlSuiteUi();
        if (buttonManager.isLfoTuningModeActive()) {
            lfoEntered = true;
            break;
        }
    }
    Serial.printf("LFO mode enter: %s\n", lfoEntered ? "PASS" : "FAIL");

    if (lfoEntered) {
        waitForButtonPress("OLED shows LFO view? Btn0");

        LFOInternalTarget beforeTarget = LFOInternalTarget::EfGainTrim;
        const bool hadTargetBefore =
            activeInternalTargetForLfo(buttonManager.lfoTuningIndex(), beforeTarget);
        drawInstruction("LFO Route", "Tap Ctrl4", "Cycle target");
        deadline = millis() + kControlSuiteTimeoutMs;
        while (millis() < deadline) {
            serviceControlSuiteUi();
            LFOInternalTarget currentTarget = LFOInternalTarget::EfGainTrim;
            if (!activeInternalTargetForLfo(buttonManager.lfoTuningIndex(), currentTarget)) {
                continue;
            }
            if (!hadTargetBefore || currentTarget != beforeTarget) {
                lfoTargetCycled = true;
                break;
            }
        }
        Serial.printf("LFO target cycle: %s\n", lfoTargetCycled ? "PASS" : "FAIL");

        drawInstruction("LFO Exit", "Tap Ctrl5", "Exit tune");
        deadline = millis() + kControlSuiteTimeoutMs;
        while (millis() < deadline) {
            serviceControlSuiteUi();
            if (!buttonManager.isLfoTuningModeActive()) {
                lfoExited = true;
                break;
            }
        }
        Serial.printf("LFO mode exit: %s\n", lfoExited ? "PASS" : "FAIL");
    }

    drawInstruction("Control Suite", "Review Serial", "Btn0 continue");
    waitForButtonPress("Control suite summary reviewed? Btn0");
}

void testDisplayManager() {
    Serial.println("\n--- DisplayManager Test ---");
    displayManager.clear();
    displayManager.showText("Display Test", "Line 2", "Line 3");
    Serial.println("Verify OLED shows 'Display Test', 'Line 2', 'Line 3'. Press Btn0.");
    waitForButtonPress();
    displayManager.clear();
    Serial.println("DisplayManager test done.");
}

void testSystemSuite() {
    Serial.println("\n--- System Test Suite ---");
    displayManager.showText("System Tests", "running...");
    SystemTestSummary summary = runSystemTests();
    char line1[32], line2[32];
    snprintf(line1, sizeof(line1), "Total %u", summary.total);
    snprintf(line2, sizeof(line2), "Fail %u", summary.failed);
    displayManager.showText(summary.failed ? "System FAIL" : "System PASS", line1, line2);
    Serial.printf("System tests: %u total, %u failed\n", summary.total, summary.failed);
    delay(1000);
    displayManager.clear();
}

// --- Setup & Loop ---
void setup() {
    Serial.begin(SERIAL_BAUD);
    while (!Serial)
        ;
    delay(250);
    Serial.println("\n=== MOARkNOBS Unit Test ===");
    Serial.printf("NUM_LEDS=%u SLOT_LED_COUNT=%u EF_LED_COUNT=%u POT_LED_COUNT=%u\n", NUM_LEDS(),
                  hwConfig.slotLedCount, hwConfig.efLedCount, hwConfig.potLedCount);

    configManager.begin(potChannels);
    configManager.loadMIDISlots(&configManager.getSlot(0), NUM_SLOTS);

    midiHandler.begin();
    midiHandler.setDiagnostics(&g_systemDiagnostics);
    midiHandler.setDisplayManager(&displayManager);

    ledManager.begin();
    displayManager.begin();
    potentiometerManager.loadFromEEPROM();
    buttonManager.initButtons();

    pinMode(potMuxAnalogPin, INPUT);
    pinMode(buttonMuxAnalogPin, INPUT);
    pinMode(phaseButtonPin, INPUT_PULLUP);

    Serial.println("\nPress Btn0 to start tests");
    displayManager.showText("Test Mode", "Press Btn0");
}

static const char *phaseName(TestPhase p) {
    switch (p) {
    case TestPhase::LEDS:
        return "LEDs";
    case TestPhase::BUTTONS:
        return "Buttons";
    case TestPhase::POTS:
        return "Pots";
    case TestPhase::ENVELOPES:
        return "Envelopes";
    case TestPhase::CONTROL:
        return "Controls";
    case TestPhase::DISP:
        return "Display";
    case TestPhase::SYSTEM:
        return "System";
    default:
        return "";
    }
}

void runPhase(TestPhase phase) {
    Serial.printf("\n-- %s Test --\n", phaseName(phase));
    displayManager.showText(phaseName(phase), "running...");
    switch (phase) {
    case TestPhase::LEDS:
        testLEDManager();
        break;
    case TestPhase::BUTTONS:
        testButtonManager();
        break;
    case TestPhase::POTS:
        testPotentiometerManager();
        testFilterPots();
        break;
    case TestPhase::ENVELOPES:
        testEnvelopeFollowers();
        break;
    case TestPhase::CONTROL:
        testControlSuite();
        break;
    case TestPhase::DISP:
        testDisplayManager();
        break;
    case TestPhase::SYSTEM:
        testSystemSuite();
        break;
    default:
        break;
    }
    Serial.printf("%s phase complete.\n", phaseName(phase));
    displayManager.showText(phaseName(phase), "DONE", "Btn0 next");
}

void loop() {
    static bool lastBtn = true;
    bool pressed = digitalRead(phaseButtonPin) == LOW;
    if (pressed && !lastBtn) {
        if (currentPhase == TestPhase::IDLE)
            currentPhase = TestPhase::LEDS;
        else if (currentPhase == TestPhase::SYSTEM)
            currentPhase = TestPhase::COMPLETE;
        else if (currentPhase == TestPhase::COMPLETE)
            currentPhase = TestPhase::IDLE;
        else
            currentPhase = (TestPhase)((int)currentPhase + 1);
        phaseStarted = false;
    }
    lastBtn = pressed;

    if (!phaseStarted) {
        if (currentPhase == TestPhase::IDLE) {
            displayManager.showText("Test Mode", "Press Btn0");
        } else if (currentPhase == TestPhase::COMPLETE) {
            Serial.println("All tests complete");
            displayManager.showText("All tests", "complete", "Btn0 reset");
        } else {
            runPhase(currentPhase);
        }
        phaseStarted = true;
    }
}
