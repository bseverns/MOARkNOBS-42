/*
 * MOARkNOBS Hardware Smoke Test
 *
 * Validates LEDs, the button matrix, one slot pot plus two filter-tuning pots,
 * envelope followers and the OLED display. Use Control Button #0 to advance
 * through each phase.
 *
 * Build and upload with PlatformIO environment `teensy40_full_system`
 * (e.g. `platformio run -e teensy40_full_system -t upload`).
 * Requires a Teensy 4.0 wired with the full MOARkNOBS hardware
 * (button matrix, LED strip, OLED, envelope circuits).
 *
 * See `firmware/README.md` under "Test Philosophy (and Real Talk)"
 * for additional context and a list of all available test sketches.
 */

#include <Arduino.h>
#include "Globals.h"

// Entry point for a series of manual board tests. Compile this instead of the
// production firmware to verify hardware functionality during assembly.
#include "MIDIHandler.h"
#include "ConfigManager.h"
#include "LEDManager.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "PotentiometerManager.h"
#include "EnvelopeFollower.h"
#include "TestHelpers.h"

static_assert(NUM_BUTTONS == 6, "expect six control buttons");

std::vector<uint8_t> potChannels; // EEPROM-loaded channels

#define SERIAL_BAUD 115200

// Instantiate board objects:
ConfigManager configManager = createConfigManager();
LEDManager    ledManager    = createLEDManager();
MIDIHandler   midiHandler;
DisplayManager displayManager = createDisplayManager();
PotentiometerManager potentiometerManager = createPotentiometerManager();
// Pin 6 reserved for LED strip
ButtonManager buttonManager = createButtonManager(&potentiometerManager);
std::vector<EnvelopeFollower> envelopeFollowers = createEnvelopeFollowers(&potentiometerManager);

uint8_t activePot = 0, activeChannel = 1;
bool envelopeFollowMode = false;
const char* envelopeMode = "SEF";
std::map<int, int> potToEnvelopeMap;

ButtonManagerContext buttonContext = {
    potChannels, activePot, activeChannel,
    envelopeFollowMode, envelopeMode,
    configManager, ledManager,
    displayManager, envelopeFollowers,
    potToEnvelopeMap
};

// Control button used to advance test phases
const uint8_t phaseButtonPin = 12; // Control Button #0

// Ordered test phases
enum class TestPhase { IDLE, LEDS, BUTTONS, POTS, ENVELOPES, DISP, COMPLETE };
TestPhase currentPhase = TestPhase::IDLE;
bool phaseStarted = false;

// --- Utility ---
/** Wait for Control Button #0 to be pressed and released. */
void waitForButtonPress(const char* prompt = "Press Btn0 to continue") {
  Serial.println(prompt);
  displayManager.showText(prompt, "", "Btn0");
  while (digitalRead(phaseButtonPin)) ;      // wait press (active LOW)
  while (!digitalRead(phaseButtonPin)) ;     // wait release
  delay(50);
}

// --- Unit Tests ---
// Cycle each LED cluster and make sure FastLED actually spits bits.
// We don't block on every single diode; instead we run a sweep and ask the
// human if the glow looked good.  Punk rock hardware testing, basically.
void testLEDManager() {
  Serial.println("\n--- LEDManager Test ---");

  // --- Slot LEDs ---
  for (int i = 0; i < SLOT_LED_COUNT; i++) {
    ledManager.setColor(CRGB::Black);
    ledManager.setPotValue(i, 127);
    ledManager.update();
    delay(50);
  }
  waitForButtonPress("Slots glow in order? Btn0");

  // --- Envelope follower LEDs ---
  for (int i = 0; i < EF_LED_COUNT; i++) {
    ledManager.setColor(CRGB::Black);
    ledManager.setEnvelopeLevel(i, 127);
    ledManager.update();
    delay(50);
  }
  waitForButtonPress("Followers shine? Btn0");

  // --- Control button LED ---
  ledManager.setColor(CRGB::Black);
  ledManager.triggerControlButton();
  ledManager.update();
  waitForButtonPress("Control LED pop? Btn0");

  // --- Pot halo LEDs ---
  for (int i = 0; i < POT_LED_COUNT; i++) {
    ledManager.setColor(CRGB::Black);
    ledManager.setPotIndicator(i, 127);
    ledManager.update();
    delay(50);
  }
  waitForButtonPress("Halos look righteous? Btn0");

  ledManager.setColor(CRGB::Black);
  ledManager.update();
  Serial.println("LEDManager test done.");
}

