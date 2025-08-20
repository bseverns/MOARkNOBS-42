// Scans the 42-button matrix and control buttons.
// Sends events to ConfigManager, DisplayManager and other modules.
// Called in every loop of firmware_main.cpp.

#include "ButtonManager.h"
#include "EnvelopeFollower.h"
#include "Globals.h"
#include "ConfigManager.h"
#include "Utility.h"
#include "TimeUtils.h"
#include "Arpeggiator.h"
#include <map>

// Scans the button matrix and direct control buttons. Results are fed into
// DisplayManager, ConfigManager, EnvelopeFollower assignments and the
// Arpeggiator. This class ties user interaction to the rest of the system.

// The BTN_42 PCB connects 42 pushbuttons in a 7×6 diode matrix. Each row and
// column is wired to one channel of two CD74HC4067 analog multiplexers. The
// select lines for the row mux are labeled `MUXR1..4` and the column mux uses
// `MUXC1..4`. The firmware cycles these select lines and reads the shared
// analog node to detect button presses.

extern std::vector<EnvelopeFollower> envelopeFollowers;
extern ButtonManagerContext buttonContext;
extern ConfigManager configManager;

// Verbose logging rides on BUTTON_MANAGER_DEBUG. See ButtonManager.h for macros.

static const unsigned long LONG_PRESS_DELAY   = 500;
static const unsigned long DOUBLE_PRESS_DELAY = 300;

// Mirror the global ARG pair count so our math stays synced without recomputing.
static const int NUM_ARG_PAIRS = ARG_PAIRS_LEN;

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

// Active configuration profile stored in EEPROM
static uint8_t currentProfile = 0;

// Constructor
ButtonManager::ButtonManager(const HardwareConfig& config,
                             const uint8_t* controlPins,
                             PotentiometerManager* potentiometerManager)
    : _cfg(config),
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

// Called during setup to configure the multiplexers and reset the internal
// state machines that track button presses.
void ButtonManager::initButtons() {
    for (int i = 0; i < PRIMARY_MUX_PINS; i++) {
        pinMode(_cfg.muxrPins[i], OUTPUT);
    }
    for (int i = 0; i < SECONDARY_MUX_PINS; i++) {
        pinMode(_cfg.muxcPins[i], OUTPUT);
    }
    pinMode(_cfg.buttonMuxAnalogPin, INPUT);
    pinMode(_cfg.rowDriverPin, OUTPUT);
    digitalWrite(_cfg.rowDriverPin, LOW);


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
    unsigned long now = ::now();

    // Scan mux matrix row by row
    uint8_t rawStates[NUM_VIRTUAL_BUTTONS];
    for (uint8_t r = 0; r < BUTTON_ROWS; ++r) {
        digitalWrite(_cfg.rowDriverPin, HIGH);
        setMux(_cfg.muxrPins, r);
        delayMicroseconds(5);
        for (uint8_t c = 0; c < BUTTON_COLS; ++c) {
            setMux(_cfg.muxcPins, c);
            delayMicroseconds(5);
            int v = analogRead(_cfg.buttonMuxAnalogPin);
            rawStates[r * BUTTON_COLS + c] = (v < 512) ? HIGH : LOW;
        }
        digitalWrite(_cfg.rowDriverPin, LOW);
    }

    // Process virtual (multiplexer) buttons using scanned states
    for (uint8_t i = 0; i < NUM_VIRTUAL_BUTTONS; i++) {
        uint8_t rawState = rawStates[i];
        bool stableReading = Utility::debounce(buttonStates[i], rawState,
                                               lastDebounceTimes[i], now,
                                               DEBOUNCE_DELAY);

        if (stableReading) {
            bool pressed = (buttonStates[i] == HIGH);
            updateButtonStateMachine(i, pressed, context);
        }
    }

    // Control buttons & pots via spare mux channels
    scanControlInputs(context);
}

/**
 * The new state machine approach for short vs. long press.
 */
