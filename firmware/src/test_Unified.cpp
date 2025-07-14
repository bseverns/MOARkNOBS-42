// unified.cpp

#include <Arduino.h>
#include "Globals.h"
#include "Utility.h"
#include "BiquadFilter.h"
#include "ConfigManager.h"
#include "LEDManager.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "PotentiometerManager.h"
#include "EnvelopeFollower.h"
#include "TestHelpers.h"

#define SERIAL_BAUD 115200

// --- Board objects ---
// potChannels just holds the loaded EEPROM channels
std::vector<uint8_t> potChannels;

// EEPROM-backed configuration
ConfigManager configManager = createConfigManager();

// LED & display
LEDManager    ledManager    = createLEDManager();
DisplayManager displayManager = createDisplayManager();

// Mux-1 (U3) for pots + control buttons:
PotentiometerManager potentiometerManager = createPotentiometerManager();

// Mux-0 (U2) for your “virtual slot” buttons:
// Pin 6 reserved for LED strip
// Control buttons wired directly to GPIOs, avoid mux select pins
ButtonManager buttonManager = createButtonManager(&potentiometerManager);

// Envelope followers (unchanged)
std::vector<EnvelopeFollower> envelopeFollowers = createEnvelopeFollowers(&potentiometerManager);

// ———————— Helpers ——————————————

bool waitForAnyButton(const char* prompt = "Press any button to continue...")
{
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
      if (analogRead(buttonMuxAnalogPin) < 512) return true;
    }
    // direct-wired control buttons (active LOW)
    for (uint8_t i = 0; i < NUM_CONTROL_BUTTONS; ++i) {
      if (! digitalRead(TEST_CONTROL_PINS[i])) return true;
    }
    delay(5);
  }
}

// ———————— Tests ——————————————

void testLEDs() {
  Serial.println("=== LED Test ===");
  for (int i = 0; i < NUM_LEDS; ++i) {
    ledManager.setColor(CRGB::Black);
    ledManager.setPotValue(i, 127);
    Serial.printf("LED %d ON? Press any button if lit.\n", i);
    displayManager.showText("LED Test", ("LED #" + String(i)).c_str());
    waitForAnyButton();
  }
  ledManager.setColor(CRGB::Black);
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
      if (analogRead(buttonMuxAnalogPin) < 512) break;
    }
    Serial.printf("  Detected slot %u OK.\n", b);
    delay(200);
  }
  // Direct-wired control buttons
  for (uint8_t i = 0; i < NUM_CONTROL_BUTTONS; ++i) {
    Serial.printf("Press C-Button #%u (pin %u)...\n", i, TEST_CONTROL_PINS[i]);
    while (digitalRead(TEST_CONTROL_PINS[i])) ;
    Serial.printf("  Detected control %u OK.\n", i);
    delay(200);
  }
  displayManager.clear();
}

void testPots() {
  Serial.println("=== Potentiometer Test ===");
  displayManager.showText("Pot Test", "See Serial...");
  for (uint8_t idx = 0; idx < NUM_POTS; ++idx) {
    char label[24];
    sprintf(label, "Pot#%u→MIN", idx);
    Serial.printf("\nPot %u: to MIN, then any button.\n", idx);
    displayManager.showText("Pot Test", label);
    waitForAnyButton();
    int vmin = analogRead(potMuxAnalogPin);

    sprintf(label, "Pot#%u→MAX", idx);
    Serial.printf("Pot %u: to MAX, then any button.\n", idx);
    displayManager.showText("Pot Test", label);
    waitForAnyButton();
    int vmax = analogRead(potMuxAnalogPin);

    int delta = vmax - vmin;
    bool pass = (delta >= POT_RANGE_MIN);

    char result[64];
    sprintf(result, "Min=%d Max=%d Δ=%d", vmin, vmax, delta);
    Serial.printf("  %s\n", result);
    displayManager.showText(pass ? "Pot PASS" : "Pot FAIL", result);
    delay(800);
    displayManager.clear();
  }
}

void testEnvelopes() {
  Serial.println("=== Envelope Test ===");
  for (size_t i = 0; i < envelopeFollowers.size(); ++i) {
    envelopeFollowers[i].toggleActive(true);
    int pin = (int[]){A0,A1,A2,A3,A6,A7}[i];
    char msg[32]; sprintf(msg, "EF pin %d", pin);

    Serial.printf("EF %u: set MIN, any button.\n", i);
    displayManager.showText("Env Test", msg, "MIN → press");
    waitForAnyButton();
    envelopeFollowers[i].update();  int vmin = envelopeFollowers[i].getEnvelopeLevel();

    Serial.println("Set MAX, any button.");
    displayManager.showText("Env Test", msg, "MAX → press");
    waitForAnyButton();
    envelopeFollowers[i].update();  int vmax = envelopeFollowers[i].getEnvelopeLevel();

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
  while (!Serial) ;
  delay(200);

  // inits
  configManager.begin(potChannels);
  ledManager.begin();
  displayManager.begin();
  potentiometerManager.loadFromEEPROM();
  buttonManager.initButtons();

  for (auto p: primaryMuxPins)   pinMode(p, OUTPUT);
  for (auto p: secondaryMuxPins) pinMode(p, OUTPUT);
  pinMode(potMuxAnalogPin, INPUT);
  pinMode(buttonMuxAnalogPin, INPUT);
  for (uint8_t c: TEST_CONTROL_PINS)   pinMode(c, INPUT_PULLUP);

  Serial.println("\n=== MOARkNOBS HW Test ===");

  testLEDs();
  testButtons();
  testPots();
  testEnvelopes();
  testDisplay();

  Serial.println("ALL TESTS COMPLETE.");
  displayManager.showText("All tests","COMPLETE!");
}

void loop() {}