void testButtonManager() {
  Serial.println("\n--- ButtonManager Test ---");
  Serial.println("Press each virtual button (mux matrix) when prompted.");
  for (int b = 0; b < NUM_VIRTUAL_BUTTONS; b++) {
    Serial.printf("Press Virtual Button #%d...\n", b);
    while (!buttonManager.isMuxButtonPressed(b));
    Serial.printf("Button #%d OK!\n", b);
    delay(200);
  }

  Serial.println("Press each physical control button now.");
  // Pin 6 reserved for LED strip
  for (int i = 0; i < NUM_CONTROL_BUTTONS; i++) {
    Serial.printf("Press Control Button #%d (pin %d)...\n", i, TEST_CONTROL_PINS[i]);
    while (digitalRead(TEST_CONTROL_PINS[i]));
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
  Serial.printf("Slot Pot sweep: MIN=%d MAX=%d Δ=%d [%s]\n",
                slotMin, slotMax, slotDelta, slotPass ? "PASS" : "FAIL");

  // --- Filter frequency pot ----------------------------------------------
  Serial.println("Filter Freq: roll to MIN, Btn0.");
  waitForButtonPress();
  int freqMin = buttonManager.getControlPotValue(1);

  Serial.println("Filter Freq: peg it to MAX, Btn0.");
  waitForButtonPress();
  int freqMax = buttonManager.getControlPotValue(1);

  int freqDelta = freqMax - freqMin;
  bool freqPass = freqDelta >= POT_RANGE_MIN;
  Serial.printf("Filter Freq sweep: MIN=%d MAX=%d Δ=%d [%s]\n",
                freqMin, freqMax, freqDelta, freqPass ? "PASS" : "FAIL");

  // --- Filter resonance/Q pot --------------------------------------------
  Serial.println("Filter Q: dive to MIN, Btn0.");
  waitForButtonPress();
  int qMin = buttonManager.getControlPotValue(2);

  Serial.println("Filter Q: hammer to MAX, Btn0.");
  waitForButtonPress();
  int qMax = buttonManager.getControlPotValue(2);

  int qDelta = qMax - qMin;
  bool qPass = qDelta >= POT_RANGE_MIN;
  Serial.printf("Filter Q sweep: MIN=%d MAX=%d Δ=%d [%s]\n",
                qMin, qMax, qDelta, qPass ? "PASS" : "FAIL");

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
    if (freq < minFreq) minFreq = freq;
    if (freq > maxFreq) maxFreq = freq;
    if (q < minQ) minQ = q;
    if (q > maxQ) maxQ = q;
    displayManager.showFilterTuning("Freq", freq, "Q", q);
    Serial.printf("Freq=%.1f Hz Q=%.2f\n", freq, q);
    delay(100);
  }
  while (!digitalRead(phaseButtonPin));
  bool freqPass = (minFreq <= 30 && maxFreq >= 4900);
  bool qPass = (minQ <= 0.6 && maxQ >= 3.9);
  Serial.printf("Freq range: %.1f-%.1f Hz [%s]\n", minFreq, maxFreq, freqPass ? "PASS" : "FAIL");
  Serial.printf("Q range: %.1f-%.1f [%s]\n", minQ, maxQ, qPass ? "PASS" : "FAIL");
  char line1[32], line2[32];
  sprintf(line1, "F %.0f-%.0f %s", minFreq, maxFreq, freqPass ? "OK" : "BAD");
  sprintf(line2, "Q %.1f-%.1f %s", minQ, maxQ, qPass ? "OK" : "BAD");
  displayManager.showText("Filter Pots", line1, line2);
  delay(1000);
  displayManager.clear();
}

void testEnvelopeFollowers() {
  Serial.println("\n--- EnvelopeFollower Test ---");
  int pins[] = {A0,A1,A2,A3,A6,A7};
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
    Serial.printf("Envelope #%d range: MIN=%d MAX=%d Δ=%d [%s]\n", i, vmin, vmax, delta, pass ? "PASS" : "FAIL");
  }
  Serial.println("EnvelopeFollower test done.");
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

// --- Setup & Loop ---
void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial);
  delay(250);
  Serial.println("\n=== MOARkNOBS Unit Test ===");
  Serial.printf("NUM_LEDS=%u SLOT_LED_COUNT=%u EF_LED_COUNT=%u POT_LED_COUNT=%u\n",
                NUM_LEDS(), hwConfig.slotLedCount, hwConfig.efLedCount, hwConfig.potLedCount);

  configManager.begin(potChannels);
  configManager.loadMIDISlots(&configManager.getSlot(0), NUM_SLOTS);

  midiHandler.begin();
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

static const char* phaseName(TestPhase p) {
  switch (p) {
    case TestPhase::LEDS: return "LEDs";
    case TestPhase::BUTTONS: return "Buttons";
    case TestPhase::POTS: return "Pots";
    case TestPhase::ENVELOPES: return "Envelopes";
    case TestPhase::DISP: return "Display";
    default: return "";
  }
}

void runPhase(TestPhase phase) {
  Serial.printf("\n-- %s Test --\n", phaseName(phase));
  displayManager.showText(phaseName(phase), "running...");
  switch (phase) {
    case TestPhase::LEDS:       testLEDManager(); break;
    case TestPhase::BUTTONS:    testButtonManager(); break;
    case TestPhase::POTS:       testPotentiometerManager(); testFilterPots(); break;
    case TestPhase::ENVELOPES:  testEnvelopeFollowers(); break;
    case TestPhase::DISP:    testDisplayManager(); break;
    default: break;
  }
  Serial.printf("%s phase complete.\n", phaseName(phase));
  displayManager.showText(phaseName(phase), "DONE", "Btn0 next");
}

void loop() {
  static bool lastBtn = true;
  bool pressed = digitalRead(phaseButtonPin) == LOW;
  if (pressed && !lastBtn) {
    if (currentPhase == TestPhase::IDLE)          currentPhase = TestPhase::LEDS;
    else if (currentPhase == TestPhase::DISP)  currentPhase = TestPhase::COMPLETE;
    else if (currentPhase == TestPhase::COMPLETE) currentPhase = TestPhase::IDLE;
    else                                          currentPhase = (TestPhase)((int)currentPhase + 1);
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