void ButtonManager::updateButtonStateMachine(uint8_t index, bool pressed, ButtonManagerContext& context) {
    ButtonStateMachine &sm = _buttonMachines[index];
    unsigned long now = ::now();

    // Kill any pending confirm if time ran out
    if (_confirmIndex >= 0 && now > _confirmDeadline) {
        _confirmIndex = -1;
    }

    // Smack pending confirm if some other button gets poked
    if (_confirmIndex >= 0 && index != _confirmIndex && pressed) {
        _confirmIndex = -1;
    }

    switch (sm.state) {
    case ButtonState::IDLE:
        if (pressed) {
            sm.state = ButtonState::PRESSED;
            sm.pressTimestamp = now;
            sm.longPressFired = false;
            if (index >= NUM_VIRTUAL_BUTTONS) {
                context.ledManager.triggerControlButton();
            }
        }
        break;

    case ButtonState::PRESSED:
        if (!pressed) {
            // short release
            sm.state = ButtonState::RELEASED;
            sm.releaseTimestamp = now;
            context.displayManager.registerInteraction();
        } else {
            // still pressed, check for long press
            if (!sm.longPressFired && (now - sm.pressTimestamp >= LONG_PRESS_DELAY)) {
                sm.state = ButtonState::LONG_PRESS;
                sm.longPressFired = true;
                onLongPress(index, context); // arm the action, wait for confirm
            }
        }
        break;

    case ButtonState::LONG_PRESS:
        // remains pressed
        if (!pressed) {
            // user just released after a long press
            sm.state = ButtonState::RELEASED;
            sm.releaseTimestamp = now;
            context.displayManager.registerInteraction();
        }
        break;

    case ButtonState::RELEASED:
        onRelease(index, context);
        sm.state = ButtonState::IDLE;
        break;
    }
}

/**
 * Called when a long press crosses the threshold; we just arm it.
 */
void ButtonManager::onLongPress(uint8_t index, ButtonManagerContext& context) {
    _confirmIndex = index;
    _confirmDeadline = ::now() + CONFIRM_WINDOW_MS;
    context.displayManager.displayStatus("Press again to confirm", 1000);
}

/**
 * Fire the actual long‑press payload after confirmation.
 */
void ButtonManager::performLongPressAction(uint8_t index, ButtonManagerContext& context) {
    // Slot buttons (0-41)
    if (index < NUM_VIRTUAL_BUTTONS) {
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
        context.displayManager.displayStatus("EF Assigned", 1000);
    }
    else {
        // Control buttons (0-5)
        uint8_t ctrlIdx = index - NUM_VIRTUAL_BUTTONS;
        switch (ctrlIdx) {
            case 0: { // Calibrate assigned envelope follower
                auto it = context.potToEnvelopeMap.find(context.activePot);
                if (it == context.potToEnvelopeMap.end()) {
                    context.displayManager.displayStatus("No EF assigned", 1000);
                    break;
                }
                int efIndex = it->second;
                context.envelopes[efIndex].calibrate(); // auto-saves baseline via ConfigManager
                context.displayManager.displayStatus("EF Calibrated", 1500);
                break;
            }
            case 2: { //Toggle Slot Active
                MIDISlot &slot = context.configManager.getSlot(context.activePot);
                slot.active = !slot.active;
                context.configManager.saveSlot(context.activePot, slot);
                char buf[32];
                sprintf(buf, "Slot %d %s", context.activePot, slot.active ? "ON" : "OFF");
                context.displayManager.displayStatus(buf, 1500);
                break;
            }
            case 3: // EEPROM reset now rides here
                context.configManager.loadConfiguration(context.potChannels);
                context.displayManager.displayStatus("EEPROM Reset", 1500);
                break;
            case 4: { // Save configuration to the active profile
                context.configManager.saveProfile(currentProfile);
                context.configManager.saveEnvelopeSettings(context.potToEnvelopeMap, context.envelopes);
                context.displayManager.displayStatus("Config Saved", 1500);
                break;
            }
            default:
                context.displayManager.displayStatus("No Long Action", 1000);
                break;
        }
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
    unsigned long now = ::now();

    if (_confirmIndex == index) {
        // Second tap confirms the long‑press move
        if (now <= _confirmDeadline) {
            performLongPressAction(index, context);
        }
        _confirmIndex = -1;
        return;
    }

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

            case 2: {
                // Double Press (Ctrl #2): Cycle MIDI message type
                MIDISlot &slot = context.configManager.getSlot(context.activePot);
                slot.type = static_cast<MIDIMessageType>((static_cast<int>(slot.type) + 1) % (static_cast<int>(MIDIMessageType::SysEx) + 1));
                context.configManager.saveSlot(context.activePot, slot);
                char buf[32];
                sprintf(buf, "Slot %d Type %d", context.activePot, static_cast<int>(slot.type));
                context.displayManager.displayStatus(buf, 1500);
                break;
            }

            case 4: {
                // Double Press (Ctrl #4): Reload current profile from EEPROM
                context.configManager.loadProfile(currentProfile);
                context.potChannels.clear();
                for (uint8_t i = 0; i < context.configManager.getNumPots(); ++i) {
                    context.potChannels.push_back(context.configManager.getPotChannel(i));
                }
                context.displayManager.displayStatus("Profile Reset!", 1500);
                break;
            }

            case 5: {
                // Intentionally left blank: no double-press stunt for Ctrl #5 now
                context.displayManager.displayStatus("No Double Action", 1000);
                break;
            }

            default:
                context.displayManager.displayStatus("Unknown double press", 1000);
                break;
        }
    }
}

