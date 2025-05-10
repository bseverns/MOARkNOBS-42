#include "ButtonManager.h"
#include "EnvelopeFollower.h"
#include "Globals.h"
#include "ConfigManager.h"
#include "Utility.h"
#include <map>

extern std::vector<EnvelopeFollower> envelopeFollowers;
extern ButtonManagerContext buttonContext;
extern ConfigManager configManager;

// A debug flag for local logs if desired
#define BM_DEBUG 1
#if BM_DEBUG
  #define BM_DBG_PRINT(x)   Serial.print(x)
  #define BM_DBG_PRINTLN(x) Serial.println(x)
#else
  #define BM_DBG_PRINT(x)
  #define BM_DBG_PRINTLN(x)
#endif

static const unsigned long LONG_PRESS_DELAY   = 500;
static const unsigned long DOUBLE_PRESS_DELAY = 300;

static const int NUM_ARG_PAIRS = sizeof(ARG_PAIRS) / sizeof(ARG_PAIRS[0]);

// We also need a quick reference to the analog pins used by each EF index:
static const int EF_PINS[6] = { A0, A1, A2, A3, A6, A7 };

static const EnvelopeFollower::FilterType ALL_FILTERS[] = {
    EnvelopeFollower::LINEAR,
    EnvelopeFollower::OPPOSITE_LINEAR,
    EnvelopeFollower::EXPONENTIAL,
    EnvelopeFollower::RANDOM,
    EnvelopeFollower::LOWPASS,
    EnvelopeFollower::HIGHPASS,
    EnvelopeFollower::BANDPASS
};
static const char* FILTER_TYPE_NAMES[] = {
    "LINEAR", "OPPOSITE_LINEAR", "EXPONENTIAL", "RANDOM",
    "LOWPASS", "HIGHPASS", "BANDPASS"
};

static const int NUM_FILTER_TYPES = sizeof(ALL_FILTERS) / sizeof(ALL_FILTERS[0]);

// We'll track which filter index each EnvelopeFollower (e.g. 6 total) is using:
static int filterTypeIndexForEF[6] = {0, 0, 0, 0, 0, 0};

// Constructor
ButtonManager::ButtonManager(const uint8_t* primaryMuxPins,
                             const uint8_t* secondaryMuxPins,
                             uint8_t muxAnalogPin,
                             const uint8_t* controlPins,
                             PotentiometerManager* potentiometerManager)
    : _primaryMuxPins(primaryMuxPins),
      _secondaryMuxPins(secondaryMuxPins),
      _muxAnalogPin(muxAnalogPin),
      _controlPins(controlPins),
      _potentiometerManager(potentiometerManager),
      activeMode(0),
      activeARGMethod(0),
      argEnvelopeA(0),
      argEnvelopeB(1)
{
    for (int i = 0; i < NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS; i++) {
        buttonStates[i] = false;
        lastDebounceTimes[i] = 0;
    }
}

// We call this once at setup, just like your original approach:
void ButtonManager::initButtons() {
    for (int i = 0; i < 3; i++) {
        pinMode(_primaryMuxPins[i], OUTPUT);
        pinMode(_secondaryMuxPins[i], OUTPUT);
    }
    pinMode(analogPin, INPUT);

    for (int i = 0; i < NUM_CONTROL_BUTTONS; i++) {
        pinMode(_controlPins[i], INPUT_PULLUP);
    }

    // optional: initialize each state machine for each button
    for (int i = 0; i < (NUM_VIRTUAL_BUTTONS + NUM_CONTROL_BUTTONS); i++) {
        _buttonMachines[i].state = ButtonState::IDLE;
        _buttonMachines[i].pressTimestamp = 0;
        _buttonMachines[i].releaseTimestamp = 0;
        _buttonMachines[i].longPressFired = false;
        _buttonMachines[i].lastShortRelease = 0;
    }
}

/**
 * One unified processButtons loop:
 *  - For each virtual (mux) button, read & debounce
 *    -> update state machine
 *  - For each control (direct) button, read & debounce
 *    -> update state machine
 */
