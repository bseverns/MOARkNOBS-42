#include "ButtonManager.h"

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
    uint8_t secondary = buttonIndex % 8;
    selectMux(primary, secondary);

    int value = analogRead(_analogPin);
    return value < 512 ? HIGH : LOW;
}

void ButtonManager::handleSingleButtonPress(uint8_t buttonIndex, ButtonManagerContext& context) {
    static uint8_t filterTypeIndex = 0;

    switch (buttonIndex) {
        case 0: {
            if (digitalRead(FUNCTION_BUTTON_PIN) == HIGH) {
                filterTypeIndex = (filterTypeIndex + 1) % 4;
                const char* filterTypes[] = {"LINEAR", "OPPOSITE", "EXP", "RAND"};
                for (auto& envelope : context.envelopes) {
                    envelope.setFilterType(static_cast<EnvelopeFollower::FilterType>(filterTypeIndex));
                }
                context.displayManager.displayStatus(filterTypes[filterTypeIndex], 2000);
            } else {
                context.envelopeFollowMode = !context.envelopeFollowMode;
                for (auto& envelope : context.envelopes) {
                    envelope.toggleActive(context.envelopeFollowMode);
                }
                context.displayManager.displayStatus(context.envelopeFollowMode ? "EF ON" : "EF OFF", 2000);
                context.ledManager.indicateEnvelopeMode(context.envelopeFollowMode);
            }
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
            context.configManager.saveConfig(context.potChannels);
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
        context.configManager.saveConfig(context.potChannels);
    } else if ((pressedButtons & (1 << 1)) && (pressedButtons & (1 << 4))) {
        context.displayManager.displayStatus("REBOOTING", 2000);
        rebootTeensy(); // Ensure this is implemented
    } else if ((pressedButtons & (1 << 2)) && (pressedButtons & (1 << 3))) {
        context.displayManager.displayStatus("CONFIG LOADED", 2000);
        context.configManager.loadConfig(context.potChannels);
    } else {
        context.displayManager.displayStatus("NO ACTION", 2000);
    }
}

void ButtonManager::processButtons(ButtonManagerContext& context) {
    uint8_t pressedButtons = 0;
    unsigned long currentTime = millis();

    for (int i = 0; i < NUM_BUTTONS; i++) {
        uint8_t currentState = readButton(i);
        if (Utility::debounce(buttonStates[i], currentState, lastDebounceTimes[i], currentTime, DEBOUNCE_DELAY)) {
            if (buttonStates[i]) {
                pressedButtons |= (1 << i);
                handleSingleButtonPress(i, context);
            }
        }
    }

    if (pressedButtons > 1) {
        // Multi-button presses can be handled here.
        handleMultiButtonPress(pressedButtons, context);

    }
}