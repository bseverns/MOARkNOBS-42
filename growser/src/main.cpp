#include <Arduino.h>
#include "MIDIHandler.h"
#include "LEDManager.h"
#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "PotentiometerManager.h"
#include "SequenceManager.h"
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
Sequencer sequencer; // Corrected class name

// Hardware states
uint8_t potChannels[NUM_POTS];
uint8_t activePot = 0xFF;    // Currently active pot
uint8_t activeChannel = 1;   // Default active MIDI channel
bool envelopeFollowMode = false; // Envelope follow mode status

std::vector<EnvelopeFollower> envelopeFollowers = {
    EnvelopeFollower(A0, &potentiometerManager),
    EnvelopeFollower(A1, &potentiometerManager), // Another audio input pin
    EnvelopeFollower(A2, &potentiometerManager)  // Add as needed
};

void setup() {
    // Initialize serial for MIDI
    Serial.begin(31250);
    midiHandler.begin();

    // Initialize LEDs and Display
    ledManager.begin();
    //startup light function
    displayManager.begin();
    displayManager.showText("Initializing...", true);

    // Load configuration from EEPROM
    potentiometerManager.loadFromEEPROM();

       for (auto& envelope : envelopeFollowers) {
        envelope.toggleActive(); // Activate them by default if needed
    }


    // Initialize buttons
    buttonManager.initButtons();
    delay(1000); // Just hang for a moment so that we're sure we've settled
    displayManager.clear();
    displayManager.showText("BENZ", true);

    
}

void loop() {
    // Process incoming MIDI messages
    midiHandler.processIncomingMIDI();
 
    // Update MIDI clock position
    static uint8_t midiBeatPosition = 0;
    if (midiHandler.isClockTick()) {
        midiBeatPosition = (midiBeatPosition + 1) % 8; // 8 clock ticks per beat
    }

    // Get the envelope level
    envelopeFollower.update();
    uint8_t envelopeLevel = envelopeFollower.getEnvelopeLevel();

    // Update display with MIDI clock, envelope level, and digital snow
    displayManager.updateDisplay(midiBeatPosition, envelopeLevel);

    // Process button presses
    buttonManager.processButtons(
        potChannels,
        activePot,
        activeChannel,
        envelopeFollowMode,
        configManager,
        ledManager,
        displayManager,
        envelopeFollower,
        sequencer 
    );

   for (auto& envelope : envelopeFollowers) {
        envelope.update(); // Update each envelope
    }

    // Apply envelopes to their respective pots
    for (int i = 0; i < envelopeFollowers.size(); ++i) {
        uint8_t ccValue = potentiometerManager.getCCNumber(i); // Assuming a 1-to-1 mapping
        envelopeFollowers[i].applyToCC(i, ccValue);
        midiHandler.sendControlChange(
            potentiometerManager.getCCNumber(i),
            ccValue,
            potentiometerManager.getChannel(i)
        );
    }
    // Send modulated CC value
    midiHandler.sendControlChange(
        potentiometerManager.getCCNumber(activePot),
        ccValue,
        potentiometerManager.getChannel(activePot)
    );
 
    // Update display
    displayManager.updateDisplay(midiBeatPosition, envelopeLevel);

    // Process potentiometer values
PotentiometerManager potManager(primaryMuxPins, secondaryMuxPins, analogPin);

potManager.setMidiCallback([](uint8_t cc, uint8_t value, uint8_t channel) {
    midiHandler.sendControlChange(cc, value, channel); // Use MIDIHandler instance
});

    if (sequencer.isActive()) {
        uint8_t stepValue = sequencer.getStepValue(activePot);
        envelopeFollower.applyToCC(activePot, stepValue);
        midiHandler.sendControlChange(
            potentiometerManager.getCCNumber(activePot),
            stepValue,
            potentiometerManager.getChannel(activePot)
        );

        ledManager.setPotValue(activePot, stepValue);
        sequencer.advanceStep();
    }

    // Update LEDs
    ledManager.update();
}
