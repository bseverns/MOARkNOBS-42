#include "ButtonManager.h"

ButtonManager::ButtonManager() {
    // Initialize button states
    for (int i = 0; i < NUM_BUTTONS; i++) {
        buttonStates[i] = false;
    }
}

void ButtonManager::initButtons(const uint8_t pins[], uint8_t numButtons) {
    for (int i = 0; i < numButtons && i < NUM_BUTTONS; i++) {
        buttonPins[i] = pins[i];
        pinMode(buttonPins[i], INPUT_PULLUP);
    }
}

void ButtonManager::processButtons(
    uint8_t potChannels[],
    uint8_t &activePot,
    uint8_t &activeChannel,
    bool &envelopeFollowMode,
    ConfigManager &configManager,
    LEDManager &ledManager,
    DisplayManager &displayManager,
    EnvelopeFollower &envelopeFollower
) {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        bool currentState = !digitalRead(buttonPins[i]); // Active LOW
        if (currentState && !buttonStates[i]) {
            // Button pressed
            buttonStates[i] = true;

            // Handle button actions
            switch (i) {
                case 0: // Toggle envelope follow mode
                    envelopeFollowMode = !envelopeFollowMode;
                    ledManager.indicateEnvelopeMode(envelopeFollowMode);
                    displayManager.showText(envelopeFollowMode ? "EF" : "OF");
                    break;

                case 1: // Increment active MIDI channel
                    activeChannel = (activeChannel % 16) + 1;
                    displayManager.showValue(activeChannel);
                    break;

                case 2: // Decrement active MIDI channel
                    activeChannel = (activeChannel == 1) ? 16 : activeChannel - 1;
                    displayManager.showValue(activeChannel);
                    break;

                case 3: // Save configuration
                    configManager.saveConfig(potChannels);
                    displayManager.showText("SV"); // Save
                    break;

                case 4: // Reset configuration
                    configManager.loadConfig(potChannels);
                    displayManager.showText("RS"); // Reset
                    break;

                case 5: // Increment active potentiometer
                    activePot = (activePot + 1) % NUM_POTS;
                    displayManager.showValue(activePot);
                    ledManager.setActivePot(activePot);
                    break;
            }
        } else if (!currentState && buttonStates[i]) {
            // Button released
            buttonStates[i] = false;
        }
    }
}
