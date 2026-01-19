/*
 * MOARkNOBS Unified Hardware Test
 *
 * Runs an end-to-end check of LEDs, buttons, pots, envelope followers
 * and the OLED display immediately at boot. Like the production firmware,
 * it walks through protocol/modes/UI/runtime layers sequentially: the test
 * harness wires the same managers that `FirmwareState.cpp` exposes so you can
 * describe how scheduler tasks would normally pick up the slack.
 *
 * Build and upload with PlatformIO environment `teensy40_unified_test`
 * (e.g. `platformio run -e teensy40_unified_test -t upload`).
 * Requires a Teensy 4.0 wired with the MOARkNOBS hardware.
 *
 * See `firmware/README.md` under "Test Philosophy (and Real Talk)"
 * for context and the list of available tests.
 */

#include <Arduino.h>
#include <map>
#include <EEPROM.h>
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
#include "TestHelpers.h"
#include "sys/report.h"

static_assert(NUM_BUTTONS == 6, "expect six control buttons");

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

    // Slot LEDs get a rainbow chase via pot values
    for (int i = 0; i < SLOT_LED_COUNT; ++i) {
        ledManager.setColor(CRGB::Black);
        ledManager.setPotValue(i, 127);
        ledManager.update();
        delay(50);
    }
    displayManager.showText("LED Test", "Slots OK?", "Hit btn");
    waitForAnyButton();

    // Envelope follower indicators in grayscale
    for (int i = 0; i < EF_LED_COUNT; ++i) {
        ledManager.setColor(CRGB::Black);
        ledManager.setEnvelopeLevel(i, 127);
        ledManager.update();
        delay(50);
    }
    displayManager.showText("LED Test", "Followers OK?", "Hit btn");
    waitForAnyButton();

    // Control button LED flash
    ledManager.setColor(CRGB::Black);
    ledManager.triggerControlButton();
    ledManager.update();
    displayManager.showText("LED Test", "Ctrl LED?", "Hit btn");
    waitForAnyButton();

    // Pot halo indicators
    for (int i = 0; i < POT_LED_COUNT; ++i) {
        ledManager.setColor(CRGB::Black);
        ledManager.setPotIndicator(i, 127);
        ledManager.update();
        delay(50);
    }
    displayManager.showText("LED Test", "Halos OK?", "Hit btn");
    waitForAnyButton();

    ledManager.setColor(CRGB::Black);
    ledManager.update();
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
    sprintf(label, "Main→MIN");
    Serial.println("\nMain Pot: twist to MIN, then hit a button.");
    displayManager.showText("Pot Test", label);
    waitForAnyButton();
    int mainMin = potentiometerManager.readRawPot(0);

    sprintf(label, "Main→MAX");
    Serial.println("Main Pot: twist to MAX, then hit a button.");
    displayManager.showText("Pot Test", label);
    waitForAnyButton();
    int mainMax = potentiometerManager.readRawPot(0);

    int delta = mainMax - mainMin;
    bool pass = (delta >= POT_RANGE_MIN);
    sprintf(result, "Min=%d Max=%d Δ=%d", mainMin, mainMax, delta);
    Serial.printf("  %s\n", result);
    displayManager.showText(pass ? "Main PASS" : "Main FAIL", result);
    delay(800);
    displayManager.clear();

    // --- Filter frequency pot ---
    sprintf(label, "Freq→MIN");
    Serial.println("\nFreq Pot: roll to MIN, button when ready.");
    displayManager.showText("Pot Test", label);
    waitForAnyButton();
    buttonManager.processButtons(bmCtx);
    int freqMin = buttonManager.getControlPotValue(1);

    sprintf(label, "Freq→MAX");
    Serial.println("Freq Pot: roll to MAX, button when ready.");
    displayManager.showText("Pot Test", label);
    waitForAnyButton();
    buttonManager.processButtons(bmCtx);
    int freqMax = buttonManager.getControlPotValue(1);

    delta = freqMax - freqMin;
    pass = (delta >= POT_RANGE_MIN);
    sprintf(result, "Min=%d Max=%d Δ=%d", freqMin, freqMax, delta);
    Serial.printf("  %s\n", result);
    displayManager.showText(pass ? "Freq PASS" : "Freq FAIL", result);
    delay(800);
    displayManager.clear();

    // --- Filter Q pot ---
    sprintf(label, "Q→MIN");
    Serial.println("\nQ Pot: drop to MIN, button when ready.");
    displayManager.showText("Pot Test", label);
    waitForAnyButton();
    buttonManager.processButtons(bmCtx);
    int qMin = buttonManager.getControlPotValue(2);

    sprintf(label, "Q→MAX");
    Serial.println("Q Pot: push to MAX, button when ready.");
    displayManager.showText("Pot Test", label);
    waitForAnyButton();
    buttonManager.processButtons(bmCtx);
    int qMax = buttonManager.getControlPotValue(2);

    delta = qMax - qMin;
    pass = (delta >= POT_RANGE_MIN);
    sprintf(result, "Min=%d Max=%d Δ=%d", qMin, qMax, delta);
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
    sprintf(line1, "F %.0f-%.0f %s", minFreq, maxFreq, freqPass ? "OK" : "BAD");
    sprintf(line2, "Q %.1f-%.1f %s", minQ, maxQ, qPass ? "OK" : "BAD");
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
        sprintf(msg, "EF pin %d", pin);

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
        sprintf(result, "Min=%d Max=%d Δ=%d", vmin, vmax, delta);
        Serial.printf("  %s\n", result);
        displayManager.showText(pass ? "Env PASS" : "Env FAIL", result);
        delay(800);
        displayManager.clear();
    }
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
    EEPROM.get(EEPROM_BROWNOUT_COUNT, g_brownoutCount);
    if (g_brownoutCount == 0xFFFF) {
        g_brownoutCount = 0;
        EEPROM.put(EEPROM_BROWNOUT_COUNT, g_brownoutCount);
    }
    if (g_resetCause & 0x40) {
        g_brownoutCount++;
        EEPROM.put(EEPROM_BROWNOUT_COUNT, g_brownoutCount);
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
    testDisplay();

    Serial.println("ALL TESTS COMPLETE.");
    displayManager.showText("All tests", "COMPLETE!");
}

void loop() {}
