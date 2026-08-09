// Handles all button scanning and debouncing.
// Works with DisplayManager and ConfigManager to drive UI actions.
//
// The button matrix gets hammered one row/column at a time through the
// multiplexers. A full 7x6 sweep runs every loop, and the 50 ms
// DEBOUNCE_DELAY keeps the ghosts at bay and sets how fast a press can
// register.
//
// Each switch feeds a tiny state machine. Short taps bubble through
// handleShortPress() → doSinglePressAction(), rapid doubles detour into
// handleDoublePress(), and sustained holds fire onLongPress() once before
// onRelease() cleans house. Those callbacks are how the rest of the firmware
// plugs in.
//
// UI ACTION LEDGER — Button Actions Reference
// ===========================================
// Virtual buttons (0-41): Slot selection and EF assignment
//   - Short: Select slot as active
//   - Double: Cycle filter type forward (+1)
//   - Long+Confirm: Assign EF follower (then pick EF via Ctrl0-5)
//
// Control buttons (Ctrl0-Ctrl5):
//   Ctrl0 (EF Toggle):
//     - Short: Toggle EF mode ON/OFF
//     - Double: Cycle filter +1 for active slot
//     - Long+Confirm: Calibrate assigned EF baseline
//   Ctrl1 (Next Slot):
//     - Short: Advance to next slot (0→41→0)
//     - Double: Cycle filter -1 for active slot
//     - Long+Confirm: Reset profile from EEPROM
//   Ctrl2 (EF Cycle / ARP):
//     - Short: Cycle EF assignment for active slot
//     - Double: Cycle slot MIDI message type
//     - Long+Confirm (with Ctrl4): Toggle ARP edit mode
//     - Long+Confirm (with Ctrl3): Cycle swing presets
//   Ctrl3 (Channel / Panic):
//     - Short: Cycle MIDI channel 1-16
//     - Long+Confirm: EEPROM reset (destructive)
//     - Combo (Ctrl0+3): Set slot to SysEx
//     - Combo (Ctrl1+3): Set slot to RPN
//     - Combo (Ctrl2+3): Increment ARP base note
//     - Combo (Ctrl3+4): Cycle LED modes
//     - Combo (Ctrl3+5): Set slot to Program Change
//   Ctrl4 (CC / Light):
//     - Short: Cycle CC/NRPN number
//     - Long+Confirm: Save config to profile
//     - Combo (Ctrl0+4): Randomize EF assignment
//     - Combo (Ctrl1+4): Set slot to Aftertouch
//     - Combo (Ctrl2+4): Toggle ARP on/off
//     - Combo (Ctrl3+4): Cycle LED modes
//     - Combo (Ctrl4+5): Set slot to Note mode
//   Ctrl5 (BPM / Diag):
//     - Short: Tap BPM (or exit diagnostic mode)
//     - Long+Confirm: Enter/cycle diagnostic pages
//     - Combo (Ctrl0+5): Set slot to Pitch Bend
//     - Combo (Ctrl1+5): Toggle MIDI clock out
//     - Combo (Ctrl2+5): Set slot to NRPN
//     - Combo (Ctrl3+4+5): Toggle USB MIDI out
//     - Combo (Ctrl1+4+5): Toggle clock source (EXT follow / INT forced)
//     - Combo (Ctrl0+1+3): Toggle LFO quick-tune mode
//   On-device config mode:
//     - Combo (Ctrl0+2+3+5): Enter dedicated slot config editing mode
//     - Ctrl0/1: Prev/next slot
//     - Ctrl2: Cycle slot type
//     - Ctrl3: Cycle channel
//     - Ctrl4: Cycle data1 (CC/NRPN/RPN)
//     - CtrlPot0: Adjust global EF idle floor
//     - Ctrl5: Exit + autosave
//
// Multi-button combos (settle window: 80ms):
//   - Ctrl0+1+2: Panic reset to profile baseline
//   - Ctrl0+1: Cycle ARG method (if ARG enabled)
//   - Ctrl0+2: Cycle ARG envelope pair
//   - Ctrl0+3: Set slot to SysEx
//   - Ctrl0+4: Randomize EF assignment
//   - Ctrl0+5: Set slot to Pitch Bend
//   - Ctrl1+2: Cycle profiles A-D
//   - Ctrl1+3: Set slot to RPN
//   - Ctrl1+4: Set slot to Aftertouch
//   - Ctrl1+5: Toggle MIDI clock out
//   - Ctrl2+3: Increment ARP base note
//   - Ctrl2+4: Toggle ARP on/off
//   - Ctrl2+5: Set slot to NRPN
//   - Ctrl3+4: Cycle LED modes
//   - Ctrl3+5: Set slot to Program Change
//   - Ctrl4+5: Set slot to Note mode
//   - Ctrl3+4+5: Toggle USB MIDI out
//   - Ctrl1+4+5: Toggle clock source (EXT follow / INT forced)
//   - Ctrl0+1+3: Toggle LFO quick-tune mode
//   - Ctrl0+2+3+5: Toggle on-device config mode
//   - In LFO tune mode, Ctrl4 cycles internal route target for selected LFO
//   - In LFO tune mode, CtrlPot2 edits sync ratio (sync ON) or bipolar state (sync OFF)

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
#include "ProfileRuntimeRequests.h"
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

