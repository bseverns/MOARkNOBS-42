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

#define SERIAL_BAUD 115200

// --- Board objects ---
std::vector<uint8_t> potChannels;
ConfigManager configManager(NUM_POTS, NUM_BUTTONS);
LEDManager ledManager(LED_PIN, NUM_LEDS);
DisplayManager displayManager(SSD1306_I2C_ADDRESS, OLED_WIDTH, OLED_HEIGHT);
PotentiometerManager potentiometerManager(primaryMuxPins, secondaryMuxPins, analogPin);
ButtonManager buttonManager(primaryMuxPins, secondaryMuxPins, analogPin, (const uint8_t[]){2,3,4,5,6,13}, &potentiometerManager);

// Envelope followers for A0, A1, A2, A3, A6, A7
std::vector<EnvelopeFollower> envelopeFollowers = {
  EnvelopeFollower(A0, &potentiometerManager),
  EnvelopeFollower(A1, &potentiometerManager),
  EnvelopeFollower(A2, &potentiometerManager),
  EnvelopeFollower(A3, &potentiometerManager),
  EnvelopeFollower(A6, &potentiometerManager),
  EnvelopeFollower(A7, &potentiometerManager),
};

// Helper: Wait for *any* button press (virtual or control)
bool waitForAnyButton(const char* prompt = "Press any button to continue...")
{
  Serial.println(prompt);
  while (true) {
    for (uint8_t b = 0; b < NUM_VIRTUAL_BUTTONS; ++b) {
      // Mux selection for button scan
      uint8_t row = b / 8, col = b % 8;
      for (int i = 0; i < 3; i++) {
        digitalWrite(primaryMuxPins[i], (row >> i) & 1);
        digitalWrite(secondaryMuxPins[i], (col >> i) & 1);
      }
      delayMicroseconds(5);
      int v = analogRead(analogPin);
      if (v < 512) return true;
    }
    // Control buttons (active LOW)
    for (uint8_t i = 0; i < NUM_CONTROL_BUTTONS; ++i) {
      if (!digitalRead((const uint8_t[]){2,3,4,5,6,13}[i])) return true;
    }
    delay(5);
  }
}

// LED test: Light each LED in sequence, prompt for visual confirmation
void testLEDs() {
  Serial.println("=== LED Test ===");
  for (int i = 0; i < NUM_LEDS; ++i) {
    ledManager.setColor(CRGB::Black); // All off
    ledManager.setPotValue(i, 127);   // Set just one to bright
    Serial.printf("LED %d should be ON. Is it lit? (Press any button if yes)\n", i);
    displayManager.showText("LED Test", ("LED #" + String(i)).c_str());
    waitForAnyButton();
  }
  ledManager.setColor(CRGB::Black); // All off at end
  displayManager.clear();
}

// Button test: Ask user to press each button, confirm detection via Serial
void testButtons() {
  Serial.println("=== Button Test ===");
  displayManager.showText("Button Test", "Follow Serial...");
  // Virtual (muxed) buttons
  for (uint8_t b = 0; b < NUM_VIRTUAL_BUTTONS; ++b) {
    Serial.printf("Press virtual button #%u now...\n", b);
    // Wait until that button is detected pressed
    while (true) {
      uint8_t row = b / 8, col = b % 8;
      for (int i = 0; i < 3; i++) {
        digitalWrite(primaryMuxPins[i], (row >> i) & 1);
        digitalWrite(secondaryMuxPins[i], (col >> i) & 1);
      }
      delayMicroseconds(5);
      int v = analogRead(analogPin);
      if (v < 512) break;
    }
    Serial.printf("  Detected! Button %u OK.\n", b);
    delay(250);
  }
  // Control buttons
  for (uint8_t i = 0; i < NUM_CONTROL_BUTTONS; ++i) {
    Serial.printf("Press control button #%u (pin %u)...\n", i, (const uint8_t[]){2,3,4,5,6,13}[i]);
    while (digitalRead((const uint8_t[]){2,3,4,5,6,13}[i]));
    Serial.printf("  Detected! Control button %u OK.\n", i);
    delay(250);
  }
  displayManager.clear();
}