void ButtonManager::processButtons(ButtonManagerContext& context) {
    unsigned long now = millis();

    // Process virtual (multiplexer) buttons
    for (uint8_t i = 0; i < NUM_VIRTUAL_BUTTONS; i++) {
        uint8_t rawState = readMuxButton(i);
        bool stableReading = Utility::debounce(buttonStates[i], rawState,
                                               lastDebounceTimes[i], now,
                                               DEBOUNCE_DELAY);

        if (stableReading) {
            // interpret 'pressed' as buttonStates[i] == HIGH or LOW, whichever is your design
            bool pressed = (buttonStates[i] == HIGH);
            updateButtonStateMachine(i, pressed, context);
        }
    }

    // Process direct control buttons
    for (uint8_t i = 0; i < NUM_CONTROL_BUTTONS; i++) {
        bool currentState = readControlButton(i);
        bool stableReading = Utility::debounce(buttonStates[NUM_VIRTUAL_BUTTONS + i],
                                               currentState,
                                               lastDebounceTimes[NUM_VIRTUAL_BUTTONS + i],
                                               now, DEBOUNCE_DELAY);
        if (stableReading) {
            bool pressed = buttonStates[NUM_VIRTUAL_BUTTONS + i];
            updateButtonStateMachine(NUM_VIRTUAL_BUTTONS + i, pressed, context);
        }
    }
}

/**
 * The new state machine approach for short vs. long press.
 */
void ButtonManager::updateButtonStateMachine(uint8_t index, bool pressed, ButtonManagerContext& context) {
    ButtonStateMachine &sm = _buttonMachines[index];
    unsigned long now = millis();

    switch (sm.state) {
    case ButtonState::IDLE:
        if (pressed) {
            sm.state = ButtonState::PRESSED;
            sm.pressTimestamp = now;
            sm.longPressFired = false;
        }
        break;

    case ButtonState::PRESSED:
        if (!pressed) {
            // short release
            sm.state = ButtonState::RELEASED;
            sm.releaseTimestamp = now;
            displayManager.registerInteraction()
        } else {
            // still pressed, check for long press
            if (!sm.longPressFired && (now - sm.pressTimestamp >= LONG_PRESS_DELAY)) {
                sm.state = ButtonState::LONG_PRESS;
                sm.longPressFired = true;
                onLongPress(index, context); // handle immediate logic for a recognized long press
            }
        }
        break;

    case ButtonState::LONG_PRESS:
        // remains pressed
        if (!pressed) {
            // user just released after a long press
            sm.state = ButtonState::RELEASED;
            sm.releaseTimestamp = now;
            displayManager.registerInteraction()
        }
        break;

    case ButtonState::RELEASED:
        onRelease(index, context);
        sm.state = ButtonState::IDLE;
        break;
    }
}

/**
 * Called as soon as we confirm a long press.
 */
void ButtonManager::onLongPress(uint8_t index, ButtonManagerContext& context)
{
    if (index < NUM_VIRTUAL_BUTTONS) {
        // Long Press (Slot Button): Assign the selected slot to an EF, or cycle which EF is assigned
        auto it = context.potToEnvelopeMap.find(index);
        if (it == context.potToEnvelopeMap.end()) {
            context.potToEnvelopeMap[index] = 0; // Assign EF0
        } else {
            int currentEF = it->second;
            int nextEF = (currentEF + 1) % context.envelopes.size();
            it->second = nextEF;
        }
        int assigned = context.potToEnvelopeMap[index];
        context.envelopes[assigned].toggleActive(true);

        char buf[32];
        sprintf(buf, "Long: Slot %d->EF %d", index, assigned);
        context.displayManager.displayStatus(buf, 1500);
    }
    else {
        // Could do something else if a control button is long-pressed
        char msg[32];
        sprintf(msg, "LongPress Ctrl %d", index - NUM_VIRTUAL_BUTTONS);
        context.displayManager.displayStatus(msg, 1000);
    }
}

/**
 * Called after the user releases (short or long). If it wasn't a long press, we treat it as short press.
 */
void ButtonManager::onRelease(uint8_t index, ButtonManagerContext& context) {
    auto &sm = _buttonMachines[index];
    if (!sm.longPressFired) {
        // It's a short press
        handleShortPress(index, context);
    } else {
        // We had a long press
        // If you want a separate 'long press release' action, do it here.
    }
}

/**
 * If short press, we see if it's a double press or single press.
 */
