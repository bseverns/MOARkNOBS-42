#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "DisplayManager.h"
#include "EnvelopeFollower.h"
#include "ConfigManager.h"
#include "Utility.h"
#include "PotentiometerManager.h"

// Optional: a compile-time DEBUG flag
// Set to 1 for debug prints, 0 to remove them entirely from your build.
#define BUTTON_MANAGER_DEBUG 1

#if BUTTON_MANAGER_DEBUG
  #define BM_DBG_PRINT(x)   Serial.print(x)
  #define BM_DBG_PRINTLN(x) Serial.println(x)
#else
  #define BM_DBG_PRINT(x)
  #define BM_DBG_PRINTLN(x)
#endif

// Forward declaration (if needed)
struct ButtonManagerContext;

// Number of multiplexed & control buttons
#define NUM_VIRTUAL_BUTTONS 42
#define NUM_CONTROL_BUTTONS 6
#define DEBOUNCE_DELAY 50

/**
 * A simple state machine for each button to handle short press, long press, etc.
 */
enum class ButtonState {
    IDLE,
    PRESSED,
    LONG_PRESS,
    RELEASED
};

struct ButtonStateMachine {
    ButtonState state = ButtonState::IDLE;
    unsigned long pressTimestamp = 0;
    unsigned long releaseTimestamp = 0;
    bool longPressFired = false;
    unsigned long lastShortRelease = 0; // For double-press detection
};

// This is the same context struct you likely already have
// containing references to pot channels, display, etc.
struct ButtonManagerContext {
    std::vector<uint8_t>& potChannels;
    uint8_t& activePot;
    uint8_t& activeChannel;
    bool& envelopeFollowMode;
    ConfigManager &configManager;
    LEDManager &ledManager;
    DisplayManager &displayManager;
    std::vector<EnvelopeFollower> &envelopes;
    std::map<int, int>& potToEnvelopeMap;
};

/**
 * The main ButtonManager class
 */
class ButtonManager {
public:
    /**
     * Constructor
     */
    ButtonManager(const uint8_t* primaryMuxPins,
                  const uint8_t* secondaryMuxPins,
                  uint8_t muxAnalogPin,
                  const uint8_t* controlPins,
                  PotentiometerManager* potentiometerManager);

    /**
     * Initialize button pins, etc.
     */
    void initButtons();

    /**
     * Main update method—call this regularly in loop().
     * Processes both multiplexed virtual buttons & direct control buttons.
     */
    void processButtons(ButtonManagerContext& context);

private:
    // Pin references
    const uint8_t* _primaryMuxPins;
    const uint8_t* _secondaryMuxPins;
    uint8_t _muxAnalogPin;
    const uint8_t* _controlPins;
    PotentiometerManager* _potentiometerManager;

    // Bookkeeping
    bool buttonStates[NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS];
    unsigned long lastDebounceTimes[NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS];

    // (Optional) track mode, ARG method, envelope pairs, etc.
    uint8_t activeMode;
    uint8_t activeARGMethod;
    uint8_t argEnvelopeA;
    uint8_t argEnvelopeB;

    // For each control button, hold a state machine struct
    ButtonStateMachine _buttonMachines[NUM_CONTROL_BUTTONS + NUM_VIRTUAL_BUTTONS];

    /**
     * Helper to select row,col in multiplexer.
     */
    void selectMux(uint8_t row, uint8_t col);

    /**
     * Read one multiplexed virtual button.
     * Return HIGH if pressed, LOW if not (or vice versa).
     */
    uint8_t readMuxButton(uint8_t buttonIndex);

    /**
     * Read a control button connected directly to a GPIO pin.
     */
    bool readControlButton(uint8_t buttonIndex);

    /**
     * Called when a single button press (short press) is finalized.
     */
    void handleSingleButtonPress(uint8_t buttonIndex, ButtonManagerContext& context);

    /**
     * Called to handle multi-button combos (or we can adapt it for SHIFT combos).
     * You can keep or remove this as suits your system.
     */
    void handleMultiButtonPress(uint8_t pressedButtons);

    /**
     * State machine update for each button
     */
    void updateButtonStateMachine(uint8_t index, bool pressed, ButtonManagerContext& context);

    /**
     * Called when we detect a transition to a LONG_PRESS state.
     */
    void onLongPress(uint8_t index, ButtonManagerContext& context);

    /**
     * Called when the button is released.
     * We can check if it was a short press or a long press release.
     */
    void onRelease(uint8_t index, ButtonManagerContext& context);

    /**
     * If short press, we check if it's a double press or single press
     */
    void handleShortPress(uint8_t index, ButtonManagerContext& context);

    /**
     * Distinguish single press from double press
     */
    void handleDoublePress(uint8_t index, ButtonManagerContext& context);

    /**
     * Actual single-press logic
     */
    void doSinglePressAction(uint8_t index, ButtonManagerContext& context);
};

#endif // BUTTON_MANAGER_H