/*
States for each button in the debounce & press state machine.
*/
enum class ButtonState {
    IDLE,       // No press detected
    PRESSED,    // Button is pressed but not yet long-pressed
    LONG_PRESS, // Long press threshold reached
    RELEASED    // Button has been released
};

/*
Per-button state machine data.
*/
struct ButtonStateMachine {
    ButtonState state = ButtonState::IDLE;
    unsigned long pressTimestamp = 0;   // When button first pressed
    unsigned long releaseTimestamp = 0; // When button released
    bool longPressFired = false;        // Ensures long-press event only fires once
    unsigned long lastShortRelease = 0; // Timestamp of last release for double-press detection
};

/*
Aggregated references passed to ::processButtons().

The context contains all mutable state shared between the
ButtonManager and the rest of the application so that button events
can modify system behaviour without the class needing global
variables.
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
    std::vector<EnvelopeFollower> &envelopes; // List of envelope follower objects
    std::map<int, MIDISlot::EfSettings> &potToEnvelopeMap; // Associative map: pot -> EF settings
    bool &diagnosticMode;                                  // Self-test mode flag
    uint8_t &diagnosticPage;                               // Which diagnostic page to show
    ProfileRuntimeRequests &profileRequests; // Explicit main-loop reload/save mailbox
};

/*
Handles scanning and interpreting all physical and virtual buttons.

The manager abstracts away the multiplexing hardware and exposes a
high level event interface.  Use ::processButtons regularly in the
main loop to update the state machines for every button.
*/
class ButtonManager {
  public:
    /*
    Create a manager for all button inputs.
    The mux pin arrays define the scanning hardware and the
    PotentiometerManager link allows button presses to change slots.
    */
    ButtonManager(const HardwareConfig &config, const uint8_t *controlPins,
                  PotentiometerManager *potentiometerManager);

    /*
    Configure the GPIO directions for all buttons.
    Call once from setup() before processButtons() is used.
    */
    void initButtons();

    /*
    Poll the button matrix, update state machines and fire callbacks.
    Invoke this in the main loop with a shared ButtonManagerContext.
    */
    void processButtons(ButtonManagerContext &context);
    bool isOnDeviceConfigModeActive() const { return _onDeviceConfigModeActive; }
    bool isLfoTuningModeActive() const { return _lfoTuningActive; }
    uint8_t lfoTuningIndex() const { return _lfoTuningIndex; }

    /*
    Directly read a muxed button's state; useful for unit tests and safe to
    call on a const ButtonManager.
    */
    bool isMuxButtonPressed(uint8_t index) const;

    /*
    Peek at the control pots and buttons without running the whole
    ::processButtons loop.  Handy for tests, but normal code should
    let processButtons() do the heavy lifting.
    */
    void scanControlInputs(ButtonManagerContext &context);

#if defined(UNIT_TEST)
    /*
    Test-only shim that exposes the private control button reader so Unity
    specs can assert on the currently installed digital provider. Wrapped in
    UNIT_TEST to avoid expanding the runtime API footprint.
    */
    bool readControlButtonForTest(uint8_t buttonIndex) { return readControlButton(buttonIndex); }

    // Test-only seam for directly seeding the smoothed control-pot cache.
    void setControlPotValueForTest(uint8_t idx, int value) {
        if (idx < 3) {
            _ctrlPotValues[idx] = value;
        }
    }
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
    bool lastRawButtonStates[NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS] = {false};
    unsigned long lastDebounceTimes[NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS] = {0};

    // Current UI mode (e.g., CC vs ENV vs ARG)
    uint8_t activeMode = 0;

