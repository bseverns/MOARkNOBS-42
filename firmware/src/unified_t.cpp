/*
MOARkNOBS Unified Hardware Test

Runs an end-to-end check of LEDs, buttons, pots, envelope followers
and the OLED display immediately at boot. Like the production firmware,
it walks through protocol/modes/UI/runtime layers sequentially: the test
harness wires the same managers that `FirmwareState.cpp` exposes so you can
describe how scheduler tasks would normally pick up the slack.

Build and upload with PlatformIO environment `teensy40_unified_test`
(e.g. `platformio run -e teensy40_unified_test -t upload`).
Requires a Teensy 4.0 wired with the MOARkNOBS hardware.

See `firmware/README.md` under "Test Philosophy (and Real Talk)"
for context and the list of available tests.
*/

#include <Arduino.h>
#include <map>
#include <imxrt.h>
#include <ArduinoJson.h>
#include "Globals.h"
#include "version.h"
#include "Utility.h"
#include "Arpeggiator.h"
#include "BiquadFilter.h"
#include "ConfigManager.h"
#include "LEDManager.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "PotentiometerManager.h"
#include "EnvelopeFollower.h"
#include "LFO/LFOManager.h"
#include "TestHelpers.h"
#include "sys/report.h"

static_assert(NUM_BUTTONS == 6, "expect six control buttons");

extern ConfigManager configManager;
extern DisplayManager displayManager;
extern ButtonManager buttonManager;
extern LFOManager lfoManager;

