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

// Define the total number of multiplexed virtual buttons and control buttons
#define NUM_VIRTUAL_BUTTONS 42 // Virtual "pots" controlled by multiplexers
#define NUM_CONTROL_BUTTONS 6  // Direct GPIO-connected buttons
#define DEBOUNCE_DELAY 50      // Debounce delay in milliseconds

// Forward declaration to avoid circular dependencies
class ButtonManager;
class ConfigManager;
class DisplayManager;
class PotentiometerManager;

// Context structure to share key state variables
struct ButtonManagerContext {
    std::vector<uint8_t>& potChannels; // Reference to pot-to-MIDI channel mapping
    uint8_t& activePot;                // Currently active "pot" (button)
    uint8_t& activeChannel;            // Currently active MIDI channel
    bool& envelopeFollowMode;          // Whether Envelope Follower mode is active
    ConfigManager &configManager;      // Reference to ConfigManager for configuration handling
    LEDManager &ledManager;            // Reference to LEDManager for LED updates
    DisplayManager &displayManager;    // Reference to DisplayManager for UI updates
    std::vector<EnvelopeFollower> &envelopes; // List of EnvelopeFollower objects
    std::map<int, int>& potToEnvelopeMap;     // Map of pot index to envelope assignments
};

class ButtonManager {
public:
    /**
     * Constructor for ButtonManager.
     * @param primaryMuxPins: GPIO pins controlling the primary multiplexer.
     * @param secondaryMuxPins: GPIO pins controlling the secondary multiplexer.
     * @param muxAnalogPin: Analog pin reading multiplexer output.
     * @param controlPins: GPIO pins for direct control buttons.
     * @param potentiometerManager: Pointer to PotentiometerManager for CC updates.
     */
    ButtonManager(const uint8_t* primaryMuxPins, const uint8_t* secondaryMuxPins, uint8_t muxAnalogPin, const uint8_t* controlPins, PotentiometerManager* potentiometerManager);

    /**
     * Initialize all buttons (multiplexed and control).
     */
    void initButtons();

    /**
     * Process button states and handle events (virtual and control).
     * @param context: Shared state context for ButtonManager.
     */
    void processButtons(ButtonManagerContext& context);

private:
    /**
     * Select a row and column on the multiplexer.
     * @param row: Row number of the matrix.
     * @param col: Column number of the matrix.
     */
    void selectMux(uint8_t row, uint8_t col);

    /**
     * Read the state of a virtual button from the multiplexer.
     * @param buttonIndex: Index of the virtual button to read.
     * @return HIGH (pressed) or LOW (not pressed).
     */
    uint8_t readMuxButton(uint8_t buttonIndex);

    /**
     * Read the state of a control button connected directly to GPIO.
     * @param buttonIndex: Index of the control button to read.
     * @return true if pressed, false otherwise.
     */
    bool readControlButton(uint8_t buttonIndex);

    /**
     * Handle a single button press event.
     * @param buttonIndex: Index of the button that was pressed.
     * @param context: Shared state context for ButtonManager.
     */
    void handleSingleButtonPress(uint8_t buttonIndex, ButtonManagerContext& context);

    /**
     * Handle multi-button press combinations.
     * @param pressedButtons: Bitmask of pressed buttons.
     * @param context: Shared state context for ButtonManager.
     */
    void handleMultiButtonPress(uint8_t pressedButtons, ButtonManagerContext& context);

    // Private member variables
    const uint8_t* _primaryMuxPins;    // Pins controlling the primary multiplexer
    const uint8_t* _secondaryMuxPins;  // Pins controlling the secondary multiplexer
    uint8_t _muxAnalogPin;             // Analog pin for multiplexer output
    const uint8_t* _controlPins;       // GPIO pins for direct control buttons
    PotentiometerManager* _potentiometerManager; // Pointer to PotentiometerManager

    // States and debounce timing
    bool buttonStates[NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS]; // Button states
    unsigned long lastDebounceTimes[NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS]; // Debounce timestamps

    // Track active envelope index for specific button interactions
    uint8_t activeEnvelopeIndex;
};

#endif // BUTTON_MANAGER_H
