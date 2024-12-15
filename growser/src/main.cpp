#include <Arduino.h>
#include "MIDIHandler.h"
#include "LEDManager.h"
#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "PotentiometerManager.h"

// Constants
#define LED_PIN 6
#define NUM_LEDS 42
#define NUM_BUTTONS 6
#define OLED_I2C_ADDRESS 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
// pin assignments for primary and secondary mux layers
const uint8_t primaryMuxPins[] = {7, 8, 9};  // pins for primary mux
const uint8_t secondaryMuxPins[] = {10, 11, 12}; // pins for secondary mux
const uint8_t analogPin = 22; // Pin to read the mux output

// Global objects
MIDIHandler midiHandler;
ConfigManager configManager(sizeof(uint8_t) * NUM_POTS);
LEDManager ledManager(LED_PIN, NUM_LEDS);
DisplayManager displayManager(OLED_I2C_ADDRESS);
ButtonManager buttonManager(primaryMuxPins, secondaryMuxPins, analogPin);
PotentiometerManager potentiometerManager(primaryMuxPins, secondaryMuxPins, analogPin);
EnvelopeFollower envelopeFollower(A0, &potentiometerManager); // A0 is the audio input pin

// Hardware states
uint8_t potChannels[NUM_POTS];
uint8_t activePot = 0xFF;    // Currently active pot
uint8_t activeChannel = 1;   // Default active MIDI channel
bool envelopeFollowMode = false; // Envelope follow mode status

void setup() {
    // Initialize serial for MIDI
    Serial.begin(31250);
    midiHandler.begin();

    // Initialize LEDs and Display
    ledManager.begin();
    displayManager.begin();
    displayManager.showText("Initializing...");

    // Load configuration from EEPROM
    potentiometerManager.loadFromEEPROM();

    // Initialize buttons
    buttonManager.initButtons();
    delay(1000); // Just hang for a moment so that we're sure we've settled
    displayManager.clear();
    displayManager.showText("BENZ");
}

void loop() {
    // Process incoming MIDI messages
    midiHandler.processIncomingMIDI();

    // Process button presses
  buttonManager.processButtons(
    potChannels,
    activePot,
    activeChannel,
    envelopeFollowMode,
    configManager,
    ledManager,
    displayManager,
    envelopeFollower
);

    // Apply envelope modulation to active pot
    uint8_t ccValue = potentiometerManager.getCCNumber(activePot);
    envelopeFollower.applyToCC(activePot, ccValue);

    // Send modulated CC value
    midiHandler.sendControlChange(
        potentiometerManager.getCCNumber(activePot),
        ccValue,
        potentiometerManager.getChannel(activePot)
    );

    // Process potentiometer values
    potentiometerManager.processPots(midiHandler, ledManager);

    // Update LEDs
    ledManager.update();
}
