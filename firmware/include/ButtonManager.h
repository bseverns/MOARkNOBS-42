// Handles all button scanning and debouncing.
// Works with DisplayManager and ConfigManager to drive UI actions.
// Polled by firmware_main.cpp every frame.
//
// The button matrix gets hammered one row/column at a time through the
// multiplexers. A full 7x6 sweep runs every loop, and the 50 ms
// DEBOUNCE_DELAY keeps the ghosts at bay and sets how fast a press can
// register.
//
// Each switch feeds a tiny state machine. Short taps bubble through
// handleShortPress() → doSinglePressAction(), rapid doubles detour into
// handleDoublePress(), and sustained holds fire onLongPress() once before
// onRelease() cleans house. Those callbacks are how the rest of the firmware
// plugs in.

#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <map>
#include "DisplayManager.h"
#include "MIDITypes.h"
#include "EnvelopeFollower.h"
#include "ConfigManager.h"
#include "Utility.h"
#include "PotentiometerManager.h"
#include "Globals.h"
#include "Log.h"
#include "LEDManager.h"

// Optional: Enable detailed debug logging for development
#ifndef BUTTON_MANAGER_DEBUG
#define BUTTON_MANAGER_DEBUG 0
#endif
#if BUTTON_MANAGER_DEBUG
#define BM_DBG_PRINT(...) LOG_PRINT(__VA_ARGS__)
#define BM_DBG_PRINTLN(...) LOG_PRINTLN(__VA_ARGS__)
#else
#define BM_DBG_PRINT(...)
#define BM_DBG_PRINTLN(...)
#endif

// Total number of multiplexed "virtual" buttons
inline constexpr uint8_t NUM_VIRTUAL_BUTTONS = 42;
// Total number of direct hardware control buttons
inline constexpr uint8_t NUM_CONTROL_BUTTONS = 6;
// Debounce period in milliseconds
inline constexpr unsigned long DEBOUNCE_DELAY = 50;
// Button matrix layout (rows x columns)
inline constexpr uint8_t BUTTON_ROWS = 7;
inline constexpr uint8_t BUTTON_COLS = 6;
// Analog read threshold for detecting a pressed button
inline constexpr int BUTTON_PRESS_THRESHOLD = 512;

/**
 * States for each button in the debounce & press state machine.
 */
enum class ButtonState {
    IDLE,       //!< No press detected
    PRESSED,    //!< Button is pressed but not yet long-pressed
    LONG_PRESS, //!< Long press threshold reached
    RELEASED    //!< Button has been released
};

/**
 * @brief Per-button state machine data.
 */
struct ButtonStateMachine {
    ButtonState state = ButtonState::IDLE;
    unsigned long pressTimestamp = 0;   // When button first pressed
    unsigned long releaseTimestamp = 0; // When button released
    bool longPressFired = false;        // Ensures long-press event only fires once
    unsigned long lastShortRelease = 0; // Timestamp of last release for double-press detection
};

/**
 * @brief Aggregated references passed to ::processButtons().
 *
 * The context contains all mutable state shared between the
 * ButtonManager and the rest of the application so that button events
 * can modify system behaviour without the class needing global
 * variables.
 */
struct ButtonManagerContext {
    std::vector<uint8_t> &potChannels;        // Mapping of pot indices to MIDI channels
    uint8_t &activePot;                       // Currently selected potentiometer index
    uint8_t &activeChannel;                   // MIDI channel to send CC on
    bool &envelopeFollowMode;                 // Flag: envelope-following mode active
    const char *&envelopeMode;                // Envelope mode display
    ConfigManager &configManager;             // For loading/saving persistent settings
    LEDManager &ledManager;                   // For updating visual feedback LEDs
    DisplayManager &displayManager;           // For writing status to OLED
    std::vector<EnvelopeFollower> &envelopes;         // List of envelope follower objects
    std::map<int, MIDISlot::EfSettings> &potToEnvelopeMap; // Associative map: pot -> EF settings
    bool &diagnosticMode;                     // Self-test mode flag
    uint8_t &diagnosticPage;                  // Which diagnostic page to show
};

/**
 * @brief Handles scanning and interpreting all physical and virtual buttons.
 *
 * The manager abstracts away the multiplexing hardware and exposes a
 * high level event interface.  Use ::processButtons regularly in the
 * main loop to update the state machines for every button.
 */
class ButtonManager {
  public:
    /**
     * Create a manager for all button inputs.
     * The mux pin arrays define the scanning hardware and the
     * PotentiometerManager link allows button presses to change slots.
     */
    ButtonManager(const HardwareConfig &config, const uint8_t *controlPins,
                  PotentiometerManager *potentiometerManager);