void ButtonManager::handleShortPress(uint8_t index, ButtonManagerContext& context) {
    auto &sm = _buttonMachines[index];
    unsigned long now = millis();

    // Double-press detection
    if ((now - sm.lastShortRelease) < DOUBLE_PRESS_DELAY) {
        handleDoublePress(index, context);
        sm.lastShortRelease = 0;
    } else {
        doSinglePressAction(index, context);
        sm.lastShortRelease = now;
    }
}

/**
 * Double press logic
 */
void ButtonManager::handleDoublePress(uint8_t index, ButtonManagerContext& context)
{
    // If user double-pressed a slot button (0..41)
    if (index < NUM_VIRTUAL_BUTTONS) {
        auto it = context.potToEnvelopeMap.find(index);
        if (it == context.potToEnvelopeMap.end()) {
            context.displayManager.displayStatus("No EF assigned", 1000);
            return;
        }
        int efIndex = it->second;

        // Move to next filter index for that EF
        filterTypeIndexForEF[efIndex] = (filterTypeIndexForEF[efIndex] + 1) % NUM_FILTER_TYPES;

        // Retrieve the new filter type
        EnvelopeFollower::FilterType newType = ALL_FILTERS[filterTypeIndexForEF[efIndex]];
        // Apply it
        context.envelopes[efIndex].setFilterType(newType);

        // Feedback
        const char* filterName = FILTER_TYPE_NAMES[filterTypeIndexForEF[efIndex]];
        char msg[32];
        sprintf(msg, "Slot %d => %s", index, filterName);
        context.displayManager.displayStatus(msg, 1500);
    }
    else {
        // Double-press on a control button
        uint8_t cIndex = index - NUM_VIRTUAL_BUTTONS;
        switch (cIndex) {
            case 0: {
                // Double Press (Ctrl #0): Cycle EF filter forward
                auto it = context.potToEnvelopeMap.find(context.activePot);
                if (it == context.potToEnvelopeMap.end()) {
                    context.displayManager.displayStatus("No EF assigned", 1000);
                    return;
                }
                int efIndex = it->second;
                filterTypeIndexForEF[efIndex] = (filterTypeIndexForEF[efIndex] + 1) % NUM_FILTER_TYPES;

                EnvelopeFollower::FilterType newType = ALL_FILTERS[filterTypeIndexForEF[efIndex]];
                context.envelopes[efIndex].setFilterType(newType);

                const char* name = FILTER_TYPE_NAMES[filterTypeIndexForEF[efIndex]];
                char msg[32];
                sprintf(msg, "Slot %d => %s", context.activePot, name);
                context.displayManager.displayStatus(msg, 1500);
                break;
            }

            case 1: {
                // Double Press (Ctrl #1): Cycle EF filter backward
                // [CHANGED] => use activePot instead of 'index', and properly wrap negative
                auto it = context.potToEnvelopeMap.find(context.activePot);
                if (it == context.potToEnvelopeMap.end()) {
                    context.displayManager.displayStatus("No EF assigned", 1000);
                    return;
                }
                int efIndex = it->second;

                // Safely move backward by adding NUM_FILTER_TYPES - 1
                filterTypeIndexForEF[efIndex] =
                    (filterTypeIndexForEF[efIndex] + NUM_FILTER_TYPES - 1) % NUM_FILTER_TYPES;

                EnvelopeFollower::FilterType newType = ALL_FILTERS[filterTypeIndexForEF[efIndex]];
                context.envelopes[efIndex].setFilterType(newType);

                const char* name = FILTER_TYPE_NAMES[filterTypeIndexForEF[efIndex]];
                char msg[32];
                sprintf(msg, "Slot %d => %s", context.activePot, name);
                context.displayManager.displayStatus(msg, 1500);
                break; // <--- ensure we break out of case 1
            }

            case 4: {
                // Double Press (Ctrl #4): Undo unsaved changes (reset EEPROM)
                context.configManager.loadConfiguration(context.potChannels);
                context.displayManager.displayStatus("EEPROM Reset!", 1500);
                break;
            }

            case 5: {
                // Double Press (Ctrl #5): Save configuration
                context.configManager.saveConfiguration();
                context.configManager.saveEnvelopeSettings(context.potToEnvelopeMap, context.envelopes);
                context.displayManager.displayStatus("Config Saved!", 1500);
                break;
            }

            default:
                context.displayManager.displayStatus("DoublePress ???", 1000);
                break;
        }
    }
}

