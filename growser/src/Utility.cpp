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

void Utility::processBulkUpdate(const String& command, uint8_t numPots) {
    if (!command.startsWith("SET_ALL")) {
        Serial.println("Error: Command must start with 'SET_ALL'");
        return;
    }

    int startIdx = 8; // Skip "SET_ALL "
    unsigned int currentPot = 0; // Use unsigned int for consistency

    // Compare startIdx with command.length() explicitly cast to unsigned
    while (static_cast<unsigned int>(startIdx) < static_cast<unsigned int>(command.length()) &&
           currentPot < static_cast<unsigned int>(numPots)) {
        int ccEnd = command.indexOf(',', startIdx);
        int channelEnd = command.indexOf(';', startIdx);

        if (ccEnd == -1 || channelEnd == -1 || ccEnd >= channelEnd) {
            Serial.println("Error: Malformed command");
            return;
        }

        int ccNumber = command.substring(startIdx, ccEnd).toInt();
        int channel = command.substring(ccEnd + 1, channelEnd).toInt();

        // Validate CC number and channel
        if (ccNumber < 0 || ccNumber > 127 || channel < 1 || channel > 16) {
            Serial.println("Error: Invalid CC number or channel");
            return;
        }

        // Update EEPROM
        int address = currentPot * 2; // Assuming 2 bytes per pot (channel + CC number)
        EEPROM.update(address, channel);
        EEPROM.update(address + 1, ccNumber);

        currentPot++;
        startIdx = channelEnd + 1;
    }

    if (currentPot == static_cast<unsigned int>(numPots)) {
        Serial.println("Bulk update successful");
    } else {
        Serial.println("Error: Insufficient data for all pots");
    }
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