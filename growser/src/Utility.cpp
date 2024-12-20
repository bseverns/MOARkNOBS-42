#include "Utility.h"


uint8_t Utility::mapToMidiValue(int analogValue, int minValue, int maxValue) {
    return map(analogValue, minValue, maxValue, 0, 127);
}

bool Utility::debounceButton(uint8_t pin, unsigned long debounceDelay) {
    static unsigned long lastDebounceTime = 0;
    static bool lastButtonState = LOW;

    bool currentState = digitalRead(pin);
    if (currentState != lastButtonState) {
        lastDebounceTime = millis();
    }
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (currentState != lastButtonState) {
            lastButtonState = currentState;
            return currentState;
        }
    }
    return false;
}

void rebootTeensy() {
    SCB_AIRCR = 0x05FA0004; // Trigger a system reset for ARM Cortex-M
    while (1);              // Halt to ensure reset happens
}

bool Utility::debounce(bool &previousState, bool currentState, unsigned long &lastDebounceTime, unsigned long currentTime, unsigned long debounceDelay) {
    if (currentState != previousState) {
        lastDebounceTime = currentTime; // Update the debounce time
    }

    if ((currentTime - lastDebounceTime) > debounceDelay) {
        // State has stabilized
        previousState = currentState;
        return true;
    }

    return false; // No stable state change yet
}

void Utility::updateVisuals(
    uint8_t midiBeatPosition,
    const std::vector<EnvelopeFollower>& envelopeFollowers,
    const char* statusMessage,
    uint8_t activePot,
    uint8_t activeChannel,
    LEDManager& ledManager,
    DisplayManager& displayManager
) {
    // Update LEDs
    ledManager.update();

    // Collect envelope levels
    std::vector<uint8_t> envelopeLevels;
    for (const auto& follower : envelopeFollowers) {
        envelopeLevels.push_back(follower.getEnvelopeLevel());
    }

    // Update the display with all information
    displayManager.updateDisplay(midiBeatPosition, envelopeLevels, statusMessage, activePot, activeChannel);
}