/**
 * Dispatch a confirmed short press. The helper `handleSingleButtonPress` keeps
 * the actual per-button actions so other parts of the firmware can reuse them.
 */
void ButtonManager::doSinglePressAction(uint8_t index, ButtonManagerContext& context) {
    BM_DBG_PRINTLN("Single Press on button " + String(index));
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
            unsigned long now = ::now();
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

    // (0) Ctrl0 + Ctrl1 + Ctrl2: toggle USB MIDI output
    if ((pressedButtons & (maskCtrl0 | maskCtrl1 | maskCtrl2)) == (maskCtrl0 | maskCtrl1 | maskCtrl2)) {
        g_usbMidiOutEnabled = !g_usbMidiOutEnabled;
        context.displayManager.displayStatus(g_usbMidiOutEnabled ? "USB MIDI ON" : "USB MIDI OFF", 1500);
    }
    // (1) Ctrl0 + Ctrl1: Cycle EF’s ARG method if in ARG mode
    else if ((pressedButtons & (maskCtrl0 | maskCtrl1)) == (maskCtrl0 | maskCtrl1)) {
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
    // (4) Ctrl0 + Ctrl4: Set active slot to MIDI Note mode
    else if ((pressedButtons & (maskCtrl0 | maskCtrl4)) == (maskCtrl0 | maskCtrl4)) {
        context.configManager.setSlotType(context.activePot, MIDIMessageType::Note);
        char buf[32];
        sprintf(buf, "Slot %d => NOTE", context.activePot);
        context.displayManager.displayStatus(buf, 1500);
    }
    // (5) Ctrl0 + Ctrl5: Set active slot to Program Change
    else if ((pressedButtons & (maskCtrl0 | maskCtrl5)) == (maskCtrl0 | maskCtrl5)) {
        context.configManager.setSlotType(context.activePot, MIDIMessageType::ProgramChange);
        char buf[32];
        sprintf(buf, "Slot %d => PROG", context.activePot);
        context.displayManager.displayStatus(buf, 1500);
    }
    // (6) Ctrl1 + Ctrl4: Set active slot to Aftertouch
    else if ((pressedButtons & (maskCtrl1 | maskCtrl4)) == (maskCtrl1 | maskCtrl4)) {
        context.configManager.setSlotType(context.activePot, MIDIMessageType::Aftertouch);
        char buf[32];
        sprintf(buf, "Slot %d => AFTER", context.activePot);
        context.displayManager.displayStatus(buf, 1500);
    }
    // (7) Ctrl1 + Ctrl5: Set active slot to Pitch Bend
    else if ((pressedButtons & (maskCtrl1 | maskCtrl5)) == (maskCtrl1 | maskCtrl5)) {
        context.configManager.setSlotType(context.activePot, MIDIMessageType::PitchBend);
        char buf[32];
        sprintf(buf, "Slot %d => BEND", context.activePot);
        context.displayManager.displayStatus(buf, 1500);
    }
    // (8) Ctrl2 + Ctrl4: Set active slot to NRPN
    else if ((pressedButtons & (maskCtrl2 | maskCtrl4)) == (maskCtrl2 | maskCtrl4)) {
        context.configManager.setSlotType(context.activePot, MIDIMessageType::NRPN);
        char buf[32];
        sprintf(buf, "Slot %d => NRPN", context.activePot);
        context.displayManager.displayStatus(buf, 1500);
    }
    // (9) Ctrl0 + Ctrl3: Set active slot to SysEx
    else if ((pressedButtons & (maskCtrl0 | maskCtrl3)) == (maskCtrl0 | maskCtrl3)) {
        context.configManager.setSlotType(context.activePot, MIDIMessageType::SysEx);
        char buf[32];
        sprintf(buf, "Slot %d => SYSEX", context.activePot);
        context.displayManager.displayStatus(buf, 1500);
    }
    // (10) Ctrl2 + Ctrl5: Cycle envelope pairings for ARG
    else if ((pressedButtons & (maskCtrl2 | maskCtrl5)) == (maskCtrl2 | maskCtrl5)) {
        auto it = context.potToEnvelopeMap.find(context.activePot);
        if (it == context.potToEnvelopeMap.end()) {
            context.displayManager.displayStatus("No EF assigned", 1000);
            return;
        }
        int efIndex = it->second;
        static int pairPos = 0;
        pairPos = (pairPos + 1) % NUM_ARG_PAIRS;
        int envA = ARG_PAIRS[pairPos].first;
        int envB = ARG_PAIRS[pairPos].second;
        context.envelopes[efIndex].setEnvelopePair(envA, envB);
        context.configManager.setEnvelopePair(envA, envB);
        auto pinName = [](int pin) {
            switch(pin) {
                case A0: return "A0";
                case A1: return "A1";
                case A2: return "A2";
                case A3: return "A3";
                case A6: return "A6";
                case A7: return "A7";
                default: return "Ax";
            }
        };
        char buf[32];
        sprintf(buf, "EF %d: %s/%s", efIndex, pinName(envA), pinName(envB));
        context.displayManager.displayStatus(buf, 1500);
    }
    // (11) Ctrl3 + Ctrl4: Increment arpeggiator base note
    else if ((pressedButtons & (maskCtrl3 | maskCtrl4)) == (maskCtrl3 | maskCtrl4)) {
        MIDISlot &slot = context.configManager.getSlot(context.activePot);
        slot.arpNote = (slot.arpNote + 1) % 128;
        context.configManager.saveSlot(context.activePot, slot);
        char buf[32];
        sprintf(buf, "ARP NOTE %d", slot.arpNote);
        context.displayManager.displayStatus(buf, 1000);
    }
    // (12) Ctrl3 + Ctrl5: Toggle arpeggiator for active slot
    else if ((pressedButtons & (maskCtrl3 | maskCtrl5)) == (maskCtrl3 | maskCtrl5)) {
        if (arpeggiator.isActive() && arpeggiator.getSlot() == context.activePot) {
            arpeggiator.stop();
            context.displayManager.displayStatus("ARP OFF", 1000);
        } else {
            arpeggiator.start(context.activePot);
            context.displayManager.displayStatus("ARP ON", 1000);
        }
    }
    // (13) Ctrl0 + Ctrl2: Cycle configuration profiles
    else if ((pressedButtons & (maskCtrl0 | maskCtrl2)) == (maskCtrl0 | maskCtrl2)) {
        currentProfile = (currentProfile + 1) % 3;
        context.configManager.loadProfile(currentProfile);
        context.potChannels.clear();
        for (uint8_t i = 0; i < context.configManager.getNumPots(); ++i) {
            context.potChannels.push_back(context.configManager.getPotChannel(i));
        }
        char buf[32];
        sprintf(buf, "PROFILE %d", currentProfile);
        context.displayManager.displayStatus(buf, 1500);
    }
    // (14) Ctrl1 + Ctrl2: Toggle MIDI clock output
    else if ((pressedButtons & (maskCtrl1 | maskCtrl2)) == (maskCtrl1 | maskCtrl2)) {
        g_clockOutEnabled = !g_clockOutEnabled;
        context.displayManager.displayStatus(g_clockOutEnabled ? "CLK OUT ON" : "CLK OUT OFF", 1000);
    }
}

/**
 * Read a single button using the row-driven scanning method.
 * Caches the most recently scanned row so repeated calls within the
 * same row do not trigger additional ADC reads.
 */
uint8_t ButtonManager::readMuxButton(uint8_t buttonIndex) {
    static uint8_t lastRow = 0xFF;
    static uint8_t rowValues[BUTTON_COLS] = {0};

    uint8_t row = buttonIndex / BUTTON_COLS;
    uint8_t col = buttonIndex % BUTTON_COLS;

    if (row != lastRow) {
        digitalWrite(_cfg.rowDriverPin, HIGH);
        setMux(_cfg.muxrPins, row);
        delayMicroseconds(5);
        for (uint8_t c = 0; c < BUTTON_COLS; ++c) {
            setMux(_cfg.muxcPins, c);
            delayMicroseconds(5);
            int v = analogRead(_cfg.buttonMuxAnalogPin);
            rowValues[c] = (v < 512) ? HIGH : LOW;
        }
        digitalWrite(_cfg.rowDriverPin, LOW);
        lastRow = row;
    }

    return rowValues[col];
}

/**
 * Read a direct-wired control button. These inputs are active LOW and bypass
 * the multiplexer used for slot buttons.
 */
bool ButtonManager::readControlButton(uint8_t buttonIndex) {
    return (digitalRead(_controlPins[buttonIndex]) == LOW);
}

void ButtonManager::scanControlInputs(ButtonManagerContext& context) {
    unsigned long now = ::now();
    for (uint8_t ch = 6; ch < 12; ++ch) {
        selectMux(0, ch);
        delayMicroseconds(5);
        int val = analogRead(_cfg.buttonMuxAnalogPin);
        bool pressed = (val < 512);
        uint8_t idx = ch - 6;
        bool stable = Utility::debounce(buttonStates[NUM_VIRTUAL_BUTTONS + idx], pressed,
                                        lastDebounceTimes[NUM_VIRTUAL_BUTTONS + idx], now,
                                        DEBOUNCE_DELAY);
        if (stable) {
            updateCtrlButton(idx, buttonStates[NUM_VIRTUAL_BUTTONS + idx], context);
        }
    }

    // After updating each control button, check for multi-button combos
    uint8_t mask = 0;
    for (uint8_t i = 0; i < NUM_CONTROL_BUTTONS; ++i) {
        if (buttonStates[NUM_VIRTUAL_BUTTONS + i]) {
            mask |= (1 << i);
        }
    }
    static uint8_t lastMask = 0;
    if (mask != lastMask) {
        if (mask && (mask & (mask - 1))) { // more than one button pressed
            handleMultiButtonPress(mask, context);
        }
        lastMask = mask;
    }

    for (uint8_t i = 0; i < 3; ++i) {
        uint8_t ch = 12 + i;
        selectMux(0, ch);
        delayMicroseconds(5);
        int val = analogRead(_cfg.buttonMuxAnalogPin);
        _ctrlPotValues[i] = Utility::exponentialMovingAverage(val, _ctrlPotValues[i], 0.1f);
    }
}

void ButtonManager::updateCtrlButton(uint8_t index, bool pressed, ButtonManagerContext& context) {
    updateButtonStateMachine(NUM_VIRTUAL_BUTTONS + index, pressed, context);
}

void ButtonManager::selectMux(uint8_t row, uint8_t col) {
    setMux(_cfg.muxrPins, row);
    setMux(_cfg.muxcPins, col);
}

bool ButtonManager::isMuxButtonPressed(uint8_t index) {
    return readMuxButton(index) == LOW;  // assuming LOW means pressed
}