// Potentiometer test: prompt to rotate each to min and max, print readings
void testPots() {
  Serial.println("=== Potentiometer Test (Range Check) ===");
  displayManager.showText("Pot Test", "Follow Serial...");
  for (uint8_t bank = 0; bank < (1 << PRIMARY_MUX_PINS); ++bank) {
    potentiometerManager.selectMuxBank(bank);
    for (uint8_t pot = 0; pot < (1 << SECONDARY_MUX_PINS); ++pot) {
      potentiometerManager.selectPotBank(pot);
      uint8_t idx = (bank << SECONDARY_MUX_PINS) | pot;
      if (idx >= NUM_POTS) continue;

      char label[24];
      sprintf(label, "Pot#%u→MIN", idx);

      Serial.printf("\nPot #%u: Turn to MIN, then press any button.\n", idx);
      displayManager.showText("Pot Test", label);
      waitForAnyButton();
      int vmin = potentiometerManager.readAnalogFiltered(analogPin);

      sprintf(label, "Pot#%u→MAX", idx);
      Serial.printf("Pot #%u: Turn to MAX, then press any button.\n", idx);
      displayManager.showText("Pot Test", label);
      waitForAnyButton();
      int vmax = potentiometerManager.readAnalogFiltered(analogPin);

      int delta = vmax - vmin;
      bool pass = (delta >= POT_RANGE_MIN);

      char result[64];
      sprintf(result, "Min=%d Max=%d Δ=%d", vmin, vmax, delta);
      Serial.printf("  %s\n", result);
      if (pass) {
        Serial.println("  PASS\n");
        displayManager.showText("Pot PASS", result);
      } else {
        Serial.println("  FAIL — Check wiring or range!");
        displayManager.showText("Pot FAIL", result);
      }
      delay(1000);
      displayManager.clear();
    }
  }
}

// Envelope follower test: Display analog level on OLED and Serial, prompt for user action
void testEnvelopes() {
  Serial.println("=== Envelope Follower Test (Range Check) ===");
  for (size_t i = 0; i < envelopeFollowers.size(); ++i) {
    envelopeFollowers[i].toggleActive(true);
    char msg[32];
    sprintf(msg, "EF pin %d", (int[]){A0, A1, A2, A3, A6, A7}[i]);
    Serial.printf("Move signal from min to max on EF #%u (pin %d).\n", (unsigned)i, (int[]){A0, A1, A2, A3, A6, A7}[i]);
    Serial.println("First, set input to MIN and press any button.");
    displayManager.showText("Env Test", msg, "Set to MIN, press");
    waitForAnyButton();
    envelopeFollowers[i].update();
    int vmin = envelopeFollowers[i].getEnvelopeLevel();

    Serial.println("Now, set input to MAX and press any button.");
    displayManager.showText("Env Test", msg, "Set to MAX, press");
    waitForAnyButton();
    envelopeFollowers[i].update();
    int vmax = envelopeFollowers[i].getEnvelopeLevel();

    int delta = vmax - vmin;
    bool pass = (delta >= ENV_RANGE_MIN);

    char result[64];
    sprintf(result, "Min=%d Max=%d Δ=%d", vmin, vmax, delta);
    Serial.printf("  %s\n", result);
    if (pass) {
      Serial.println("  PASS\n");
      displayManager.showText("Env PASS", result);
    } else {
      Serial.println("  FAIL — Check signal or EF input!");
      displayManager.showText("Env FAIL", result);
    }
    delay(1000);
    displayManager.clear();
  }
}

// Display test: show various test patterns
void testDisplay() {
  Serial.println("=== Display Test ===");
  displayManager.clear();
  displayManager.showText("Display Test", "Line 2", "Line 3");
  Serial.println("Check: All 3 lines visible on OLED. Press any button.");
  waitForAnyButton();
  displayManager.clear();
  displayManager.showText("Test Done", "", "");
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial);
  delay(250);
  Serial.println("===== MOARkNOBS Hardware Interactive Test =====");

  // Hardware init
  configManager.begin(potChannels);
  ledManager.begin();
  displayManager.begin();
  potentiometerManager.loadFromEEPROM();
  buttonManager.initButtons();
  for (auto pin: primaryMuxPins) pinMode(pin, OUTPUT);
  for (auto pin: secondaryMuxPins) pinMode(pin, OUTPUT);
  pinMode(analogPin, INPUT);

  delay(300);
  Serial.println("Get ready!");

  testLEDs();
  testButtons();
  testPots();
  testEnvelopes();
  testDisplay();

  Serial.println("ALL TESTS COMPLETE. If all steps were confirmed, hardware is verified.");
  displayManager.showText("All tests", "COMPLETE!");
}

void loop() {}
