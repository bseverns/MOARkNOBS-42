#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>
#include "Utility.h"
#include "ConfigManager.h"
#include "LEDManager.h"
#include "DisplayManager.h"
#include "EnvelopeFollower.h"
#include "SequenceManager.h"

#define NUM_BUTTONS 6
#define DEBOUNCE_DELAY 50 // 50 milliseconds debounce delay

class ButtonManager {
public:
    ButtonManager(const uint8_t* primaryMuxPins, const uint8_t* secondaryMuxPins, uint8_t analogPin);
    void initButtons();
    void processButtons(
        uint8_t* potChannels,
        uint8_t& activePot,
        uint8_t& activeChannel,
        bool& envelopeFollowMode,
        ConfigManager& configManager,
        LEDManager& ledManager,
        DisplayManager& displayManager,
        EnvelopeFollower& envelopeFollower,
        Sequencer& sequencer
    );

private:
    const uint8_t* _primaryMuxPins;
    const uint8_t* _secondaryMuxPins;
    uint8_t _analogPin;
    bool buttonStates[NUM_BUTTONS];
    unsigned long lastDebounceTimes[NUM_BUTTONS]; // Track last debounce time for each button

    void selectMux(uint8_t primary, uint8_t secondary);
    uint8_t readButton(uint8_t buttonIndex);
    void handleSingleButtonPress(
        uint8_t buttonIndex,
        uint8_t* potChannels,
        uint8_t& activePot,
        uint8_t& activeChannel,
        bool& envelopeFollowMode,
        ConfigManager& configManager,
        LEDManager& ledManager,
        DisplayManager& displayManager
    );
    void handleMultiButtonPress(uint8_t pressedButtons, DisplayManager& displayManager);
};

#endif // BUTTON_MANAGER_H