    /**
     * Configure the GPIO directions for all buttons.
     * Call once from setup() before processButtons() is used.
     */
    void initButtons();

    /**
     * Poll the button matrix, update state machines and fire callbacks.
     * Invoke this in the main loop with a shared ButtonManagerContext.
     */
    void processButtons(ButtonManagerContext &context);

    /**
     * Directly read a muxed button's state; useful for unit tests and safe to
     * call on a const ButtonManager.
     */
    bool isMuxButtonPressed(uint8_t index) const;

    /**
     * Peek at the control pots and buttons without running the whole
     * ::processButtons loop.  Handy for tests, but normal code should
     * let processButtons() do the heavy lifting.
     */
    void scanControlInputs(ButtonManagerContext &context);

#if defined(UNIT_TEST)
    /**
     * Test-only shim that exposes the private control button reader so Unity
     * specs can assert on the currently installed digital provider. Wrapped in
     * UNIT_TEST to avoid expanding the runtime API footprint.
     */
    bool readControlButtonForTest(uint8_t buttonIndex) { return readControlButton(buttonIndex); }
#endif

  private:
    // Mux select pins & analog input for virtual buttons scan
    const HardwareConfig &_cfg;
    // Direct control button pins
    const uint8_t *_controlPins; // direct GPIOs (legacy, unused with mux scan)
    // Link to PotentiometerManager for mode switching
    PotentiometerManager *_potentiometerManager;

    // Debounce & last-press tracking for all buttons
    bool buttonStates[NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS] = {false};
    unsigned long lastDebounceTimes[NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS] = {0};

    // Current UI mode (e.g., CC vs ENV vs ARG)
    uint8_t activeMode = 0;
    uint8_t activeARGMethod = 0;
    uint8_t argEnvelopeA = 0;
    uint8_t argEnvelopeB = 0;

    // State machines for each button detection
    ButtonStateMachine _buttonMachines[NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS];

    /** Set the multiplexer address lines so a given row/column can be read. */
    void selectMux(uint8_t row, uint8_t col);

    /** Return the digital state for a multiplexed button. */
    uint8_t readMuxButton(uint8_t buttonIndex) const;

    /** Read a direct control button pin (legacy non-mux input). */
    bool readControlButton(uint8_t buttonIndex);

    /** Handle the action for a single short press after debouncing. */
    void handleSingleButtonPress(uint8_t buttonIndex, ButtonManagerContext &context);

    /** Optional hook for combination presses (e.g. SHIFT + button). */
    void handleMultiButtonPress(uint8_t pressedButtons, ButtonManagerContext &context);

    /** Internal state machine driving press/hold/release detection. */
    void updateButtonStateMachine(uint8_t index, bool pressed, ButtonManagerContext &context);

    /** Arm a long‑press action and wait for a confirm tap. */
    void onLongPress(uint8_t index, ButtonManagerContext &context);

    /** Actually perform the long‑press action once confirmed. */
    void performLongPressAction(uint8_t index, ButtonManagerContext &context);

    /** Called when the button is released after press or long-press. */
    void onRelease(uint8_t index, ButtonManagerContext &context);

    /** Detect and dispatch short vs double presses based on timing. */
    void handleShortPress(uint8_t index, ButtonManagerContext &context);
    void handleDoublePress(uint8_t index, ButtonManagerContext &context);

    /** Perform the mapped action for a simple press. */
    void doSinglePressAction(uint8_t index, ButtonManagerContext &context);

    // ---- New multiplexer-based control scanning ----
    /** Update a single control button state during scanning. */
    void updateCtrlButton(uint8_t index, bool pressed, ButtonManagerContext &context);
    void cancelPendingConfirm(ButtonManagerContext &context);
    void startWarningForIndex(uint8_t index, ButtonManagerContext &context);
    int _ctrlPotValues[3] = {0};

    // Long‑press confirmation tracking
    static constexpr unsigned long CONFIRM_WINDOW_MS = 2000; // fat‑finger safety net
    int8_t _confirmIndex = -1;                               // which button waits for confirmation
    unsigned long _confirmDeadline = 0;                      // when the confirm window expires

    // Pending envelope follower assignment after a slot long press
    static constexpr unsigned long EF_ASSIGN_WINDOW_MS = 3000; // window to pick EF 0-5
    int _pendingEfSlot = -1;                                   // slot waiting for EF selection
    unsigned long _efAssignDeadline = 0;                       // cancel time for pending assignment

  public:
    /** Return the latest smoothed value for one of the control pots. */
    int getControlPotValue(uint8_t idx) const { return (idx < 3) ? _ctrlPotValues[idx] : 0; }
};

#endif // BUTTON_MANAGER_H
