#include "ButtonManager.h"
#include "DisplayManager.h"
#include "EnvelopeFollower.h"
#include "Globals.h"
#include "ConfigManager.h"
#include "Utility.h"
#include <map>

extern DisplayManager displayManager;
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

// For each EnvelopeFollower index (0..5), we store the current (A,B) pair offset
static int argPairIndexForEF[6] = {0, 0, 0, 0, 0, 0};

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
void ButtonManager::onLongPress(uint8_t index, ButtonManagerContext& context) {
    // Detect if this is one of the 42 virtual pot buttons
    if (index < NUM_VIRTUAL_BUTTONS) {
        // If envelope follow mode isn't on globally, turn it on
        if (!context.envelopeFollowMode) {
            context.envelopeFollowMode = true;
            context.displayManager.displayStatus("EF: GLOBAL ON", 1500);
        }

        // Look up if this pot is already assigned to an EF
        auto it = context.potToEnvelopeMap.find(index);

        if (it == context.potToEnvelopeMap.end()) {
            // Not assigned to any EF yet, pick the first EF
            context.potToEnvelopeMap[index] = 0; // EF0
        } else {
            // Already assigned: cycle to the next EF in the vector
            int currentEF = it->second;
            int nextEF = (currentEF + 1) % context.envelopes.size();
            context.potToEnvelopeMap[index] = nextEF;
        }

        int assignedEF = context.potToEnvelopeMap[index];

        // Optionally ensure the chosen EF is toggled active
        context.envelopes[assignedEF].toggleActive(true);

        // Optionally set the pot's CC as that EF's modulation target
        // or let your existing logic do it. Example:
        //   int potCC = context.configManager.getPotCCNumber(index);
        //   context.envelopes[assignedEF].setModulationTarget(potCC);

        // Show updated assignment on display
        char buf[32];
        sprintf(buf, "POT %d -> EF %d", index, assignedEF);
        context.displayManager.displayStatus(buf, 2000);
    }
    else {
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
void ButtonManager::handleDoublePress(uint8_t index, ButtonManagerContext& context) {
    BM_DBG_PRINTLN("Double Press on button " + String(index));

    // e.g. a switch (index) { ... } for double-press actions, 
    // or call your existing multi-press logic
}

/**
 * The real single-press logic calls your *original* handleSingleButtonPress.
 */
void ButtonManager::doSinglePressAction(uint8_t index, ButtonManagerContext& context) {
    BM_DBG_PRINTLN("Single Press on button " + String(index));

    // old logic:
    handleSingleButtonPress(index, context);
}

void ButtonManager::handleSingleButtonPress(uint8_t buttonIndex, ButtonManagerContext& context) {
    // If this used to do the switch-case for pot vs. control button:
    if (buttonIndex < NUM_VIRTUAL_BUTTONS) {
        // old logic
        context.activePot = buttonIndex;
        context.displayManager.showText(
            ("Active BTN: " + String(buttonIndex)).c_str(),
            "Adjust w/ Master Pot",
            ""
        );
        return;
    }

    // If it's a control button
    uint8_t controlIndex = buttonIndex - NUM_VIRTUAL_BUTTONS;
    switch (controlIndex) {
        case 0: 
            // toggle EF
            context.envelopeFollowMode = !context.envelopeFollowMode;
            context.displayManager.displayStatus(context.envelopeFollowMode ? "EF ON" : "EF OFF", 2000);
            break;
        case 1:{
            // MIDI increment channel
            context.activeChannel = (context.activeChannel % 16) + 1;
            context.displayManager.displayStatus(("CHAN+ " + String(context.activeChannel)).c_str(), 2000);
            break;
            }
        case 2:{ 
            //EF<->ARG
        auto it = context.potToEnvelopeMap.find(context.activePot);
            if (it == context.potToEnvelopeMap.end()) {
                // no envelope assigned to this pot
                context.displayManager.displayStatus("No EF assigned", 1000);
                break;
            }
            int envIndex = it->second;
            EnvelopeFollower &env = context.envelopes[envIndex];

            // b) read the current mode, toggle it
            EnvelopeFollower::Mode oldMode = env.getMode();
            EnvelopeFollower::Mode newMode =
                (oldMode == EnvelopeFollower::SEF) ? EnvelopeFollower::ARG : EnvelopeFollower::SEF;
            env.setMode(newMode);

            // c) user feedback
            const char* modeString = (newMode == EnvelopeFollower::SEF) ? "SEF" : "ARG";
            char msg[32];
            sprintf(msg, "EF %d => %s", envIndex, modeString);
            context.displayManager.displayStatus(msg, 1500);
            break;
            } 
            case 3:{ 
                //ARG pair selection
                // 1) Find which EF is assigned to the active pot:
                auto it = context.potToEnvelopeMap.find(context.activePot);
                if (it == context.potToEnvelopeMap.end()) {
                    context.displayManager.displayStatus("No EF assigned", 1000);
                    break;
                }
                int envIndex = it->second;
                EnvelopeFollower &env = context.envelopes[envIndex];
            
                // 2) Check that EF is in ARG mode
                if (env.getMode() != EnvelopeFollower::ARG) {
                    context.displayManager.displayStatus("EF not in ARG mode!", 1000);
                    break;
                }
            
                // 3) Cycle the pair index for that EF
                argPairIndexForEF[envIndex] = (argPairIndexForEF[envIndex] + 1) % NUM_ARG_PAIRS;
            
                // 4) Apply the new pair to this EnvelopeFollower
                auto newPair = ARG_PAIRS[argPairIndexForEF[envIndex]];
                env.setEnvelopePair(newPair.first, newPair.second);
            
                // 5) Provide some feedback
                char msg[32];
                // e.g. "EF 2 => A=A1, B=A3" if envIndex=2 and new pair is (A1,A3)
                snprintf(msg, sizeof(msg),
                         "EF %d => A=%d, B=%d", envIndex, newPair.first, newPair.second);
                context.displayManager.displayStatus(msg, 1500);
                break;
    }
        case 4:{
            // Save all pot + EF settings to EEPROM
            context.configManager.saveConfiguration();
            context.configManager.saveEnvelopeSettings(context.potToEnvelopeMap, context.envelopes);

            context.displayManager.displayStatus("All settings saved!", 1500);
            break;
            }
            case 5: {
                //tap tempo
                static unsigned long lastTap = 0;
                unsigned long now = millis();
                if (lastTap != 0) {
                    float intervalMs = (float)(now - lastTap);
                    float newBPM = 60000.0 / intervalMs; // BPM = 60,000ms / beat-length
                    // set globalBPM = newBPM, or send a MIDI tempo
                    // ...
                    char buf[32];
                    snprintf(buf, sizeof(buf), "Tapped BPM=%.1f", newBPM);
                    context.displayManager.displayStatus(buf, 1500);
                }
                lastTap = now;
                break;
            }
        default:
            context.displayManager.displayStatus("UNKNOWN BTN", 1000);
            break;
    }
}

/**
 * Also keep your multi-button combos
 */
void ButtonManager::handleMultiButtonPress(uint8_t pressedButtons, ButtonManagerContext& context) {
    // The array indices for control buttons #0 and #1
    uint8_t c0Index = NUM_VIRTUAL_BUTTONS + 0; // 42
    uint8_t c1Index = NUM_VIRTUAL_BUTTONS + 1; // 43
    uint8_t c2Index = NUM_VIRTUAL_BUTTONS + 2; // 43
    uint8_t c3Index = NUM_VIRTUAL_BUTTONS + 3; // 43

    bool c0Pressed = (pressedButtons & (1 << c0Index)) != 0;
    bool c1Pressed = (pressedButtons & (1 << c1Index)) != 0;
    bool c2Pressed = (pressedButtons & (1 << c2Index)) != 0;
    bool c3Pressed = (pressedButtons & (1 << c3Index)) != 0;


    if (c0Pressed && c1Pressed) {
        // Both control #0 and control #1 are pressed.
        // Let's cycle the ARG method for the EF assigned to the currently active pot.

        // 1) See if there's an EF assigned to the active pot
        auto it = context.potToEnvelopeMap.find(context.activePot);
        if (it == context.potToEnvelopeMap.end()) {
            context.displayManager.displayStatus("No EF assigned", 1000);
            return;
        }
        int envIndex = it->second;
        EnvelopeFollower &env = context.envelopes[envIndex];

        // 2) Check if that EF is in ARG mode
        if (env.getMode() != EnvelopeFollower::ARG) {
            context.displayManager.displayStatus("Not in ARG mode!", 1000);
            return;
        }

        // 3) Cycle to the next ARG method
        static const EnvelopeFollower::ARG_Method ALL_METHODS[] = {
            EnvelopeFollower::PLUS, EnvelopeFollower::MIN,
            EnvelopeFollower::PECK, EnvelopeFollower::SHAV,
            EnvelopeFollower::SQAR, EnvelopeFollower::BABS,
            EnvelopeFollower::TABS
        };
        static const char* METHOD_NAMES[] = {
            "PLUS", "MIN", "PECK", "SHAV", "SQAR", "BABS", "TABS"
        };
        static const int NUM_METHODS = sizeof(ALL_METHODS) / sizeof(ALL_METHODS[0]);

        // We'll track each EF's current index in a small static array:
        static int argMethodIndexForEF[6] = {0,0,0,0,0,0};
        // (assuming you have 6 EnvelopeFollowers total)

        // Move to next
        argMethodIndexForEF[envIndex] = (argMethodIndexForEF[envIndex] + 1) % NUM_METHODS;
        EnvelopeFollower::ARG_Method newMethod = ALL_METHODS[argMethodIndexForEF[envIndex]];
        env.setARGMethod(newMethod);

        // 4) Show feedback
        char msg[32];
        snprintf(msg, sizeof(msg), "EF %d => %s", envIndex, METHOD_NAMES[argMethodIndexForEF[envIndex]]);
        context.displayManager.displayStatus(msg, 1500);
    }
    else if (c2Pressed && c3Pressed) {
        // 1) Find the EF assigned to the currently active pot
        auto it = context.potToEnvelopeMap.find(context.activePot);
        if (it == context.potToEnvelopeMap.end()) {
            // No EF assigned to this pot
            context.displayManager.displayStatus("No EF assigned", 1000);
            return;
        }
        int envIndex = it->second;
        EnvelopeFollower &env = context.envelopes[envIndex];

        // (Optional) If you only want to allow filter cycling in SEF mode:
        // if (env.getMode() != EnvelopeFollower::SEF) {
        //     context.displayManager.displayStatus("Not in SEF mode!", 1000);
        //     return;
        // }

        // 2) Advance the filterType index for that EF
        filterTypeIndexForEF[envIndex] = (filterTypeIndexForEF[envIndex] + 1) % NUM_FILTER_TYPES;
        auto newFilterType = ALL_FILTERS[filterTypeIndexForEF[envIndex]];

        // 3) Apply the new filter type
        env.setFilterType(newFilterType);

        // 4) Provide some feedback
        const char* filterName = FILTER_TYPE_NAMES[filterTypeIndexForEF[envIndex]];
        char msg[32];
        snprintf(msg, sizeof(msg), "EF %d => %s", envIndex, filterName);
        context.displayManager.displayStatus(msg, 1500);
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

/**
 * If you had them originally
 */
void ButtonManager::selectMux(uint8_t row, uint8_t col) {
    for (int i = 0; i < 3; i++) {
        digitalWrite(_primaryMuxPins[i], (row >> i) & 1);
        digitalWrite(_secondaryMuxPins[i], (col >> i) & 1);
    }
}