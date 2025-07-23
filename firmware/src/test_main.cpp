/*
 * MOARkNOBS Hardware Smoke Test
 *
 * Validates LEDs, the button matrix, potentiometers, envelope followers and
 * the OLED display. Use Control Button #0 to advance through each phase.
 *
 * Build and upload with PlatformIO environment `teensy40_mainTEST`
 * (e.g. `platformio run -e teensy40_mainTEST -t upload`).
 * Requires a Teensy 4.0 wired with the full MOARkNOBS hardware
 * (button matrix, LED strip, OLED, envelope circuits).
 *
 * See `firmware/README.md` under "Test Philosophy (and Real Talk)"
 * for additional context and a list of all available test sketches.
 */

#include <Arduino.h>
#include "Globals.h"
#include "MIDIHandler.h"
#include "ConfigManager.h"
#include "LEDManager.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "PotentiometerManager.h"
#include "EnvelopeFollower.h"
#include "TestHelpers.h"

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
void testLEDManager() {
  Serial.println("\n--- LEDManager Test ---");
  for (int i = 0; i < NUM_LEDS; i++) {
    ledManager.setColor(CRGB::Black);
    ledManager.setPotValue(i, 127);
    Serial.printf("LED #%d ON? Press Btn0.\n", i);
    waitForButtonPress();
  }
  ledManager.setColor(CRGB::Black);
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
  const uint8_t ctrlPins[] = {12,13,14,15,24,25};
  for (int i = 0; i < NUM_CONTROL_BUTTONS; i++) {
    Serial.printf("Press Control Button #%d (pin %d)...\n", i, ctrlPins[i]);
    while (digitalRead(ctrlPins[i]));
    Serial.printf("Control Button #%d OK!\n", i);
    delay(200);
  }
  Serial.println("ButtonManager test done.");
}

void testPotentiometerManager() {
  Serial.println("\n--- PotentiometerManager Test ---");
  for (int idx = 0; idx < NUM_POTS; idx++) {
    Serial.printf("Pot #%d: set MIN, press Btn0.\n", idx);
    waitForButtonPress();
    int vmin = potentiometerManager.readRawPot(idx);
    
    Serial.printf("Pot #%d: set MAX, press Btn0.\n", idx);
    waitForButtonPress();
    int vmax = potentiometerManager.readRawPot(idx);

    int delta = vmax - vmin;
    bool pass = delta >= POT_RANGE_MIN;
    Serial.printf("Pot #%d range check: MIN=%d MAX=%d Δ=%d [%s]\n", idx, vmin, vmax, delta, pass ? "PASS" : "FAIL");
  }
  Serial.println("PotentiometerManager test done.");
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
    case TestPhase::POTS:       testPotentiometerManager(); break;
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
