#include "ButtonManager.h"

// Add new member variables for tracking double-press state
static uint8_t lastPressedButton = 255;  // Invalid button index initially
static unsigned long lastPressTime = 0;  // Time of the last button press
const unsigned long doublePressThreshold = 400; // Max time (ms) between presses for a double-press

// Constructor
ButtonManager::ButtonManager(const uint8_t* primaryMuxPins, const uint8_t* secondaryMuxPins, uint8_t analogPin, PotentiometerManager* potentiometerManager)
    : _primaryMuxPins(primaryMuxPins), _secondaryMuxPins(secondaryMuxPins), _analogPin(analogPin), _potentiometerManager(potentiometerManager), activeEnvelopeIndex(0) {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        buttonStates[i] = false;
        lastDebounceTimes[i] = 0;
    }
}

void ButtonManager::initButtons() {
    for (int i = 0; i < 3; i++) {
        pinMode(_primaryMuxPins[i], OUTPUT);
        pinMode(_secondaryMuxPins[i], OUTPUT);
    }
    pinMode(_analogPin, INPUT);
}

void ButtonManager::selectMux(uint8_t primary, uint8_t secondary) {
    for (int i = 0; i < 3; i++) {
        digitalWrite(_primaryMuxPins[i], (primary >> i) & 1);
        digitalWrite(_secondaryMuxPins[i], (secondary >> i) & 1);
    }
}

uint8_t ButtonManager::readButton(uint8_t buttonIndex) {
    uint8_t primary = buttonIndex / 8;
    uint8_t secondary = 8;
    selectMux(primary, secondary);

    int value = analogRead(_analogPin);
    return value < 512 ? HIGH : LOW;
}

void ButtonManager::handleSingleButtonPress(uint8_t buttonIndex, ButtonManagerContext& context) {
    static uint8_t filterTypeIndex = 0;
    unsigned long currentTime = millis();

    // Restrict double-press to button 0
    if (buttonIndex == 0) {
        // Check if this is a double-press
        if (buttonIndex == lastPressedButton && (currentTime - lastPressTime <= doublePressThreshold)) {
            // Double-press detected for button 0
            lastPressedButton = 255; // Reset the last pressed button
            lastPressTime = 0;

            // Perform the shift-key functionality on double-press
            filterTypeIndex = (filterTypeIndex + 1) % 4;
            const char* filterTypes[] = {"LINEAR", "OPPOSITE", "EXP", "RAND"};
            for (auto& envelope : context.envelopes) {
                envelope.setFilterType(static_cast<EnvelopeFollower::FilterType>(filterTypeIndex));
            }
            context.displayManager.displayStatus(filterTypes[filterTypeIndex], 2000);

            return; // Exit to avoid single-press logic
        }
    }

    if (buttonIndex == 1) { // Button 2: Double-press to switch envelope assignment
        if (buttonIndex == lastPressedButton && (currentTime - lastPressTime <= doublePressThreshold)) {
            // Double-press detected on button 2
            lastPressTime = 0; // Reset timing
            lastPressedButton = 255;

            // Cycle through envelope assignments for the active potentiometer
            if (context.activePot < NUM_POTS) {
                int currentEnvelope = context.potToEnvelopeMap[context.activePot];

                // Cycle: -1 for MIDI, then 0 to (number of envelopes - 1)
                currentEnvelope = (currentEnvelope + 2) % (context.envelopes.size() + 1) - 1;

                context.potToEnvelopeMap[context.activePot] = currentEnvelope;

                // Update the display to show the new assignment
                if (currentEnvelope == -1) {
                    context.displayManager.displayStatus("MIDI", 2000);
                } else {
                    context.displayManager.displayStatus(
                        ("POT " + String(context.activePot) + " -> ENV" + String(currentEnvelope + 1)).c_str(), 
                        2000
                    );
                }
            }
            return;
        }
    }

    // Single-press logic (applies to all buttons)
    lastPressedButton = buttonIndex;
    lastPressTime = currentTime;

    switch (buttonIndex) {
        case 0: {
            context.envelopeFollowMode = !context.envelopeFollowMode;
            for (auto& envelope : context.envelopes) {
                envelope.toggleActive(context.envelopeFollowMode);
            }
            context.displayManager.displayStatus(context.envelopeFollowMode ? "EF ON" : "EF OFF", 2000);
            context.ledManager.indicateEnvelopeMode(context.envelopeFollowMode);
            break;
        }
        case 1: {
            if (context.activePot < NUM_POTS) {
                uint8_t activeEnvelope = (context.potToEnvelopeMap[context.activePot] + 1) % context.envelopes.size();
                context.potToEnvelopeMap[context.activePot] = activeEnvelope;
                context.displayManager.displayStatus(("ENV->POT " + String(context.activePot) + "->" + String(activeEnvelope)).c_str(), 2000);
            }
            break;
        }
        case 2: {
            context.activeChannel = (context.activeChannel % 16) + 1;
            context.displayManager.displayStatus(("CHAN+ " + String(context.activeChannel)).c_str(), 2000);
            break;
        }
        case 3: {
            context.configManager.saveConfiguration();
            context.displayManager.displayStatus("SAVED", 2000);
            break;
        }
        case 4: {
            for (int i = 0; i < NUM_POTS; i++) {
                context.potChannels[i] = random(1, 17);
                _potentiometerManager->setCCNumber(i, random(0, 128));
            }
            context.displayManager.displayStatus("RANDOMIZED", 2000);
            break;
        }
        case 5: {
            context.displayManager.displayStatus("CONFIRM CLEAR?", 3000);
            delay(3000); // Wait for user reaction or button confirmation
            context.potToEnvelopeMap.clear();
            _potentiometerManager->resetEEPROM();
            context.displayManager.displayStatus("CLEARED", 2000);
            break;
        }
        default:
            context.displayManager.displayStatus("UNKNOWN BTN", 1000);
            break;
    }
}

void ButtonManager::handleMultiButtonPress(uint8_t pressedButtons, ButtonManagerContext& context) {
    if ((pressedButtons & (1 << 0)) && (pressedButtons & (1 << 5))) {
        context.displayManager.displayStatus("EEPROM SAVED", 2000);
        context.configManager.saveConfiguration();
    } else if ((pressedButtons & (1 << 1)) && (pressedButtons & (1 << 4))) {
        context.displayManager.displayStatus("REBOOTING", 2000);
        Utility::rebootTeensy(); // Ensure this is implemented
    } else if ((pressedButtons & (1 << 2)) && (pressedButtons & (1 << 3))) {
        context.displayManager.displayStatus("CONFIG LOADED", 2000);
        context.configManager.loadConfiguration(context.potChannels);
    } else {
        context.displayManager.displayStatus("NO ACTION", 2000);
    }
}

void ButtonManager::processButtons(ButtonManagerContext& context) {
    unsigned long currentTime = millis();

    for (int i = 0; i < NUM_BUTTONS; i++) {
        if ((i % 8) != 7) continue; // Skip all buttons except those mapped to pin 8
        
        uint8_t currentState = readButton(i);
        if (Utility::debounce(buttonStates[i], currentState, lastDebounceTimes[i], currentTime, DEBOUNCE_DELAY)) {
            if (buttonStates[i]) {
             handleSingleButtonPress(i, context);
            }
        }
    }
}