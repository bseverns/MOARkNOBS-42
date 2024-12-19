#include "ButtonManager.h"

ButtonManager::ButtonManager(const uint8_t* primaryMuxPins, const uint8_t* secondaryMuxPins, uint8_t analogPin)
    : _primaryMuxPins(primaryMuxPins), _secondaryMuxPins(secondaryMuxPins), _analogPin(analogPin), activeEnvelopeIndex(0) {
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
    EnvelopeFollower& envelopeFollower,
    Sequencer& sequencer
) {
    uint8_t pressedButtons = 0;
    unsigned long currentTime = millis();

    for (int i = 0; i < NUM_BUTTONS; i++) {
        uint8_t currentState = readButton(i);
        if (Utility::debounce(buttonStates[i], currentState, lastDebounceTimes[i], currentTime, DEBOUNCE_DELAY)) {
            if (buttonStates[i]) {
                pressedButtons |= (1 << i);
                handleSingleButtonPress(
                    i,
                    potChannels,
                    activePot,
                    activeChannel,
                    envelopeFollowMode,
                    configManager,
                    ledManager,
                    displayManager,
                    sequencer
                );
            }
        }
    }

    handleMultiButtonPress(pressedButtons, displayManager, sequencer);
}

void ButtonManager::handleSingleButtonPress(
    uint8_t buttonIndex,
    uint8_t* potChannels,
    uint8_t& activePot,
    uint8_t& activeChannel,
    bool& envelopeFollowMode,
    ConfigManager& configManager,
    LEDManager& ledManager,
    DisplayManager& displayManager,
    Sequencer& sequencer
) {
    switch (buttonIndex) {
        case 0: // Envelope button
            if (envelopes && !envelopes->empty()) {
                activeEnvelopeIndex = (activeEnvelopeIndex + 1) % (envelopes->size() + 1); // Cycle through envelopes + MIDI
                if (activeEnvelopeIndex < envelopes->size()) {
                    envelopes->at(activeEnvelopeIndex).toggleActive();
                    displayManager.displayStatus(
                        ("ENV " + String(activeEnvelopeIndex + 1)).c_str(), 2000);
                } else {
                    envelopeFollowMode = false; // Exit envelope mode and return to MIDI
                    displayManager.displayStatus("MIDI", 2000);
                }
                ledManager.indicateEnvelopeMode(activeEnvelopeIndex < envelopes->size());
            }
            break;

        case 1:
            activeChannel = (activeChannel % 16) + 1;
            displayManager.displayStatus(("CHAN+" + String(activeChannel)).c_str(), 1500);
            break;

        case 2:
            activeChannel = (activeChannel == 1) ? 16 : activeChannel - 1;
            displayManager.displayStatus(("CH-" + String(activeChannel)).c_str(), 1500);
            break;

        case 3:
            configManager.saveConfig(potChannels);
            displayManager.displayStatus("SAVED", 2000);
            break;

        case 4:
            configManager.loadConfig(potChannels);
            displayManager.displayStatus("LOADED", 2000);
            break;

        case 5:
            activePot = (activePot + 1) % NUM_POTS;
            displayManager.displayStatus(("POT " + String(activePot)).c_str(), 1500);
            ledManager.setActivePot(activePot);
            break;

        default:
            displayManager.displayStatus("UNKNOWN", 1000);
            break;
    }
}

void ButtonManager::handleMultiButtonPress(uint8_t pressedButtons, DisplayManager& displayManager, Sequencer& sequencer) {
    if ((pressedButtons & (1 << 0)) && (pressedButtons & (1 << 5))) {
        Serial.println("Rebooting Teensy...");
        displayManager.displayStatus("RESET", 1000);
        rebootTeensy();
    } else if ((pressedButtons & (1 << 1)) && (pressedButtons & (1 << 4))) {
        Serial.println("Special MIDI channel adjustment");
        displayManager.showText("MD", true);
    } else if ((pressedButtons & (1 << 2)) && (pressedButtons & (1 << 3))) {
        displayManager.displayStatus("SEQ", 2000);
        sequencer.resetSequencer();
    }
}