/**
 * The real single-press logic calls your *original* handleSingleButtonPress.
 */
void ButtonManager::doSinglePressAction(uint8_t index, ButtonManagerContext& context) {
    BM_DBG_PRINTLN("Single Press on button " + String(index));

    // old logic:
    handleSingleButtonPress(index, context);
}

void ButtonManager::handleSingleButtonPress(uint8_t buttonIndex, ButtonManagerContext& context)
{
    // If it's a virtual "slot" button (0..41)
    if (buttonIndex < NUM_VIRTUAL_BUTTONS) {
        // Make that pot (slot) the “active slot.”
        context.activePot = buttonIndex;
        context.displayManager.displayStatus(("Active Slot=" + String(buttonIndex)).c_str(), 1000);
        return;
    }

    // Otherwise, it's a control button
    uint8_t controlIndex = buttonIndex - NUM_VIRTUAL_BUTTONS;
    switch (controlIndex) {
        case 0:
            // Short Press (Control Button #0): Toggle EF On/Off
            context.envelopeFollowMode = !context.envelopeFollowMode;
            context.displayManager.displayStatus(
                context.envelopeFollowMode ? "EF: ON" : "EF: OFF",
                1500
            );
            break;

        case 1: {
            // Short Press (Control Button #1): Select next slot
            context.activePot = (context.activePot + 1) % NUM_POTS;
            context.displayManager.displayStatus(
                ("Next Slot=" + String(context.activePot)).c_str(), 1500);
        }
            break;

        case 2: {
            // Short Press (Control Button #2): Cycle Envelope to follow [if EF on]
            if (!context.envelopeFollowMode) {
                context.displayManager.displayStatus("EF is OFF", 1000);
                break;
            }

            // If EF is on, cycle to the next EF for the active slot
            auto it = context.potToEnvelopeMap.find(context.activePot);
            if (it == context.potToEnvelopeMap.end()) {
                // not assigned yet => assign EF0
                context.potToEnvelopeMap[context.activePot] = 0;
            } else {
                int currentEF = it->second;
                int nextEF = (currentEF + 1) % context.envelopes.size();
                it->second = nextEF;
            }
            int assigned = context.potToEnvelopeMap[context.activePot];
            context.envelopes[assigned].toggleActive(true);

            char buf[32];
            sprintf(buf, "Slot %d -> EF %d", context.activePot, assigned);
            context.displayManager.displayStatus(buf, 1500);
        }
        break;

        case 3: {
            // Short Press (Control Button #3): Cycle the active slot’s MIDI channel 1..16
            uint8_t oldChan = context.configManager.getPotChannel(context.activePot);
            uint8_t newChan = (oldChan % 16) + 1;  // cycles 1..16
            context.configManager.setPotChannel(context.activePot, newChan);

            char buf[32];
            sprintf(buf, "Slot %d => Ch %d", context.activePot, newChan);
            context.displayManager.displayStatus(buf, 1500);
        }
        break;

        case 4: {
            // Short Press (Control Button #4): Cycle the active slot’s CC number
            uint8_t oldCC = context.configManager.getPotCCNumber(context.activePot);
            uint8_t newCC = (oldCC + 1) % 128; // 0..127
            context.configManager.setPotCCNumber(context.activePot, newCC);

            char buf[32];
            sprintf(buf, "Slot %d => CC %d", context.activePot, newCC);
            context.displayManager.displayStatus(buf, 1500);
        }
        break;

        case 5: {
            // Short Press (Control Button #5): Tapped BPM
            static unsigned long lastTap = 0;
            unsigned long now = millis();
            if (lastTap != 0) {
                float intervalMs = (float)(now - lastTap);
                float newBPM = 60000.0f / intervalMs;
                char buf[32];
                snprintf(buf, sizeof(buf), "Tapped BPM=%.1f", newBPM);
                context.displayManager.displayStatus(buf, 1500);
            }
            lastTap = now;
        }
        break;

        default:
            context.displayManager.displayStatus("UNKNOWN CTRL BTN", 1000);
            break;
    }
}