namespace {
const CRGB kBringUpLedTestColor = CRGB::Green;
constexpr unsigned long kControlSuiteTimeoutMs = 20000;

void showBringUpLed(LEDManager &manager, uint16_t index) {
    manager.setColor(CRGB::Black);
    manager.setPixelColor(index, kBringUpLedTestColor);
    manager.update();
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

void drawOnDeviceConfigView(ButtonManagerContext &context) {
    const uint8_t slotIndex = context.activePot;
    const MIDISlot &slot = context.configManager.getSlot(slotIndex);
    const uint8_t channel = context.configManager.getPotChannel(slotIndex);
    uint8_t data1 = context.configManager.getPotCCNumber(slotIndex);
    if (slot.type == MIDIMessageType::NRPN || slot.type == MIDIMessageType::RPN) {
        data1 = context.configManager.getSlotData1(slotIndex);
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
} // namespace

// --- Board objects ---
// potChannels just holds the loaded EEPROM channels
std::vector<uint8_t> potChannels;

// EEPROM-backed configuration
ConfigManager configManager = createConfigManager();

// LED & display
LEDManager ledManager = createLEDManager();
DisplayManager displayManager = createDisplayManager();

// Mux-1 (U3) for pots + control buttons:
PotentiometerManager potentiometerManager = createPotentiometerManager();

// Mux-0 (U2) for your “virtual slot” buttons:
// Pin 6 reserved for LED strip
// Control buttons wired directly to GPIOs, avoid mux select pins
ButtonManager buttonManager = createButtonManager(&potentiometerManager);

// Envelope followers (unchanged)
std::vector<EnvelopeFollower> envelopeFollowers = createEnvelopeFollowers(&potentiometerManager);

Arpeggiator arpeggiator; // Test stub
LFOManager lfoManager;   // ButtonManager's LFO quick-tune path depends on this symbol.

// ———————— Helpers ——————————————

static void runBulkConfigSelfTest() {
    Utility::BulkConfigAssembler assembler;
    String error;

    assembler.ingestChunk("{\"seq\":7,\"checksum\":\"loopback\",\"config\":", error);
    assembler.ingestChunk("{\"slots\":[]}}", error);
    StaticJsonDocument<256> doc;
    auto parseErr = deserializeJson(doc, assembler.payload());
    if (parseErr) {
        Serial.print("SELFTEST_ACK_ERROR:");
        Serial.println(parseErr.c_str());
    } else {
        uint32_t seq = doc["seq"].as<uint32_t>();
        const char *checksum = doc["checksum"].as<const char *>();
        Serial.print("SELFTEST_ACK:");
        Serial.println(Utility::formatAck(checksum, seq));
    }

    assembler.reset();
    String giant;
    giant.reserve(Utility::kMaxBulkConfigSize + 2);
    giant = "{";
    for (size_t i = 0; i <= Utility::kMaxBulkConfigSize; ++i) {
        giant += 'x';
    }
    if (!assembler.ingestChunk(giant, error)) {
        Serial.print("SELFTEST_OVERFLOW:");
        Serial.println(error);
    } else {
        Serial.println("SELFTEST_OVERFLOW:missed");
    }
}

bool waitForAnyButton(const char *prompt = "Press any button to continue...") {
    Serial.println(prompt);
    while (true) {
        // scan U2’s COM
        for (uint8_t b = 0; b < NUM_VIRTUAL_BUTTONS; ++b) {
            uint8_t row = b / 8, col = b % 8;
            for (int i = 0; i < PRIMARY_MUX_PINS; i++)
                digitalWrite(primaryMuxPins[i], (row >> i) & 1);
            for (int i = 0; i < SECONDARY_MUX_PINS; i++)
                digitalWrite(secondaryMuxPins[i], (col >> i) & 1);
            delayMicroseconds(5);
            if (analogRead(buttonMuxAnalogPin) < 512)
                return true;
        }
        // direct-wired control buttons (active LOW)
        for (uint8_t i = 0; i < NUM_CONTROL_BUTTONS; ++i) {
            if (!digitalRead(TEST_CONTROL_PINS[i]))
                return true;
        }
        delay(5);
    }
}

// Non-blocking check for any button press
static bool anyButtonPressed() {
    // scan U2’s COM
    for (uint8_t b = 0; b < NUM_VIRTUAL_BUTTONS; ++b) {
        uint8_t row = b / 8, col = b % 8;
        for (int i = 0; i < PRIMARY_MUX_PINS; i++)
            digitalWrite(primaryMuxPins[i], (row >> i) & 1);
        for (int i = 0; i < SECONDARY_MUX_PINS; i++)
            digitalWrite(secondaryMuxPins[i], (col >> i) & 1);
        delayMicroseconds(5);
        if (analogRead(buttonMuxAnalogPin) < 512)
            return true;
    }
    // direct-wired control buttons (active LOW)
    for (uint8_t i = 0; i < NUM_CONTROL_BUTTONS; ++i) {
        if (!digitalRead(TEST_CONTROL_PINS[i]))
            return true;
    }
    return false;
}

static const char *resetCauseToString(uint32_t cause) {
    if (cause & 0x01)
        return "Power-on";
    if (cause & 0x02)
        return "Reset pin";
    if (cause & 0x04)
        return "Watchdog";
    if (cause & 0x08)
        return "JTAG";
    if (cause & 0x10)
        return "Software";
    if (cause & 0x20)
        return "Lockup";
    if (cause & 0x40)
        return "Brown-out";
    return "Unknown";
}

// ———————— Tests ——————————————

// Walk each set of LEDs in turn, then ask the human if the show looked right.
void testLEDs() {
    Serial.println("=== LED Test ===");
    const uint8_t previousBrightness = ledManager.getBrightness();
    ledManager.setBrightness(MN42_SAFE_BENCH_LED_BRIGHTNESS);

    // Slot LEDs walk one at a time in a single low-current test color.
    for (int i = 0; i < SLOT_LED_COUNT; ++i) {
        showBringUpLed(ledManager, static_cast<uint16_t>(i));
    }
    displayManager.showText("LED Test", "Slots OK?", "Hit btn");
    waitForAnyButton();

    // Envelope follower indicators use the same fixed color and brightness.
    for (int i = 0; i < EF_LED_COUNT; ++i) {
        showBringUpLed(ledManager, static_cast<uint16_t>(EF_LED_OFFSET() + i));
    }
    displayManager.showText("LED Test", "Followers OK?", "Hit btn");
    waitForAnyButton();

    // Control LED gets the same direct single-pixel check.
    showBringUpLed(ledManager, CONTROL_LED_INDEX());
    displayManager.showText("LED Test", "Ctrl LED?", "Hit btn");
    waitForAnyButton();

    // Pot halo indicators stay on the same low-current path.
    for (int i = 0; i < POT_LED_COUNT; ++i) {
        showBringUpLed(ledManager, static_cast<uint16_t>(POT_LED_OFFSET() + i));
    }
    displayManager.showText("LED Test", "Halos OK?", "Hit btn");
    waitForAnyButton();

    ledManager.setColor(CRGB::Black);
    ledManager.update();
    ledManager.setBrightness(previousBrightness);
    displayManager.clear();
}

void testButtons() {
    Serial.println("=== Button Test ===");
    displayManager.showText("Button Test", "See Serial...");
    // Virtual slots via U2
    for (uint8_t b = 0; b < NUM_VIRTUAL_BUTTONS; ++b) {
        Serial.printf("Press V-Button #%u now...\n", b);
        while (true) {
            uint8_t row = b / 8, col = b % 8;
            for (int i = 0; i < PRIMARY_MUX_PINS; i++)
                digitalWrite(primaryMuxPins[i], (row >> i) & 1);
            for (int i = 0; i < SECONDARY_MUX_PINS; i++)
                digitalWrite(secondaryMuxPins[i], (col >> i) & 1);
            delayMicroseconds(5);
            if (analogRead(buttonMuxAnalogPin) < 512)
                break;
        }
        Serial.printf("  Detected slot %u OK.\n", b);
        delay(200);
    }
    // Direct-wired control buttons
    for (uint8_t i = 0; i < NUM_CONTROL_BUTTONS; ++i) {
        Serial.printf("Press C-Button #%u (pin %u)...\n", i, TEST_CONTROL_PINS[i]);
        while (digitalRead(TEST_CONTROL_PINS[i]))
            ;
        Serial.printf("  Detected control %u OK.\n", i);
        delay(200);
    }
    displayManager.clear();
}

void testPots() {
    Serial.println("=== Potentiometer Test ===");
    displayManager.showText("Pot Test", "See Serial...");

    // Context required for scanning the control pots via ButtonManager
    uint8_t dummyPot = 0, dummyChannel = 1;
    bool dummyEnvFollow = false;
    const char *dummyEnvMode = "";
    std::map<int, MIDISlot::EfSettings> dummyMap;
    bool diag = false;
    uint8_t diagPage = 0;
    ButtonManagerContext bmCtx = {potChannels,       dummyPot,      dummyChannel, dummyEnvFollow,
                                  dummyEnvMode,      configManager, ledManager,   displayManager,
                                  envelopeFollowers, dummyMap,      diag,         diagPage};

    char label[24];
    char result[64];

    // --- Main control pot (MUX A index 0) ---
    snprintf(label, sizeof(label), "Main→MIN");
    Serial.println("\nMain Pot: twist to MIN, then hit a button.");
    displayManager.showText("Pot Test", label);
    waitForAnyButton();
    int mainMin = potentiometerManager.readRawPot(0);

    snprintf(label, sizeof(label), "Main→MAX");
    Serial.println("Main Pot: twist to MAX, then hit a button.");
    displayManager.showText("Pot Test", label);
    waitForAnyButton();
    int mainMax = potentiometerManager.readRawPot(0);

    int delta = mainMax - mainMin;
    bool pass = (delta >= POT_RANGE_MIN);
    snprintf(result, sizeof(result), "Min=%d Max=%d Δ=%d", mainMin, mainMax, delta);
    Serial.printf("  %s\n", result);
    displayManager.showText(pass ? "Main PASS" : "Main FAIL", result);
    delay(800);
    displayManager.clear();

    // --- Filter frequency pot ---
    snprintf(label, sizeof(label), "Freq→MIN");
    Serial.println("\nFreq Pot: roll to MIN, button when ready.");
    displayManager.showText("Pot Test", label);
    waitForAnyButton();
    buttonManager.processButtons(bmCtx);
    int freqMin = buttonManager.getControlPotValue(1);

    snprintf(label, sizeof(label), "Freq→MAX");
    Serial.println("Freq Pot: roll to MAX, button when ready.");
    displayManager.showText("Pot Test", label);
    waitForAnyButton();
    buttonManager.processButtons(bmCtx);
    int freqMax = buttonManager.getControlPotValue(1);

    delta = freqMax - freqMin;
    pass = (delta >= POT_RANGE_MIN);
    snprintf(result, sizeof(result), "Min=%d Max=%d Δ=%d", freqMin, freqMax, delta);
    Serial.printf("  %s\n", result);
    displayManager.showText(pass ? "Freq PASS" : "Freq FAIL", result);
    delay(800);
    displayManager.clear();

    // --- Filter Q pot ---
    snprintf(label, sizeof(label), "Q→MIN");
    Serial.println("\nQ Pot: drop to MIN, button when ready.");
    displayManager.showText("Pot Test", label);
    waitForAnyButton();
    buttonManager.processButtons(bmCtx);
    int qMin = buttonManager.getControlPotValue(2);

    snprintf(label, sizeof(label), "Q→MAX");
    Serial.println("Q Pot: push to MAX, button when ready.");
    displayManager.showText("Pot Test", label);
    waitForAnyButton();
    buttonManager.processButtons(bmCtx);
    int qMax = buttonManager.getControlPotValue(2);

    delta = qMax - qMin;
    pass = (delta >= POT_RANGE_MIN);
    snprintf(result, sizeof(result), "Min=%d Max=%d Δ=%d", qMin, qMax, delta);
    Serial.printf("  %s\n", result);
    displayManager.showText(pass ? "Q PASS" : "Q FAIL", result);
    delay(800);
    displayManager.clear();
}

void testFilterPots() {
    Serial.println("=== Filter Filter Pots ===");
    Serial.println("Sweep Freq/Q pots then press any button.");
    float minFreq = 1e6, maxFreq = 0;
    float minQ = 10, maxQ = 0;
    while (!anyButtonPressed()) {
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
    while (anyButtonPressed())
        delay(10);
    bool freqPass = (minFreq <= 30 && maxFreq >= 4900);
    bool qPass = (minQ <= 0.6 && maxQ >= 3.9);
    char line1[32], line2[32];
    snprintf(line1, sizeof(line1), "F %.0f-%.0f %s", minFreq, maxFreq, freqPass ? "OK" : "BAD");
    snprintf(line2, sizeof(line2), "Q %.1f-%.1f %s", minQ, maxQ, qPass ? "OK" : "BAD");
    Serial.printf("Freq range: %.1f-%.1f Hz [%s]\n", minFreq, maxFreq, freqPass ? "PASS" : "FAIL");
    Serial.printf("Q range: %.1f-%.1f [%s]\n", minQ, maxQ, qPass ? "PASS" : "FAIL");
    displayManager.showText("Filter Pots", line1, line2);
    delay(1000);
    displayManager.clear();
}

void testEnvelopes() {
    Serial.println("=== Envelope Test ===");
    for (size_t i = 0; i < envelopeFollowers.size(); ++i) {
        envelopeFollowers[i].toggleActive(true);
        int pin = (int[]){A0, A1, A2, A3, A6, A7}[i];
        char msg[32];
        snprintf(msg, sizeof(msg), "EF pin %d", pin);

        Serial.printf("EF %u: set MIN, any button.\n", i);
        displayManager.showText("Env Test", msg, "MIN → press");
        waitForAnyButton();
        envelopeFollowers[i].update();
        int vmin = envelopeFollowers[i].getEnvelopeLevel();

        Serial.println("Set MAX, any button.");
        displayManager.showText("Env Test", msg, "MAX → press");
        waitForAnyButton();
        envelopeFollowers[i].update();
        int vmax = envelopeFollowers[i].getEnvelopeLevel();

        int delta = vmax - vmin;
        bool pass = (delta >= ENV_RANGE_MIN);

        char result[64];
        snprintf(result, sizeof(result), "Min=%d Max=%d Δ=%d", vmin, vmax, delta);
        Serial.printf("  %s\n", result);
        displayManager.showText(pass ? "Env PASS" : "Env FAIL", result);
        delay(800);
        displayManager.clear();
    }
}

void testControlSuite() {
    Serial.println("=== On-Board Control Suite ===");
    Serial.println("Exercises config mode, clock source toggle, and LFO quick-tune.");

    bool envelopeFollowMode = false;
    const char *envelopeMode = "";
    std::map<int, MIDISlot::EfSettings> potToEnvelopeMap;
    bool diagnosticMode = false;
    uint8_t diagnosticPage = 0;
    uint8_t activePot = 0;
    uint8_t activeChannel = 1;
    ButtonManagerContext context = {potChannels,        activePot,      activeChannel,
                                    envelopeFollowMode, envelopeMode,   configManager,
                                    ledManager,         displayManager, envelopeFollowers,
                                    potToEnvelopeMap,   diagnosticMode, diagnosticPage};

    auto serviceUi = [&]() {
        buttonManager.processButtons(context);
        if (buttonManager.isOnDeviceConfigModeActive()) {
            drawOnDeviceConfigView(context);
        } else if (buttonManager.isLfoTuningModeActive()) {
            drawLfoTuningView();
        }
        delay(2);
    };

    const uint8_t slotBefore = context.activePot;
    const uint8_t ccBefore = configManager.getPotCCNumber(slotBefore);
    const bool clockFollowBefore = g_followExternalClock;
    bool configEntered = false;
    bool configEdited = false;
    bool configExited = false;
    bool clockToggled = false;
    bool lfoEntered = false;
    bool lfoTargetCycled = false;
    bool lfoExited = false;

    g_profileSaveRequested = false;

    drawInstruction("Control Suite", "Ctrl0+2+3+5", "Enter Config");
    unsigned long deadline = millis() + kControlSuiteTimeoutMs;
    while (millis() < deadline) {
        serviceUi();
        if (buttonManager.isOnDeviceConfigModeActive()) {
            configEntered = true;
            break;
        }
    }
    Serial.printf("Config mode enter: %s\n", configEntered ? "PASS" : "FAIL");

    if (configEntered) {
        drawInstruction("Cfg OLED", "Slot/Type/Ch", "Any btn=OK");
        waitForAnyButton("Verify Config OLED, then any button.");

        drawInstruction("Config Edit", "Tap Ctrl4", "Change CC/D1");
        deadline = millis() + kControlSuiteTimeoutMs;
        while (millis() < deadline) {
            serviceUi();
            if (configManager.getPotCCNumber(context.activePot) != ccBefore) {
                configEdited = true;
                break;
            }
        }
        Serial.printf("Config edit detected: %s\n", configEdited ? "PASS" : "FAIL");

        drawInstruction("Config Exit", "Tap Ctrl5", "Exit+Save");
        deadline = millis() + kControlSuiteTimeoutMs;
        while (millis() < deadline) {
            serviceUi();
            if (!buttonManager.isOnDeviceConfigModeActive()) {
                configExited = true;
                break;
            }
        }
        Serial.printf("Config mode exit: %s\n", configExited ? "PASS" : "FAIL");
        Serial.printf("Config autosave flag: %s\n",
                      (configEdited && g_profileSaveRequested) ? "PASS" : "FAIL");
    }

    drawInstruction("Clock Source", "Ctrl1+4+5", "Toggle source");
    deadline = millis() + kControlSuiteTimeoutMs;
    while (millis() < deadline) {
        serviceUi();
        if (g_followExternalClock != clockFollowBefore) {
            clockToggled = true;
            break;
        }
    }
    Serial.printf("Clock source toggle: %s\n", clockToggled ? "PASS" : "FAIL");

    drawInstruction("LFO Tune", "Ctrl0+1+3", "Enter mode");
    deadline = millis() + kControlSuiteTimeoutMs;
    while (millis() < deadline) {
        serviceUi();
        if (buttonManager.isLfoTuningModeActive()) {
            lfoEntered = true;
            break;
        }
    }
    Serial.printf("LFO mode enter: %s\n", lfoEntered ? "PASS" : "FAIL");

    if (lfoEntered) {
        drawInstruction("LFO OLED", "Target/Shape", "Any btn=OK");
        waitForAnyButton("Verify LFO OLED, then any button.");

        LFOInternalTarget beforeTarget = LFOInternalTarget::EfGainTrim;
        const bool hadTargetBefore =
            activeInternalTargetForLfo(buttonManager.lfoTuningIndex(), beforeTarget);
        drawInstruction("LFO Route", "Tap Ctrl4", "Cycle target");
        deadline = millis() + kControlSuiteTimeoutMs;
        while (millis() < deadline) {
            serviceUi();
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
            serviceUi();
            if (!buttonManager.isLfoTuningModeActive()) {
                lfoExited = true;
                break;
            }
        }
        Serial.printf("LFO mode exit: %s\n", lfoExited ? "PASS" : "FAIL");
    }

    drawInstruction("Control Suite", "Review Serial", "Any btn");
    waitForAnyButton("Control suite summary reviewed? Any button.");
}

void testDisplay() {
    Serial.println("=== Display Test ===");
    displayManager.clear();
    displayManager.showText("Disp Test", "L2", "L3");
    Serial.println("Check 3 lines. Press any button.");
    waitForAnyButton();
    displayManager.clear();
    displayManager.showText("Done", "", "");
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    while (!Serial)
        ;
    delay(200);

    // boot-time banner and brownout sniff test
    g_resetCause = SRC_SRSR;
    ConfigManager::getStorageBackend()->readBytes(EEPROM_BROWNOUT_COUNT, &g_brownoutCount,
                                                  sizeof(g_brownoutCount));
    if (g_brownoutCount == 0xFFFF) {
        g_brownoutCount = 0;
        ConfigManager::getStorageBackend()->writeBytes(EEPROM_BROWNOUT_COUNT, &g_brownoutCount,
                                                       sizeof(g_brownoutCount));
    }
    if (g_resetCause & 0x40) {
        g_brownoutCount++;
        ConfigManager::getStorageBackend()->writeBytes(EEPROM_BROWNOUT_COUNT, &g_brownoutCount,
                                                       sizeof(g_brownoutCount));
    }
    Serial.printf("MN42 FW %s schema %04X UID %08lX%08lX%08lX%08lX\n", FW_VERSION_STR,
                  CONFIG_VERSION, HW_OCOTP_CFG0, HW_OCOTP_CFG1, HW_OCOTP_CFG2, HW_OCOTP_CFG3);
    Serial.printf("Reset 0x%08lX (%s) Brownouts %u\n", g_resetCause,
                  resetCauseToString(g_resetCause), g_brownoutCount);
    sys::printReport();
    runBulkConfigSelfTest();

    // inits
    configManager.begin(potChannels);
    ledManager.begin();
    displayManager.begin();
    potentiometerManager.loadFromEEPROM();
    buttonManager.initButtons();

    for (auto p : primaryMuxPins)
        pinMode(p, OUTPUT);
    for (auto p : secondaryMuxPins)
        pinMode(p, OUTPUT);
    pinMode(potMuxAnalogPin, INPUT);
    pinMode(buttonMuxAnalogPin, INPUT);
    for (uint8_t c : TEST_CONTROL_PINS)
        pinMode(c, INPUT_PULLUP);

    Serial.println("\n=== MOARkNOBS HW Test ===");
    Serial.printf("NUM_LEDS=%u SLOT_LED_COUNT=%u EF_LED_COUNT=%u POT_LED_COUNT=%u\n", NUM_LEDS(),
                  hwConfig.slotLedCount, hwConfig.efLedCount, hwConfig.potLedCount);

    testLEDs();
    testButtons();
    testPots();
    testFilterPots();
    testEnvelopes();
    testControlSuite();
    testDisplay();

    Serial.println("ALL TESTS COMPLETE.");
    displayManager.showText("All tests", "COMPLETE!");
}

void loop() {}
