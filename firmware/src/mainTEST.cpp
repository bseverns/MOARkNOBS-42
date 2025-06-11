#include <Arduino.h>
#include "Globals.h"
#include "ConfigManager.h"
#include "LEDManager.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "PotentiometerManager.h"
#include "EnvelopeFollower.h"
std::vector<uint8_t> potChannels; // EEPROM-loaded channels

#define SERIAL_BAUD 115200

// Instantiate board objects:
ConfigManager configManager(NUM_POTS, NUM_BUTTONS);
LEDManager ledManager(LED_PIN, NUM_LEDS);
DisplayManager displayManager(SSD1306_I2C_ADDRESS, OLED_WIDTH, OLED_HEIGHT);
PotentiometerManager potentiometerManager(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin);
ButtonManager buttonManager(primaryMuxPins, secondaryMuxPins, buttonMuxAnalogPin, (const uint8_t[]){2,3,4,5,6,13}, &potentiometerManager);
std::vector<EnvelopeFollower> envelopeFollowers = {
  EnvelopeFollower(A0, &potentiometerManager),
  EnvelopeFollower(A1, &potentiometerManager),
  EnvelopeFollower(A2, &potentiometerManager),
  EnvelopeFollower(A3, &potentiometerManager),
  EnvelopeFollower(A6, &potentiometerManager),
  EnvelopeFollower(A7, &potentiometerManager),
};

// --- Utility ---
void waitForSerialInput(const char* prompt = "Press Enter to continue...") {
  Serial.println(prompt);
  while (!Serial.available());
  Serial.read();  // clear input
}

// --- Unit Tests ---
void testLEDManager() {
  Serial.println("\n--- LEDManager Test ---");
  for (int i = 0; i < NUM_LEDS; i++) {
    ledManager.setColor(CRGB::Black);
    ledManager.setPotValue(i, 127);
    Serial.printf("LED #%d ON? Confirm visually and press Enter.\n", i);
    waitForSerialInput();
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
  const uint8_t ctrlPins[] = {2,3,4,5,6,13};
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
    Serial.printf("Pot #%d: set MIN, press Enter.\n", idx);
    waitForSerialInput();
    int vmin = potentiometerManager.readRawPot(idx);
    
    Serial.printf("Pot #%d: set MAX, press Enter.\n", idx);
    waitForSerialInput();
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
    Serial.printf("Envelope #%d (Pin A%d): MIN input, press Enter.\n", i, pins[i]);
    waitForSerialInput();
    envelopeFollowers[i].update();
    int vmin = envelopeFollowers[i].getEnvelopeLevel();

    Serial.printf("Envelope #%d (Pin A%d): MAX input, press Enter.\n", i, pins[i]);
    waitForSerialInput();
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
  Serial.println("Verify OLED shows 'Display Test', 'Line 2', 'Line 3'. Press Enter.");
  waitForSerialInput();
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
  ledManager.begin();
  displayManager.begin();
  potentiometerManager.loadFromEEPROM();
  buttonManager.initButtons();

  pinMode(potMuxAnalogPin, INPUT);
  pinMode(buttonMuxAnalogPin, INPUT);
  
  // Run each test individually:
  testLEDManager();
  testButtonManager();
  testPotentiometerManager();
  testEnvelopeFollowers();
  testDisplayManager();

  Serial.println("\n=== All Tests Completed! ===");
}

void loop() {}