    // State machines for each button detection
    ButtonStateMachine _buttonMachines[NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS];

    // Set the multiplexer address lines so a given row/column can be read.
    void selectMux(uint8_t row, uint8_t col);

    // Return the digital state for a multiplexed button.
    uint8_t readMuxButton(uint8_t buttonIndex) const;

    // Read a direct control button pin (legacy non-mux input).
    bool readControlButton(uint8_t buttonIndex);

    // Handle the action for a single short press after debouncing.
    void handleSingleButtonPress(uint8_t buttonIndex, ButtonManagerContext &context);

    // Optional hook for combination presses (e.g. SHIFT + button).
    void handleMultiButtonPress(uint8_t pressedButtons, ButtonManagerContext &context);

    // Internal state machine driving press/hold/release detection.
    void updateButtonStateMachine(uint8_t index, bool pressed, ButtonManagerContext &context);

    // Arm a long‑press action and wait for a confirm tap.
    void onLongPress(uint8_t index, ButtonManagerContext &context);

    // Actually perform the long‑press action once confirmed.
    void performLongPressAction(uint8_t index, ButtonManagerContext &context);

    // Called when the button is released after press or long-press.
    void onRelease(uint8_t index, ButtonManagerContext &context);

    // Detect and dispatch short vs double presses based on timing.
    void handleShortPress(uint8_t index, ButtonManagerContext &context);
    void handleDoublePress(uint8_t index, ButtonManagerContext &context);

    // Perform the mapped action for a simple press.
    void doSinglePressAction(uint8_t index, ButtonManagerContext &context);

    // ---- New multiplexer-based control scanning ----
    // Update a single control button state during scanning.
    void updateCtrlButton(uint8_t index, bool pressed, ButtonManagerContext &context);
    void enterOnDeviceConfigMode(ButtonManagerContext &context);
    void exitOnDeviceConfigMode(ButtonManagerContext &context, bool autosave);
    void markOnDeviceConfigDirty() { _onDeviceConfigModeDirty = true; }
    void enterLfoTuningMode(ButtonManagerContext &context);
    void exitLfoTuningMode(ButtonManagerContext &context);
    void cancelPendingConfirm(ButtonManagerContext &context);
    void startWarningForIndex(uint8_t index, ButtonManagerContext &context);
    int _ctrlPotValues[3] = {0};
    float _lastJitterDepth = -1.0f;
    float _lastJitterSmoothness = -1.0f;
    int16_t _lastConfigFloorPotBucket = -1;
    // Chord settle window: allows human fingers to land on multi-button combos reliably.
    // 80ms accommodates typical finger spread variance without introducing noticeable lag.
    static constexpr unsigned long COMBO_SETTLE_MS = 80;
    uint8_t _comboHoldMask = 0;             // Active combo mask for long-press tracking
    unsigned long _comboHoldTimestamp = 0;  // When the current combo started
    bool _comboLongPressFired = false;      // True once the combo long-press action fired
    uint8_t _comboCandidateMask = 0;        // Candidate short-combo mask while settling
    unsigned long _comboCandidateSince = 0; // Timestamp when current short-combo candidate started

    // Long‑press confirmation tracking
    static constexpr unsigned long CONFIRM_WINDOW_MS = 2000; // fat‑finger safety net
    int8_t _confirmIndex = -1;                               // which button waits for confirmation
    unsigned long _confirmDeadline = 0;                      // when the confirm window expires

    // Pending envelope follower assignment after a slot long press
    static constexpr unsigned long EF_ASSIGN_WINDOW_MS = 3000; // window to pick EF 0-5
    int _pendingEfSlot = -1;                                   // slot waiting for EF selection
    unsigned long _efAssignDeadline = 0;                       // cancel time for pending assignment
    bool _onDeviceConfigModeActive = false;
    bool _onDeviceConfigModeDirty = false;
    bool _lfoTuningActive = false;
    uint8_t _lfoTuningIndex = 0;
    float _lastLfoTuneFreqHz = -1.0f;
    float _lastLfoTuneDepth = -1.0f;
    int8_t _lastLfoTuneRatioIndex = -1;
    int8_t _lastLfoTuneBipolarState = -1;

  public:
    // Return the latest smoothed value for one of the control pots.
    int getControlPotValue(uint8_t idx) const { return (idx < 3) ? _ctrlPotValues[idx] : 0; }
};

#endif // BUTTON_MANAGER_H
