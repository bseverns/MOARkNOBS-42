#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <map>
#include "DisplayManager.h"
#include "EnvelopeFollower.h"
#include "ConfigManager.h"
#include "Utility.h"
#include "PotentiometerManager.h"

// Optional: Enable detailed debug logging for development
#define BUTTON_MANAGER_DEBUG 1
#if BUTTON_MANAGER_DEBUG
  #define BM_DBG_PRINT(x)   Serial.print(x)
  #define BM_DBG_PRINTLN(x) Serial.println(x)
#else
  #define BM_DBG_PRINT(x)
  #define BM_DBG_PRINTLN(x)
#endif

// Total number of multiplexed "virtual" buttons
#define NUM_VIRTUAL_BUTTONS 42
// Total number of direct hardware control buttons
#define NUM_CONTROL_BUTTONS 6
// Debounce period in milliseconds
#define DEBOUNCE_DELAY 50

/**
 * States for each button in the debounce & press state machine.
 */
enum class ButtonState {
    IDLE,        // No press detected
    PRESSED,     // Button is pressed (but not yet long-pressed)
    LONG_PRESS,  // Long press threshold reached
    RELEASED     // Button has been released
};

/**
 * Tracks timing and flags for each individual button.
 */
struct ButtonStateMachine {
    ButtonState state = ButtonState::IDLE;
    unsigned long pressTimestamp     = 0;  // When button first pressed
    unsigned long releaseTimestamp   = 0;  // When button released
    bool longPressFired              = false; // Ensures long-press event only fires once
    unsigned long lastShortRelease   = 0;  // Timestamp of last release for double-press detection
};

/**
 * Aggregated context passed into processButtons(), containing all
 * shared resources and state the ButtonManager needs to act.
 */
struct ButtonManagerContext {
    std::vector<uint8_t>& potChannels;          // Mapping of pot indices to CC channels
    uint8_t& activePot;                         // Currently selected potentiometer index
    uint8_t& activeChannel;                     // MIDI channel to send CC on
    bool& envelopeFollowMode;                   // Flag: envelope-following mode active
    ConfigManager& configManager;               // For loading/saving persistent settings
    LEDManager& ledManager;                     // For updating visual feedback LEDs
    DisplayManager& displayManager;             // For writing status to OLED
    std::vector<EnvelopeFollower>& envelopes;   // List of envelope follower objects
    std::map<int, int>& potToEnvelopeMap;       // Associative map: pot -> envelope index
};

/**
 * ButtonManager handles both:
 *  - Virtual buttons via external multiplexers
 *  - Direct control buttons wired to GPIO
 *
 * It manages debounce, short/long/double press detection, and
 * dispatches events into user code via the provided context.
 */
class ButtonManager {
public:
    /**
     * Constructor
     * @param primaryMuxPins   Array of GPIO pins controlling primary mux select lines
     * @param secondaryMuxPins Array of GPIO pins controlling secondary mux select lines
     * @param muxAnalogPin     Analog pin reading the mux output
     * @param controlPins      Array of direct GPIO pins for control buttons
     * @param potentiometerManager Pointer to PotentiometerManager (to sync mode changes)
     */
    ButtonManager(const uint8_t* primaryMuxPins,
                  const uint8_t* secondaryMuxPins,
                  uint8_t muxAnalogPin,
                  const uint8_t* controlPins,
                  PotentiometerManager* potentiometerManager);

    /**
     * Call once in setup() to configure pin modes for control buttons.
     */
    void initButtons();

    /**
     * Call in loop() to scan both virtual & control buttons,
     * update state machines, and trigger press events.
     * @param context    Aggregated references & state used for handling events
     */
    void processButtons(ButtonManagerContext& context);

private:
    // Mux select pins & analog input for virtual buttons scan
    const uint8_t* _primaryMuxPins;
    const uint8_t* _secondaryMuxPins;
    uint8_t _muxAnalogPin;
    // Direct control button pins
    const uint8_t* _controlPins;
    // Link to PotentiometerManager for mode switching
    PotentiometerManager* _potentiometerManager;

    // Debounce & last-press tracking for all buttons
    bool buttonStates[NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS] = {false};
    unsigned long lastDebounceTimes[NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS] = {0};

    // Current UI mode (e.g., CC vs ENV vs ARG)
    uint8_t activeMode      = 0;
    uint8_t activeARGMethod = 0;
    uint8_t argEnvelopeA    = 0;
    uint8_t argEnvelopeB    = 0;

    // State machines for each button detection
    ButtonStateMachine _buttonMachines[NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS];

    /**
     * Drive the mux select lines to read a specific row/column.
     */
    void selectMux(uint8_t row, uint8_t col);

    /**
     * Read the analog value for a virtual button index via the mux.
     * @return HIGH (unpressed) or LOW (pressed)
     */
    uint8_t readMuxButton(uint8_t buttonIndex);

    /**
     * Read a direct control button pin.
     * @return true if pressed (active LOW), false otherwise
     */
    bool readControlButton(uint8_t buttonIndex);

    /**
     * Handle a confirmed short press (single tap). Updates display and state.
     */
    void handleSingleButtonPress(uint8_t buttonIndex, ButtonManagerContext& context);

    /**
     * (Optional) handle combination presses, e.g. SHIFT+button
     */
    void handleMultiButtonPress(uint8_t pressedButtons, ButtonManagerContext& context);

    /**
     * Core state-machine logic for each button. Handles transitions between
     * IDLE, PRESSED, LONG_PRESS, RELEASED, and fires appropriate callbacks.
     */
    void updateButtonStateMachine(uint8_t index, bool pressed, ButtonManagerContext& context);

    /**
     * Called once when a button transitions into LONG_PRESS state.
     */
    void onLongPress(uint8_t index, ButtonManagerContext& context);

    /**
     * Called once when a button is released from PRESSED or LONG_PRESS.
     */
    void onRelease(uint8_t index, ButtonManagerContext& context);

    /**
     * Detect single vs double short-press based on release timing.
     */
    void handleShortPress(uint8_t index, ButtonManagerContext& context);
    void handleDoublePress(uint8_t index, ButtonManagerContext& context);

    /**
     * Actual action for a single-press event, separate for clarity.
     */
    void doSinglePressAction(uint8_t index, ButtonManagerContext& context);
};

#endif // BUTTON_MANAGER_H
