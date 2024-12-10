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
#define DISPLAY_CLK_PIN 5
#define DISPLAY_DIO_PIN 4
#define NUM_BUTTONS 6
const uint8_t BUTTON_PINS[NUM_BUTTONS] = {2, 3, 8, 9, 10, 11};

// Global objects
MIDIHandler midiHandler;
ConfigManager configManager(sizeof(uint8_t) * NUM_POTS);
LEDManager ledManager(LED_PIN, NUM_LEDS);
DisplayManager displayManager(DISPLAY_CLK_PIN, DISPLAY_DIO_PIN);
ButtonManager buttonManager;
PotentiometerManager potentiometerManager;
EnvelopeFollower envelopeFollower(A0, &potentiometerManager); // Replace A0 with the actual pin for envelope input

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

    // Load configuration from EEPROM
    potentiometerManager.loadFromEEPROM();

    // Initialize buttons
    buttonManager.initButtons(BUTTON_PINS, NUM_BUTTONS);
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
