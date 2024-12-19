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

// Pin assignments for primary and secondary mux layers
const uint8_t primaryMuxPins[] = {7, 8, 9};
const uint8_t secondaryMuxPins[] = {10, 11, 12};
const uint8_t analogPin = 22;

// Global objects
MIDIHandler midiHandler;
ConfigManager configManager(sizeof(uint8_t) * NUM_POTS);
LEDManager ledManager(LED_PIN, NUM_LEDS);
DisplayManager displayManager(OLED_I2C_ADDRESS);
ButtonManager buttonManager(primaryMuxPins, secondaryMuxPins, analogPin);
PotentiometerManager potentiometerManager(primaryMuxPins, secondaryMuxPins, analogPin);
Sequencer sequencer;

// Multiple envelope followers
std::vector<EnvelopeFollower> envelopeFollowers = {
    EnvelopeFollower(A0, &potentiometerManager),
    EnvelopeFollower(A1, &potentiometerManager),
    EnvelopeFollower(A2, &potentiometerManager)
};

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

    for (auto& envelope : envelopeFollowers) {
        envelope.toggleActive();
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

    for (auto& envelope : envelopeFollowers) {
        envelope.update();
        if (envelope.getActiveState() && envelope.getAssignedPot() >= 0) {
            int potIndex = envelope.getAssignedPot();
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

    ledManager.update();
    if (!envelopeFollowers.empty()) {
        displayManager.updateDisplay(midiBeatPosition, envelopeFollowers[0].getEnvelopeLevel());
    }
}
