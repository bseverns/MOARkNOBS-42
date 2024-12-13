#include "ButtonManager.h"
#include "Utility.h"

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
    // Read button states
    for (int i = 0; i < NUM_BUTTONS; i++) {
        bool currentState = !digitalRead(buttonPins[i]); // Active LOW
        if (currentState && !buttonStates[i]) {
            // Button pressed
            buttonStates[i] = true;
        } else if (!currentState && buttonStates[i]) {
            // Button released
            buttonStates[i] = false;
        }
    }

    // Check combinations or single presses
    if (buttonStates[0] && buttonStates[5]) {
        // Example: Reboot on simultaneous press of buttons 0 and 5
        displayManager.showText("RB"); // Show "Reboot"
        delay(500);                    // Optional debounce delay
        rebootTeensy();                // Call the reboot function
    }

    if (buttonStates[0]) {
        // Toggle envelope follow mode
        envelopeFollowMode = !envelopeFollowMode;
        ledManager.indicateEnvelopeMode(envelopeFollowMode);
        displayManager.showText(envelopeFollowMode ? "EF" : "OF");
    }

    if (buttonStates[1]) {
        // Increment active MIDI channel
        activeChannel = (activeChannel % 16) + 1;
        displayManager.showValue(activeChannel);
    }

    if (buttonStates[2]) {
        // Decrement active MIDI channel
        activeChannel = (activeChannel == 1) ? 16 : activeChannel - 1;
        displayManager.showValue(activeChannel);
    }

    if (buttonStates[3]) {
        // Save configuration
        configManager.saveConfig(potChannels);
        displayManager.showText("SV"); // Save
    }

    if (buttonStates[4]) {
        // Reset configuration
        configManager.loadConfig(potChannels);
        displayManager.showText("RS"); // Reset
    }

    if (buttonStates[5]) {
        // Increment active potentiometer
        activePot = (activePot + 1) % NUM_POTS;
        displayManager.showValue(activePot);
        ledManager.setActivePot(activePot);
    }
}