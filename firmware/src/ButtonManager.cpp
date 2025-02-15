#include "ButtonManager.h"

// Add new member variables for tracking double-press state
//static uint8_t lastPressedButton = 255;  // Invalid button index initially
//static unsigned long lastPressTime = 0;  // Time of the last button press
const unsigned long doublePressThreshold = 400; // Max time (ms) between presses for a double-press

// Constructor
ButtonManager::ButtonManager(const uint8_t* primaryMuxPins, const uint8_t* secondaryMuxPins, uint8_t muxAnalogPin, const uint8_t* controlPins, PotentiometerManager* potentiometerManager)
    : _primaryMuxPins(primaryMuxPins), _secondaryMuxPins(secondaryMuxPins), _muxAnalogPin(muxAnalogPin), _controlPins(controlPins), _potentiometerManager(potentiometerManager) {
    for (int i = 0; i < NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS; i++) {
        buttonStates[i] = false;
        lastDebounceTimes[i] = 0;
    }
}

void ButtonManager::initButtons() {
    for (int i = 0; i < 3; i++) {
        pinMode(_primaryMuxPins[i], OUTPUT);
        pinMode(_secondaryMuxPins[i], OUTPUT);
    }
    pinMode(analogPin, INPUT);

    for (int i = 0; i < NUM_CONTROL_BUTTONS; i++) {
        pinMode(_controlPins[i], INPUT_PULLUP); // Control buttons use GPIO with pull-up
    }
}

void ButtonManager::selectMux(uint8_t row, uint8_t col) {
    for (int i = 0; i < 3; i++) {
        digitalWrite(_primaryMuxPins[i], (row >> i) & 1);
        digitalWrite(_secondaryMuxPins[i], (col >> i) & 1);
    }
}

uint8_t ButtonManager::readMuxButton(uint8_t buttonIndex) {
    uint8_t row = buttonIndex / 8;
    uint8_t col = buttonIndex % 8;

    selectMux(row, col);
    int value = analogRead(analogPin);

    return value < 512 ? HIGH : LOW; // Threshold for button press detection
}

bool ButtonManager::readControlButton(uint8_t buttonIndex) {
    return digitalRead(_controlPins[buttonIndex]) == LOW; // Active LOW
}

void ButtonManager::handleSingleButtonPress(uint8_t buttonIndex, ButtonManagerContext& context) {
    // Handle virtual button presses
    if (buttonIndex < NUM_VIRTUAL_BUTTONS) {
        context.activePot = buttonIndex; // Update active "pot"
        context.displayManager.showText(
            ("Active BTN: " + String(buttonIndex)).c_str(),
            "Adjust w/ Master Pot",
            ""
        );
        return;
    }

    // Handle control button presses
    buttonIndex -= NUM_VIRTUAL_BUTTONS; // Offset to index control buttons
    switch (buttonIndex) {
        case 0:
            context.envelopeFollowMode = !context.envelopeFollowMode; // Toggle EF mode
            context.displayManager.displayStatus(context.envelopeFollowMode ? "EF ON" : "EF OFF", 2000);
            break;
        case 1:
            context.activeChannel = (context.activeChannel % 16) + 1; // Increment MIDI channel
            context.displayManager.displayStatus(("CHAN+ " + String(context.activeChannel)).c_str(), 2000);
            break;
        case 2:
            context.configManager.saveConfiguration();
            context.displayManager.displayStatus("Config SAVED", 2000);
            break;
        case 3:
            context.displayManager.displayStatus("Randomizing...", 2000);
            for (int i = 0; i < NUM_VIRTUAL_BUTTONS; i++) {
                context.potChannels[i] = random(1, 17);
                _potentiometerManager->setCCNumber(i, random(0, 128));
            }
            break;
        case 4:
            context.displayManager.displayStatus("CONFIRM CLEAR?", 2000);
            delay(2000);
            context.potToEnvelopeMap.clear();
            _potentiometerManager->resetEEPROM();
            context.displayManager.displayStatus("CLEARED", 2000);
            break;
        default:
            context.displayManager.displayStatus("UNKNOWN BTN", 1000);
            break;
    }
}

void ButtonManager::processButtons(ButtonManagerContext& context) {
    unsigned long currentTime = millis();

    // Process virtual buttons
    for (uint8_t i = 0; i < NUM_VIRTUAL_BUTTONS; i++) {
        uint8_t currentState = readMuxButton(i);
        if (Utility::debounce(buttonStates[i], currentState, lastDebounceTimes[i], currentTime, DEBOUNCE_DELAY)) {
            if (buttonStates[i]) {
                handleSingleButtonPress(i, context);
            }
        }
    }

    // Process control buttons
    for (uint8_t i = 0; i < NUM_CONTROL_BUTTONS; i++) {
        bool currentState = readControlButton(i);
        if (Utility::debounce(buttonStates[NUM_VIRTUAL_BUTTONS + i], currentState, lastDebounceTimes[NUM_VIRTUAL_BUTTONS + i], currentTime, DEBOUNCE_DELAY)) {
            if (buttonStates[NUM_VIRTUAL_BUTTONS + i]) {
                handleSingleButtonPress(NUM_VIRTUAL_BUTTONS + i, context);
            }
        }
    }
}
