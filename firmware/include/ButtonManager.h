#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>
#include <map>
#include <vector>
#include "ConfigManager.h"
#include "LEDManager.h"
#include "DisplayManager.h"
#include "EnvelopeFollower.h"
#include "Utility.h"
#include "PotentiometerManager.h"

#define NUM_BUTTONS 6
#define FUNCTION_BUTTON_PIN 8 // to be changed in assembly
#define DEBOUNCE_DELAY 50 // Debounce delay in milliseconds

// Forward declaration
class ButtonManager;

struct ButtonManagerContext {
    std::vector<uint8_t>& potChannels; // Use reference to vector instead of raw pointer
    uint8_t& activePot;
    uint8_t& activeChannel;
    bool& envelopeFollowMode;
    ConfigManager& configManager;
    LEDManager& ledManager;
    DisplayManager& displayManager;
    std::vector<EnvelopeFollower>& envelopes;
    std::map<int, int>& potToEnvelopeMap;
};

class ButtonManager {
public:
    ButtonManager(const uint8_t* primaryMuxPins, const uint8_t* secondaryMuxPins, uint8_t analogPin, PotentiometerManager* potentiometerManager);
    void initButtons();
    void processButtons(ButtonManagerContext& context);
private:
    void selectMux(uint8_t primary, uint8_t secondary);
    uint8_t readButton(uint8_t buttonIndex);
    void handleSingleButtonPress(uint8_t buttonIndex, ButtonManagerContext& context);
    void handleMultiButtonPress(uint8_t pressedButtons, ButtonManagerContext& context);
    const uint8_t* _primaryMuxPins;
    const uint8_t* _secondaryMuxPins;
    uint8_t _analogPin;
    PotentiometerManager* _potentiometerManager;
    bool buttonStates[NUM_BUTTONS];
    unsigned long lastDebounceTimes[NUM_BUTTONS];
    uint8_t activeEnvelopeIndex;
};

#endif // BUTTON_MANAGER_H