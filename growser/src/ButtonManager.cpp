#include "ButtonManager.h"

ButtonManager::ButtonManager(const uint8_t* primaryMuxPins, const uint8_t* secondaryMuxPins, uint8_t analogPin)
    : _primaryMuxPins(primaryMuxPins), _secondaryMuxPins(secondaryMuxPins), _analogPin(analogPin) {
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

void ButtonManager::processButtons(
    uint8_t* potChannels,
    uint8_t& activePot,
    uint8_t& activeChannel,
    bool& envelopeFollowMode,
    ConfigManager& configManager,
    LEDManager& ledManager,
    DisplayManager& displayManager,
    EnvelopeFollower& envelopeFollower
) {
    uint8_t pressedButtons = 0;
    unsigned long currentTime = millis();

    for (int i = 0; i < NUM_BUTTONS; i++) {
        uint8_t currentState = readButton(i);
        if (Utility::debounce(buttonStates[i], currentState, lastDebounceTimes[i], currentTime, DEBOUNCE_DELAY)) {
            // Button state has changed
            if (buttonStates[i]) {
                // Button pressed
                pressedButtons |= (1 << i);
                handleSingleButtonPress(
                    i,
                    potChannels,
                    activePot,
                    activeChannel,
                    envelopeFollowMode,
                    configManager,
                    ledManager,
                    displayManager
                );
            }
        }
    }

    handleMultiButtonPress(pressedButtons, displayManager);
}

void ButtonManager::handleSingleButtonPress(
    uint8_t buttonIndex,
    uint8_t* potChannels,
    uint8_t& activePot,
    uint8_t& activeChannel,
    bool& envelopeFollowMode,
    ConfigManager& configManager,
    LEDManager& ledManager,
    DisplayManager& displayManager
) {
    switch (buttonIndex) {
        case 0:
            envelopeFollowMode = !envelopeFollowMode;
            ledManager.indicateEnvelopeMode(envelopeFollowMode);
            displayManager.showText(envelopeFollowMode ? "EF" : "OF", true);
            break;

        case 1:
            activeChannel = (activeChannel % 16) + 1;
            displayManager.showValue(activeChannel, true);
            break;

        case 2:
            activeChannel = (activeChannel == 1) ? 16 : activeChannel - 1;
            displayManager.showValue(activeChannel, true);
            break;

        case 3:
            configManager.saveConfig(potChannels);
            displayManager.showText("SV", true);
            break;

        case 4:
            configManager.loadConfig(potChannels);
            displayManager.showText("RS", true);
            break;

        case 5:
            activePot = (activePot + 1) % NUM_POTS;
            displayManager.showValue(activePot, true);
            ledManager.setActivePot(activePot);
            break;

        default:
            Serial.println("Unknown Button Pressed");
            break;
    }
}

void ButtonManager::handleMultiButtonPress(uint8_t pressedButtons, DisplayManager& displayManager) {
    if ((pressedButtons & (1 << 0)) && (pressedButtons & (1 << 5))) { //buttons 1 & 6
        Serial.println("Rebooting Teensy...");
        displayManager.showText("RESET", false);
        rebootTeensy();
    } else if ((pressedButtons & (1 << 1)) && (pressedButtons & (1 << 4))) { //buttons 2 & 5
        Serial.println("Special MIDI channel adjustment");
        displayManager.showText("MD", true);
    } else if ((pressedButtons & (1 << 2)) && (pressedButtons & (1 << 3))) { //buttons 3 & 4
        displayManager.showText("SEQ", true); //sequencer mode? 42 step, tempo set by external midi clock
    }
}
