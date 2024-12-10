#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>
#include "ConfigManager.h"
#include "LEDManager.h"
#include "DisplayManager.h"
#include "EnvelopeFollower.h"
#include "PotentiometerManager.h"

#define NUM_BUTTONS 6

class ButtonManager {
private:
    uint8_t buttonPins[NUM_BUTTONS];
    bool buttonStates[NUM_BUTTONS];

public:
    ButtonManager();
    void initButtons(const uint8_t pins[], uint8_t numButtons);
    void processButtons(
        uint8_t potChannels[],
        uint8_t &activePot,
        uint8_t &activeChannel,
        bool &envelopeFollowMode,
        ConfigManager &configManager,
        LEDManager &ledManager,
        DisplayManager &displayManager,
        EnvelopeFollower &envelopeFollower
    );
};

#endif // BUTTON_MANAGER_H