void ButtonManager::handleMultiButtonPress(uint8_t pressedButtons, ButtonManagerContext& context) {
    // Define bit masks for the control buttons
    const uint8_t maskCtrl0 = 1 << 0;
    const uint8_t maskCtrl1 = 1 << 1;
    const uint8_t maskCtrl2 = 1 << 2;
    const uint8_t maskCtrl3 = 1 << 3;
    const uint8_t maskCtrl4 = 1 << 4;
    const uint8_t maskCtrl5 = 1 << 5;

    // (1) Ctrl0 + Ctrl1: Cycle EF’s ARG method if in ARG mode
    if ((pressedButtons & (maskCtrl0 | maskCtrl1)) == (maskCtrl0 | maskCtrl1)) {
        auto it = context.potToEnvelopeMap.find(context.activePot);
        if (it == context.potToEnvelopeMap.end()) {
            context.displayManager.displayStatus("No EF assigned", 1000);
            return;
        }
        int efIndex = it->second;
        EnvelopeFollower &env = context.envelopes[efIndex];
        if (env.getMode() != EnvelopeFollower::ARG) {
            context.displayManager.displayStatus("Not in ARG mode", 1000);
            return;
        }
        // Cycle through ARG methods (using similar logic as before)
        static EnvelopeFollower::ARG_Method ALL_METHODS[] = {
            EnvelopeFollower::PLUS, EnvelopeFollower::MIN,
            EnvelopeFollower::PECK, EnvelopeFollower::SHAV,
            EnvelopeFollower::SQAR, EnvelopeFollower::BABS,
            EnvelopeFollower::TABS
        };
        static const char* NAMES[] = {"PLUS", "MIN", "PECK", "SHAV", "SQAR", "BABS", "TABS"};
        static int argMethodPos[6] = {0,0,0,0,0,0};

        argMethodPos[efIndex] = (argMethodPos[efIndex] + 1) % (sizeof(ALL_METHODS)/sizeof(ALL_METHODS[0]));
        env.setARGMethod(ALL_METHODS[argMethodPos[efIndex]]);
        char msg[32];
        sprintf(msg, "EF %d=>%s", efIndex, NAMES[argMethodPos[efIndex]]);
        context.displayManager.displayStatus(msg, 1500);
    }
    // (2) Ctrl2 + Ctrl3: Cycle light modes (unchanged)
    else if ((pressedButtons & (maskCtrl2 | maskCtrl3)) == (maskCtrl2 | maskCtrl3)) {
        static uint8_t currentLightMode = 0;
        currentLightMode = (currentLightMode + 1) % 4;
        context.ledManager.setModeDisplay(currentLightMode);
        char buf[32];
        sprintf(buf, "LightMode=%d", currentLightMode);
        context.displayManager.displayStatus(buf, 1500);
    }
    // (3) Ctrl4 + Ctrl5: Toggle EF on and randomly assign envelope
    else if ((pressedButtons & (maskCtrl4 | maskCtrl5)) == (maskCtrl4 | maskCtrl5)) {
        if (!context.envelopeFollowMode) {
            context.envelopeFollowMode = true;
            context.displayManager.displayStatus("EF turned ON", 1000);
        }
        int randomEF = random(context.envelopes.size());
        context.potToEnvelopeMap[context.activePot] = randomEF;
        context.envelopes[randomEF].toggleActive(true);
        char buf[32];
        sprintf(buf, "Slot %d->RandomEF %d", context.activePot, randomEF);
        context.displayManager.displayStatus(buf, 1500);
    }
}

/**
 * Implementation of reading from multiplexer (same as your old code).
 */
uint8_t ButtonManager::readMuxButton(uint8_t buttonIndex) {
    uint8_t row = buttonIndex / 8;
    uint8_t col = buttonIndex % 8;
    selectMux(row, col);
    int value = analogRead(_muxAnalogPin);
    return (value < 512) ? HIGH : LOW; // or invert if needed
}

/**
 * Implementation of reading direct buttons (same as old).
 */
bool ButtonManager::readControlButton(uint8_t buttonIndex) {
    return (digitalRead(_controlPins[buttonIndex]) == LOW);
}

void ButtonManager::selectMux(uint8_t row, uint8_t col) {
    for (int i = 0; i < 3; i++) {
        digitalWrite(_primaryMuxPins[i], (row >> i) & 1);
        digitalWrite(_secondaryMuxPins[i], (col >> i) & 1);
    }
}
