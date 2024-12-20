#include <Arduino.h>
#include "MIDIHandler.h"
#include "LEDManager.h"
#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "PotentiometerManager.h"
#include "SequenceManager.h"
#include <map> // For tracking pot-to-envelope associations

// Constants
#define LED_PIN 6
#define NUM_LEDS 42
#define NUM_BUTTONS 6
#define OLED_I2C_ADDRESS 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

// Pin assignments for primary and secondary mux layers
const uint8_t primaryMuxPins[] = {7, 8, 9};
const uint8_t secondaryMuxPins[] = {10, 11, 12};
const uint8_t analogPin = 22; //mux reader

// Global objects
MIDIHandler midiHandler;
ConfigManager configManager(sizeof(uint8_t) * NUM_POTS);
LEDManager ledManager(LED_PIN, NUM_LEDS);
DisplayManager displayManager(OLED_I2C_ADDRESS); //check documentation
ButtonManager buttonManager(primaryMuxPins, secondaryMuxPins, analogPin);
PotentiometerManager potentiometerManager(primaryMuxPins, secondaryMuxPins, analogPin);
Sequencer sequencer;

// envelope followers - assign to analog inputs
std::vector<EnvelopeFollower> envelopeFollowers = {
    EnvelopeFollower(A0, &potentiometerManager),
    EnvelopeFollower(A1, &potentiometerManager),
    EnvelopeFollower(A2, &potentiometerManager),
    EnvelopeFollower(A3, &potentiometerManager)
};

// Pot-to-envelope mapping
std::map<int, int> potToEnvelopeMap; // Map pot index to envelope index

// Hardware states
uint8_t potChannels[NUM_POTS];
uint8_t activePot = 0xFF;
uint8_t activeChannel = 1;
bool envelopeFollowMode = false;

void setup() {
    Serial.begin(31250);
    midiHandler.begin();
    ledManager.begin();
    displayManager.begin();
    displayManager.showText("Initializing...", true);
    potentiometerManager.loadFromEEPROM();

    // Initialize the pot-to-envelope map
    potToEnvelopeMap[0] = 0; // Pot 0 controls Envelope 0
    potToEnvelopeMap[1] = 1; // Pot 1 controls Envelope 1
    potToEnvelopeMap[2] = 2; // Pot 2 controls Envelope 2
    potToEnvelopeMap[3] = 3; // Pot 3 controls Envelope 3

    for (auto& envelope : envelopeFollowers) {
    envelope.toggleActive(true); // Ensure all envelopes are activated
    }

    buttonManager.initButtons();
    delay(1000);
    displayManager.clear();
    displayManager.showText("BENZ", true);
}

void loop() {
    midiHandler.processIncomingMIDI();

    static uint8_t midiBeatPosition = 0;
    if (midiHandler.isClockTick()) {
        midiBeatPosition = (midiBeatPosition + 1) % 8;
    }

    Utility::updateVisuals(
        midiBeatPosition,
        envelopeFollowers,
        envelopeFollowMode ? "EF ON" : "EF OFF",
        activePot,
        activeChannel,
        ledManager,
        displayManager
    );

    buttonManager.processButtons(
        potChannels,
        activePot,
        activeChannel,
        envelopeFollowMode,
        configManager,
        ledManager,
        displayManager,
        envelopeFollowers[0], // First envelope as reference
        sequencer
    );

    // Process envelope followers
    for (const auto& [potIndex, envelopeIndex] : potToEnvelopeMap) {
    if (envelopeIndex < static_cast<int>(envelopeFollowers.size())) { // Fix signedness comparison
        EnvelopeFollower& envelope = envelopeFollowers[envelopeIndex];
        envelope.update();

            if (envelope.getActiveState()) {
                uint8_t ccValue = potentiometerManager.getCCNumber(potIndex);
                envelope.applyToCC(potIndex, ccValue);
                midiHandler.sendControlChange(
                    potentiometerManager.getCCNumber(potIndex),
                    ccValue,
                    potentiometerManager.getChannel(potIndex)
                );
                ledManager.setPotValue(potIndex, ccValue);
            }
        }
    }


    potentiometerManager.processPots(ledManager, envelopeFollowers);

    if (sequencer.isActive()) {
        uint8_t stepValue = sequencer.getStepValue(activePot);
        envelopeFollowers[0].applyToCC(activePot, stepValue);
        midiHandler.sendControlChange(
            potentiometerManager.getCCNumber(activePot),
            stepValue,
            potentiometerManager.getChannel(activePot)
        );
        ledManager.setPotValue(activePot, stepValue);
        sequencer.advanceStep();
    }
}
