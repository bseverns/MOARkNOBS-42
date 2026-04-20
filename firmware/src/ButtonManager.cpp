// ButtonManager is where human hands meet firmware logic. Think of this file as
// the field manual for diode-matrix scanning, timing thresholds, and how we map
// physical gestures onto virtual slots. Comments call out why the mux waits,
// how we debounce without hiding latency, and why we stream context back to the
// WebSerial educator in the loop. Read it like a lab notebook.

#include "ButtonManager.h"
#include "EnvelopeFollower.h"
#include "Globals.h"
#include "ConfigManager.h"
#include "MIDITypes.h"
#include "Hardware/IO.h"
#include "Utility.h"
#include "TimeUtils.h"
#include "Arpeggiator.h"
#include "WebSerial.h"
#include <map>
#include <cmath>

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

static const unsigned long LONG_PRESS_DELAY = 500;
static const unsigned long DOUBLE_PRESS_DELAY = 300;

// Mirror the global ARG pair count so our math stays synced without recomputing.
static const int NUM_ARG_PAIRS = ARG_PAIRS_LEN;

static constexpr MIDISlot::EfSettings::FilterType SLOT_FILTERS[] = {
    MIDISlot::EfSettings::FilterType::Linear,      MIDISlot::EfSettings::FilterType::OppositeLinear,
    MIDISlot::EfSettings::FilterType::Exponential, MIDISlot::EfSettings::FilterType::Random,
    MIDISlot::EfSettings::FilterType::Lowpass,     MIDISlot::EfSettings::FilterType::Highpass,
    MIDISlot::EfSettings::FilterType::Bandpass};
static constexpr const char *FILTER_TYPE_NAMES[] = {
    "LINEAR", "OPPOSITE_LINEAR", "EXPONENTIAL", "RANDOM", "LOWPASS", "HIGHPASS", "BANDPASS"};

static constexpr int NUM_FILTER_TYPES = sizeof(SLOT_FILTERS) / sizeof(SLOT_FILTERS[0]);
static constexpr ARGMethod ALL_ARG_METHODS[] = {
    ARGMethod::PLUS, ARGMethod::MIN,  ARGMethod::PECK, ARGMethod::SHAV, ARGMethod::SQAR,
    ARGMethod::BABS, ARGMethod::TABS, ARGMethod::MULT, ARGMethod::DIVI, ARGMethod::AVG,
    ARGMethod::XABS, ARGMethod::MAXX, ARGMethod::MINN, ARGMethod::XORR};

static constexpr const char *ARG_METHOD_NAMES[] = {"PLUS", "MIN",  "PECK", "SHAV", "SQAR",
                                                   "BABS", "TABS", "MULT", "DIVI", "AVG",
                                                   "XABS", "MAXX", "MINN", "XORR"};

// Active profile index is stored globally so boot can restore it.

namespace {
constexpr uint8_t maskCtrl0 = 1 << 0;
constexpr uint8_t maskCtrl1 = 1 << 1;
constexpr uint8_t maskCtrl2 = 1 << 2;
constexpr uint8_t maskCtrl3 = 1 << 3;
constexpr uint8_t maskCtrl4 = 1 << 4;
constexpr uint8_t maskCtrl5 = 1 << 5;
constexpr uint8_t panicMask = maskCtrl0 | maskCtrl1 | maskCtrl2;

constexpr uint32_t MUX_SETTLE_US = 5;

inline void waitForMuxSettle() {
    uint32_t start = micros();
    while (micros() - start < MUX_SETTLE_US) {
        yield();
    }
}

// Fast LUT-based mux writer used in the tight matrix scan loop.
inline void setMuxFast(const uint8_t selPins[4], uint8_t index) {
    static const uint8_t lut[16][4] = {{0, 0, 0, 0}, {1, 0, 0, 0}, {0, 1, 0, 0}, {1, 1, 0, 0},
                                       {0, 0, 1, 0}, {1, 0, 1, 0}, {0, 1, 1, 0}, {1, 1, 1, 0},
                                       {0, 0, 0, 1}, {1, 0, 0, 1}, {0, 1, 0, 1}, {1, 1, 0, 1},
                                       {0, 0, 1, 1}, {1, 0, 1, 1}, {0, 1, 1, 1}, {1, 1, 1, 1}};
    const uint8_t *bits = lut[index & 0x0F];
    digitalWriteFast(selPins[0], bits[0]);
    digitalWriteFast(selPins[1], bits[1]);
    digitalWriteFast(selPins[2], bits[2]);
    digitalWriteFast(selPins[3], bits[3]);
}

// Emit one slot patch so the browser can follow on-device edits.
inline void streamSlotPatch(ConfigManager &config, uint8_t slotIndex) {
    WebSerial::sendSlotPatch(config, slotIndex);
}

// Emit one slot-to-envelope assignment patch for the browser/editor layer.
inline void streamEnvelopeAssignment(uint8_t slotIndex, int envelopeIndex) {
    WebSerial::sendEnvelopeAssignment(slotIndex, envelopeIndex);
}

// Emit updated filter tuning after a control-button gesture changes it.
inline void streamFilterPatch(const EnvelopeFollower &env) {
    WebSerial::sendFilterPatch(env.getFilterType(), env.getShapingFrequency(), env.getShapingQ());
}

// Emit current ARG settings after local edits so the browser stays honest.
inline void streamArgPatch(const ConfigManager &config) {
    WebSerial::sendArgPatch(config.getARGMethod(), config.getARGEnable() != 0,
                            config.getEnvelopeA(), config.getEnvelopeB());
}

// Accept either raw analog pin ids or normalized follower indices from older call sites.
inline uint8_t normalizeEnvelopeIndex(uint8_t raw) {
    int idx = envelopeIndexFromAnalogPin(raw);
    if (idx >= 0) {
        return static_cast<uint8_t>(idx);
    }
    if (raw < NUM_ENVELOPES) {
        return raw;
    }
    return static_cast<uint8_t>(raw % NUM_ENVELOPES);
}

// Map one EF filter enum back to its position in the cycle list.
inline int filterIndex(MIDISlot::EfSettings::FilterType type) {
    for (int i = 0; i < NUM_FILTER_TYPES; ++i) {
        if (SLOT_FILTERS[i] == type) {
            return i;
        }
    }
    return 0;
}

// Step forward/backward through the supported EF filter list.
inline MIDISlot::EfSettings::FilterType cycleFilter(MIDISlot::EfSettings::FilterType current,
                                                    int delta) {
    int index = filterIndex(current);
    index = (index + delta + NUM_FILTER_TYPES) % NUM_FILTER_TYPES;
    return SLOT_FILTERS[index];
}

// Persist new EF settings, update the slot cache, and reconfigure the live follower if present.
inline void commitEfSettings(ButtonManagerContext &context, int slotIndex,
                             const MIDISlot::EfSettings &settings) {
    MIDISlot &slot = context.configManager.getSlot(static_cast<uint8_t>(slotIndex));
    slot.efSettings = settings;
    slot.setEnvelopeFollowerIndex(settings.followerIndex);
    context.configManager.saveSlot(static_cast<uint8_t>(slotIndex), slot);
    context.potToEnvelopeMap[slotIndex] = slot.efSettings;
    int follower = settings.followerIndex;
    if (follower >= 0 && follower < static_cast<int>(context.envelopes.size())) {
        context.envelopes[follower].configureFromEfSettings(slot.efSettings);
    }
}
} // namespace

// Constructor
ButtonManager::ButtonManager(const HardwareConfig &config, const uint8_t *controlPins,
                             PotentiometerManager *potentiometerManager)
    : _cfg(config), _controlPins(controlPins), _potentiometerManager(potentiometerManager),
      activeMode(0), _pendingEfSlot(-1), _efAssignDeadline(0) {
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
void ButtonManager::processButtons(ButtonManagerContext &context) {
    unsigned long now = ::now();

    // Drop pending EF assignment if the window expired
    if (_pendingEfSlot >= 0 && now > _efAssignDeadline) {
        _pendingEfSlot = -1;
        context.displayManager.displayStatus("EF assign timeout", 1000);
    }

#ifdef BUTTON_MANAGER_PROFILE
    uint32_t tStart = micros();
#endif

    // Scan mux matrix row by row
    uint8_t rawStates[NUM_VIRTUAL_BUTTONS];
    for (uint8_t r = 0; r < BUTTON_ROWS; ++r) {
        digitalWrite(_cfg.rowDriverPin, HIGH);
        setMuxFast(_cfg.muxrPins, r);
        waitForMuxSettle();
        for (uint8_t c = 0; c < BUTTON_COLS; ++c) {
            setMuxFast(_cfg.muxcPins, c);
            waitForMuxSettle();
            int v = hardware::readAnalog(_cfg.buttonMuxAnalogPin);
            rawStates[r * BUTTON_COLS + c] = (v < BUTTON_PRESS_THRESHOLD) ? HIGH : LOW;
        }
        digitalWrite(_cfg.rowDriverPin, LOW);
    }

    // Process virtual (multiplexer) buttons using scanned states
    for (uint8_t i = 0; i < NUM_VIRTUAL_BUTTONS; i++) {
        uint8_t rawState = rawStates[i];
        bool stableReading =
            Utility::debounce(buttonStates[i], rawState, lastDebounceTimes[i], now, DEBOUNCE_DELAY);

        if (stableReading) {
            bool pressed = (buttonStates[i] == HIGH);
            updateButtonStateMachine(i, pressed, context);
        }
    }

    // Control buttons & pots via spare mux channels
    scanControlInputs(context);

#ifdef BUTTON_MANAGER_PROFILE
    uint32_t tElapsed = micros() - tStart;
    static uint32_t maxScan = 0;
    static uint64_t totalScan = 0;
    static uint32_t scanCount = 0;
    if (tElapsed > maxScan) {
        maxScan = tElapsed;
    }
    totalScan += tElapsed;
    if (++scanCount % 1000 == 0) {
        Serial.printf("btn scan avg %luus max %luus\n", (unsigned long)(totalScan / scanCount),
                      (unsigned long)maxScan);
    }
    // Scan budget reconciliation: flag if we exceed the scheduler slice budget.
    // Button scan should complete in <2ms to leave room for MIDI/LED work.
    if (tElapsed > 2000U) {
        ++g_systemDiagnostics.loopOverrunCount;
    }
#endif
}

/**
 * The new state machine approach for short vs. long press.
 */
void ButtonManager::updateButtonStateMachine(uint8_t index, bool pressed,
                                             ButtonManagerContext &context) {
    ButtonStateMachine &sm = _buttonMachines[index];
    unsigned long now = ::now();

    // Kill any pending confirm if time ran out
    if (_confirmIndex >= 0 && now > _confirmDeadline) {
        cancelPendingConfirm(context);
    }

    // Smack pending confirm if some other button gets poked
    if (_confirmIndex >= 0 && index != _confirmIndex && pressed) {
        cancelPendingConfirm(context);
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
void ButtonManager::onLongPress(uint8_t index, ButtonManagerContext &context) {
    _confirmIndex = index;
    _confirmDeadline = ::now() + CONFIRM_WINDOW_MS;
    context.displayManager.displayStatus("CONFIRM\nTap again", 1000);
    startWarningForIndex(index, context);
}

/**
 * Fire the actual long‑press payload after confirmation.
 */
void ButtonManager::performLongPressAction(uint8_t index, ButtonManagerContext &context) {
    // Slot buttons (0-41)
    if (index < NUM_VIRTUAL_BUTTONS) {
        MIDISlot &slotRef = context.configManager.getSlot(index);
        MIDISlot::EfSettings settings = slotRef.efSettings;
        settings.followerIndex = slotRef.getEnvelopeFollowerIndex();
        auto it = context.potToEnvelopeMap.find(index);
        if (it != context.potToEnvelopeMap.end()) {
            settings = it->second;
        }
        if (settings.followerIndex < 0) {
            settings.followerIndex = 0;
        } else {
            settings.followerIndex =
                static_cast<int8_t>((settings.followerIndex + 1) % context.envelopes.size());
        }
        commitEfSettings(context, index, settings);
        int assigned = settings.followerIndex;
        if (assigned >= 0 && assigned < static_cast<int>(context.envelopes.size())) {
            context.envelopes[assigned].toggleActive(true);
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "Slot %d -> EF %d", index, assigned);
        context.displayManager.displayStatus(buf, 1500);

        // Allow an explicit EF pick via control buttons 0-5
        _pendingEfSlot = index;
        _efAssignDeadline = ::now() + EF_ASSIGN_WINDOW_MS;
    } else {
        // Control buttons (0-5)
        uint8_t ctrlIdx = index - NUM_VIRTUAL_BUTTONS;
        switch (ctrlIdx) {
        case 0: { // Calibrate assigned envelope follower
            auto it = context.potToEnvelopeMap.find(context.activePot);
            if (it == context.potToEnvelopeMap.end()) {
                context.displayManager.displayStatus("No EF assigned", 1000);
                break;
            }
            int efIndex = it->second.followerIndex;
            if (efIndex >= 0 && efIndex < static_cast<int>(context.envelopes.size())) {
                context.envelopes[efIndex].calibrate(); // auto-saves baseline via ConfigManager
            }
            context.displayManager.displayStatus("EF Calibrated", 1500);
            break;
        }
        case 1: {
            if (context.diagnosticMode) {
                context.diagnosticPage = static_cast<uint8_t>((context.diagnosticPage + 1) %
                                                              DisplayManager::kDiagnosticPageCount);
                context.displayManager.displayStatus("Diag Page", 1000);
            } else {
                // Reload configuration profile from EEPROM
                context.configManager.loadProfile(g_activeProfile);
                context.potChannels.clear();
                for (uint8_t i = 0; i < context.configManager.getNumPots(); ++i) {
                    context.potChannels.push_back(context.configManager.getPotChannel(i));
                }
                g_profileChangeRequested = true;
                context.displayManager.displayStatus("Profile Reset!", 1500);
            }
            break;
        }
        case 2: { // Toggle Slot Active
            MIDISlot &slot = context.configManager.getSlot(context.activePot);
            slot.active = !slot.active;
            context.configManager.saveSlot(context.activePot, slot);
            char buf[32];
            snprintf(buf, sizeof(buf), "Slot %d %s", context.activePot, slot.active ? "ON" : "OFF");
            context.displayManager.displayStatus(buf, 1500);
            break;
        }
        case 3: // EEPROM reload from current profile
            context.configManager.loadConfiguration(context.potChannels);
            context.displayManager.displayStatus("Config Reloaded", 1500);
            break;
        case 4: { // Save configuration to the active profile
            context.configManager.saveProfile(g_activeProfile);
            context.configManager.saveEnvelopeSettings(context.potToEnvelopeMap, context.envelopes);
            g_profileSaveRequested = true;
            context.displayManager.displayStatus("Config Saved", 1500);
            break;
        }
        case 5: { // Diagnostic entry + page cycling (encoder long-press)
            if (!context.diagnosticMode) {
                context.diagnosticMode = true;
                context.diagnosticPage = DisplayManager::kDiagnosticPageDebug;
                context.displayManager.displayStatus("Diagnostics", 1000);
                context.ledManager.setDiagnosticMode(true);
            } else {
                context.diagnosticPage = static_cast<uint8_t>((context.diagnosticPage + 1) %
                                                              DisplayManager::kDiagnosticPageCount);
                context.displayManager.displayStatus("Diag Page", 1000);
            }
            break;
        }
        default:
            context.displayManager.displayStatus("No Long Action", 1000);
            break;
        }
    }
}

/**
 * Called after the user releases (short or long). If it wasn't a long press, we treat it as short
 * press.
 */
void ButtonManager::onRelease(uint8_t index, ButtonManagerContext &context) {
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
void ButtonManager::handleShortPress(uint8_t index, ButtonManagerContext &context) {
    auto &sm = _buttonMachines[index];
    unsigned long now = ::now();

    if (_confirmIndex == index) {
        // Second tap confirms the long‑press move
        bool withinWindow = (now <= _confirmDeadline);
        cancelPendingConfirm(context);
        if (withinWindow) {
            performLongPressAction(index, context);
        }
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
void ButtonManager::handleDoublePress(uint8_t index, ButtonManagerContext &context) {
    auto cycleFilterForSlot = [&](uint8_t slotIndex, int delta) -> bool {
        auto it = context.potToEnvelopeMap.find(slotIndex);
        if (it == context.potToEnvelopeMap.end()) {
            context.displayManager.displayStatus("No EF assigned", 1000);
            return false;
        }

        MIDISlot::EfSettings settings = it->second;
        if (settings.followerIndex < 0 ||
            settings.followerIndex >= static_cast<int>(context.envelopes.size())) {
            context.displayManager.displayStatus("No EF assigned", 1000);
            return false;
        }

        settings.filterType = cycleFilter(settings.filterType, delta);
        commitEfSettings(context, slotIndex, settings);

        SlotEnvelopePayload payload = context.configManager.getSlotEnvelopePayload(slotIndex);
        payload.filterType =
            static_cast<uint8_t>(EnvelopeFollower::filterFromEfType(settings.filterType));
        context.configManager.setSlotEnvelopePayload(slotIndex, payload);

        streamSlotPatch(context.configManager, slotIndex);
        streamFilterPatch(context.envelopes[settings.followerIndex]);

        const char *filterName = FILTER_TYPE_NAMES[filterIndex(settings.filterType)];
        char msg[32];
        snprintf(msg, sizeof(msg), "Slot %d => %s", slotIndex, filterName);
        context.displayManager.displayStatus(msg, 1500);
        return true;
    };

    if (index < NUM_VIRTUAL_BUTTONS) {
        cycleFilterForSlot(index, +1);
        return;
    }

    uint8_t cIndex = static_cast<uint8_t>(index - NUM_VIRTUAL_BUTTONS);
    switch (cIndex) {
    case 0:
        cycleFilterForSlot(context.activePot, +1);
        break;
    case 1:
        cycleFilterForSlot(context.activePot, -1);
        break;
    case 2: {
        // Double Press (Ctrl #2): Cycle MIDI message type
        MIDISlot &slot = context.configManager.getSlot(context.activePot);
        slot.type = static_cast<MIDIMessageType>((static_cast<int>(slot.type) + 1) %
                                                 (static_cast<int>(MIDIMessageType::SysEx) + 1));
        context.configManager.saveSlot(context.activePot, slot);
        char buf[32];
        snprintf(buf, sizeof(buf), "Slot %d Type %d", context.activePot,
                 static_cast<int>(slot.type));
        context.displayManager.displayStatus(buf, 1500);
        streamSlotPatch(context.configManager, context.activePot);
        break;
    }
    case 4:
    case 5:
        context.displayManager.displayStatus("No Double Action", 1000);
        break;
    default:
        context.displayManager.displayStatus("Unknown double press", 1000);
        break;
    }
}

/**
 * Dispatch a confirmed short press. The helper `handleSingleButtonPress` keeps
 * the actual per-button actions so other parts of the firmware can reuse them.
 */
void ButtonManager::doSinglePressAction(uint8_t index, ButtonManagerContext &context) {
    BM_DBG_PRINTLN("Single Press on button " + String(index));
    handleSingleButtonPress(index, context);
}

void ButtonManager::handleSingleButtonPress(uint8_t buttonIndex, ButtonManagerContext &context) {
    // Pending EF selection overrides normal control button behavior
    if (_pendingEfSlot >= 0 && buttonIndex >= NUM_VIRTUAL_BUTTONS) {
        uint8_t controlIndex = buttonIndex - NUM_VIRTUAL_BUTTONS;
        if (controlIndex < context.envelopes.size()) {
            MIDISlot &slotRef = context.configManager.getSlot(_pendingEfSlot);
            MIDISlot::EfSettings settings = slotRef.efSettings;
            settings.followerIndex = slotRef.getEnvelopeFollowerIndex();
            settings.followerIndex = static_cast<int8_t>(controlIndex);
            commitEfSettings(context, _pendingEfSlot, settings);
            context.envelopes[controlIndex].toggleActive(true);
            char buf[32];
            snprintf(buf, sizeof(buf), "Slot %d -> EF %d", _pendingEfSlot, controlIndex);
            context.displayManager.displayStatus(buf, 1500);
            streamEnvelopeAssignment(_pendingEfSlot, controlIndex);
        }
        _pendingEfSlot = -1;
        return;
    }

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
        context.displayManager.displayStatus(context.envelopeFollowMode ? "EF: ON" : "EF: OFF",
                                             1500);
        break;

    case 1: {
        // Short Press (Control Button #1): Select next slot
        context.activePot = (context.activePot + 1) % NUM_POTS;
        context.displayManager.displayStatus(("Next Slot=" + String(context.activePot)).c_str(),
                                             1500);
    } break;

    case 2: {
        // Short Press (Control Button #2): Cycle Envelope to follow [if EF on]
        if (!context.envelopeFollowMode) {
            context.displayManager.displayStatus("EF is OFF", 1000);
            break;
        }

        // If EF is on, cycle to the next EF for the active slot
        MIDISlot &slotRef = context.configManager.getSlot(context.activePot);
        MIDISlot::EfSettings settings = slotRef.efSettings;
        settings.followerIndex = slotRef.getEnvelopeFollowerIndex();
        auto it = context.potToEnvelopeMap.find(context.activePot);
        if (it != context.potToEnvelopeMap.end()) {
            settings = it->second;
        }
        if (settings.followerIndex < 0) {
            settings.followerIndex = 0;
        } else {
            settings.followerIndex =
                static_cast<int8_t>((settings.followerIndex + 1) % context.envelopes.size());
        }
        commitEfSettings(context, context.activePot, settings);
        int assigned = settings.followerIndex;
        if (assigned >= 0 && assigned < static_cast<int>(context.envelopes.size())) {
            context.envelopes[assigned].toggleActive(true);
        }

        char buf[32];
        snprintf(buf, sizeof(buf), "Slot %d -> EF %d", context.activePot, assigned);
        context.displayManager.displayStatus(buf, 1500);
        streamEnvelopeAssignment(context.activePot, assigned);
    } break;

    case 3: {
        // Short Press (Control Button #3): Cycle the active slot’s MIDI channel 1..16
        uint8_t oldChan = context.configManager.getPotChannel(context.activePot);
        uint8_t newChan = (oldChan % 16) + 1; // cycles 1..16
        context.configManager.setPotChannel(context.activePot, newChan);

        if (_potentiometerManager != nullptr) {
            _potentiometerManager->setChannel(context.activePot, newChan);
        }
        if (context.activePot < context.potChannels.size()) {
            context.potChannels[context.activePot] = newChan;
        }

        char buf[32];
        snprintf(buf, sizeof(buf), "Slot %d => Ch %d", context.activePot, newChan);
        context.displayManager.displayStatus(buf, 1500);
        streamSlotPatch(context.configManager, context.activePot);
    } break;

    case 4: {
        // Short Press (Control Button #4): Cycle the active slot’s registry number
        MIDIMessageType type = context.configManager.getSlotType(context.activePot);
        if (type == MIDIMessageType::NRPN || type == MIDIMessageType::RPN) {
            uint8_t param = context.configManager.getSlotData1(context.activePot);
            param = (param + 1) % 128;
            context.configManager.setSlotData1(context.activePot, param);
            char buf[32];
            snprintf(buf, sizeof(buf), "Slot %d => %s %d", context.activePot,
                     type == MIDIMessageType::NRPN ? "NRPN" : "RPN", param);
            context.displayManager.displayStatus(buf, 1500);
            streamSlotPatch(context.configManager, context.activePot);
        } else {
            uint8_t oldCC = context.configManager.getPotCCNumber(context.activePot);
            uint8_t newCC = (oldCC + 1) % 128; // 0..127
            context.configManager.setPotCCNumber(context.activePot, newCC);
            if (_potentiometerManager != nullptr) {
                _potentiometerManager->setCCNumber(context.activePot, newCC);
            }
            char buf[32];
            snprintf(buf, sizeof(buf), "Slot %d => CC %d", context.activePot, newCC);
            context.displayManager.displayStatus(buf, 1500);
            streamSlotPatch(context.configManager, context.activePot);
        }
        WebSerial::sendSlotPatch(context.configManager, context.activePot);
    } break;

    case 5: {
        // Short Press (Control Button #5): Tapped BPM
        if (context.diagnosticMode) {
            // Diagnostics is modal; a quick tap exits so the panel returns to tempo duty.
            context.diagnosticMode = false;
            context.displayManager.displayStatus("Diag Off", 1000);
            context.ledManager.setDiagnosticMode(false);
            break;
        }
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
    } break;

    default:
        context.displayManager.displayStatus("UNKNOWN CTRL BTN", 1000);
        break;
    }
}

void ButtonManager::handleMultiButtonPress(uint8_t pressedButtons, ButtonManagerContext &context) {
    const uint8_t jitterMask = maskCtrl0 | maskCtrl3 | maskCtrl4;

    auto ensureActiveSlot = [&]() -> MIDISlot * {
        if (context.activePot >= NUM_SLOTS) {
            context.displayManager.displayStatus("Slot out of range", 1000);
            return nullptr;
        }
        return &context.configManager.getSlot(context.activePot);
    };

    // (0) Ctrl0 + Ctrl3 + Ctrl4: Jitter tuning mode (handled in scanControlInputs)
    if (pressedButtons == jitterMask) {
        context.displayManager.displayStatus("Jitter Mode", 1000);
    }
    // (0.5) Ctrl0 + Ctrl1 + Ctrl2: panic-safe reset to active profile baseline.
    else if (pressedButtons == panicMask) {
        arpeggiator.stop();
        g_arpEditActive = false;
        context.envelopeFollowMode = false;
        context.configManager.loadProfile(g_activeProfile);
        context.potChannels.clear();
        for (uint8_t i = 0; i < context.configManager.getNumPots(); ++i) {
            context.potChannels.push_back(context.configManager.getPotChannel(i));
        }
        g_profileChangeRequested = true;
        context.displayManager.displayStatus("Panic: Baseline", 1500);
    }
    // (1) Ctrl3 + Ctrl4 + Ctrl5: toggle USB MIDI output
    else if ((pressedButtons & (maskCtrl3 | maskCtrl4 | maskCtrl5)) ==
             (maskCtrl3 | maskCtrl4 | maskCtrl5)) {
        g_usbMidiOutEnabled = !g_usbMidiOutEnabled;
        context.displayManager.displayStatus(g_usbMidiOutEnabled ? "USB MIDI ON" : "USB MIDI OFF",
                                             1500);
    }
    // (2) Ctrl0 + Ctrl1: Cycle EF’s ARG method if in ARG mode
    else if ((pressedButtons & (maskCtrl0 | maskCtrl1)) == (maskCtrl0 | maskCtrl1)) {
        MIDISlot *slot = ensureActiveSlot();
        if (slot == nullptr) {
            return;
        }
        constexpr size_t methodCount = sizeof(ALL_ARG_METHODS) / sizeof(ALL_ARG_METHODS[0]);
        size_t methodIndex = 0;
        for (; methodIndex < methodCount; ++methodIndex) {
            if (slot->arg.method == ALL_ARG_METHODS[methodIndex]) {
                break;
            }
        }
        methodIndex = (methodIndex + 1) % methodCount;

        slot->arg.enabled = 1;
        slot->arg.method = ALL_ARG_METHODS[methodIndex];
        context.configManager.saveSlot(context.activePot, *slot);

        context.configManager.setARGEnable(slot->arg.enabled);
        context.configManager.setARGMethod(static_cast<uint8_t>(slot->arg.method));

        context.configManager.setEnvelopePair(slot->arg.sourceA, slot->arg.sourceB);

        char msg[32];
        snprintf(msg, sizeof(msg), "Slot %d ARG=%s", context.activePot,
                 ARG_METHOD_NAMES[methodIndex]);
        context.displayManager.displayStatus(msg, 1500);
        streamSlotPatch(context.configManager, context.activePot);
        streamArgPatch(context.configManager);
    }
    // (3) Ctrl0 + Ctrl2: Cycle ARG envelope pair
    else if ((pressedButtons & (maskCtrl0 | maskCtrl2)) == (maskCtrl0 | maskCtrl2)) {
        MIDISlot *slot = ensureActiveSlot();
        if (slot == nullptr) {
            return;
        }
        static size_t pairIndex = 0;
        pairIndex = (pairIndex + 1) % NUM_ARG_PAIRS;
        const auto &pair = ARG_PAIRS[pairIndex];
        const uint8_t sourceA = normalizeEnvelopeIndex(pair.first);
        const uint8_t sourceB = normalizeEnvelopeIndex(pair.second);

        slot->arg.enabled = 1;
        slot->arg.sourceA = sourceA;
        slot->arg.sourceB = sourceB;
        context.configManager.saveSlot(context.activePot, *slot);

        context.configManager.setARGEnable(slot->arg.enabled);
        context.configManager.setARGMethod(static_cast<uint8_t>(slot->arg.method));
        context.configManager.setEnvelopePair(sourceA, sourceB);

        char buf[32];
        snprintf(buf, sizeof(buf), "Slot %d: EF%u+EF%u", context.activePot, sourceA + 1,
                 sourceB + 1);
        context.displayManager.displayStatus(buf, 1500);
        streamSlotPatch(context.configManager, context.activePot);
        streamArgPatch(context.configManager);
    }
    // (4) Ctrl3 + Ctrl4: Cycle light modes
    else if ((pressedButtons & (maskCtrl3 | maskCtrl4)) == (maskCtrl3 | maskCtrl4)) {
        static uint8_t currentLightMode = 0;
        currentLightMode = (currentLightMode + 1) % 4;
        context.ledManager.setModeDisplay(currentLightMode);
        char buf[32];
        snprintf(buf, sizeof(buf), "LightMode=%d", currentLightMode);
        context.displayManager.displayStatus(buf, 1500);
    }
    // (5) Ctrl0 + Ctrl4: Enable EF and randomize settings
    else if ((pressedButtons & (maskCtrl0 | maskCtrl4)) == (maskCtrl0 | maskCtrl4)) {
        if (!context.envelopeFollowMode) {
            context.envelopeFollowMode = true;
            context.displayManager.displayStatus("EF turned ON", 1000);
        }
        int randomEF = random(context.envelopes.size());
        MIDISlot &slotRef = context.configManager.getSlot(context.activePot);
        MIDISlot::EfSettings settings = slotRef.efSettings;
        settings.followerIndex = slotRef.getEnvelopeFollowerIndex();
        settings.followerIndex = static_cast<int8_t>(randomEF);
        commitEfSettings(context, context.activePot, settings);
        context.envelopes[randomEF].toggleActive(true);
        char buf[32];
        snprintf(buf, sizeof(buf), "Slot %d->RandomEF %d", context.activePot, randomEF);
        context.displayManager.displayStatus(buf, 1500);
        streamEnvelopeAssignment(context.activePot, randomEF);
    }
    // (6) Ctrl4 + Ctrl5: Set active slot to MIDI Note mode
    else if ((pressedButtons & (maskCtrl4 | maskCtrl5)) == (maskCtrl4 | maskCtrl5)) {
        context.configManager.setSlotType(context.activePot, MIDIMessageType::Note);
        char buf[32];
        snprintf(buf, sizeof(buf), "Slot %d => NOTE", context.activePot);
        context.displayManager.displayStatus(buf, 1500);
        streamSlotPatch(context.configManager, context.activePot);
    }
    // (7) Ctrl3 + Ctrl5: Set active slot to Program Change
    else if ((pressedButtons & (maskCtrl3 | maskCtrl5)) == (maskCtrl3 | maskCtrl5)) {
        context.configManager.setSlotType(context.activePot, MIDIMessageType::ProgramChange);
        char buf[32];
        snprintf(buf, sizeof(buf), "Slot %d => PROG", context.activePot);
        context.displayManager.displayStatus(buf, 1500);
        streamSlotPatch(context.configManager, context.activePot);
    }
    // (8) Ctrl0 + Ctrl5: Set active slot to Pitch Bend
    else if ((pressedButtons & (maskCtrl0 | maskCtrl5)) == (maskCtrl0 | maskCtrl5)) {
        context.configManager.setSlotType(context.activePot, MIDIMessageType::PitchBend);
        char buf[32];
        snprintf(buf, sizeof(buf), "Slot %d => BEND", context.activePot);
        context.displayManager.displayStatus(buf, 1500);
        streamSlotPatch(context.configManager, context.activePot);
    }
    // (9) Ctrl1 + Ctrl4: Set active slot to Aftertouch
    else if ((pressedButtons & (maskCtrl1 | maskCtrl4)) == (maskCtrl1 | maskCtrl4)) {
        context.configManager.setSlotType(context.activePot, MIDIMessageType::Aftertouch);
        char buf[32];
        snprintf(buf, sizeof(buf), "Slot %d => AFTER", context.activePot);
        context.displayManager.displayStatus(buf, 1500);
        streamSlotPatch(context.configManager, context.activePot);
    }
    // (10) Ctrl1 + Ctrl5: Toggle MIDI clock output
    else if ((pressedButtons & (maskCtrl1 | maskCtrl5)) == (maskCtrl1 | maskCtrl5)) {
        g_clockOutEnabled = !g_clockOutEnabled;
        context.displayManager.displayStatus(g_clockOutEnabled ? "CLK OUT ON" : "CLK OUT OFF",
                                             1000);
    }
    // (11) Ctrl2 + Ctrl5: Set active slot to NRPN
    else if ((pressedButtons & (maskCtrl2 | maskCtrl5)) == (maskCtrl2 | maskCtrl5)) {
        context.configManager.setSlotType(context.activePot, MIDIMessageType::NRPN);
        char buf[32];
        snprintf(buf, sizeof(buf), "Slot %d => NRPN", context.activePot);
        context.displayManager.displayStatus(buf, 1500);
        streamSlotPatch(context.configManager, context.activePot);
    }
    // (12) Ctrl1 + Ctrl3: Set active slot to RPN
    else if ((pressedButtons & (maskCtrl1 | maskCtrl3)) == (maskCtrl1 | maskCtrl3)) {
        context.configManager.setSlotType(context.activePot, MIDIMessageType::RPN);
        char buf[32];
        snprintf(buf, sizeof(buf), "Slot %d => RPN", context.activePot);
        context.displayManager.displayStatus(buf, 1500);
        streamSlotPatch(context.configManager, context.activePot);
    }
    // (13) Ctrl0 + Ctrl3: Set active slot to SysEx
    else if ((pressedButtons & (maskCtrl0 | maskCtrl3)) == (maskCtrl0 | maskCtrl3)) {
        context.configManager.setSlotType(context.activePot, MIDIMessageType::SysEx);
        char buf[32];
        snprintf(buf, sizeof(buf), "Slot %d => SYSEX", context.activePot);
        context.displayManager.displayStatus(buf, 1500);
        streamSlotPatch(context.configManager, context.activePot);
    }
    // (14) Ctrl2 + Ctrl4: Toggle arpeggiator for active slot
    else if ((pressedButtons & (maskCtrl2 | maskCtrl4)) == (maskCtrl2 | maskCtrl4)) {
        if (arpeggiator.isActive() && arpeggiator.getSlot() == context.activePot) {
            arpeggiator.stop();
            context.displayManager.displayStatus("ARP OFF", 1000);
        } else {
            arpeggiator.start(context.activePot);
            context.displayManager.displayStatus("ARP ON", 1000);
        }
    }
    // (15) Ctrl2 + Ctrl3: Increment arpeggiator base note
    else if ((pressedButtons & (maskCtrl2 | maskCtrl3)) == (maskCtrl2 | maskCtrl3)) {
        MIDISlot &slot = context.configManager.getSlot(context.activePot);
        slot.arpNote = (slot.arpNote + 1) % 128;
        context.configManager.saveSlot(context.activePot, slot);
        char buf[32];
        snprintf(buf, sizeof(buf), "ARP NOTE %d", slot.arpNote);
        context.displayManager.displayStatus(buf, 1000);
        streamSlotPatch(context.configManager, context.activePot);
    }
    // (16) Ctrl1 + Ctrl2: Cycle configuration profiles
    else if ((pressedButtons & (maskCtrl1 | maskCtrl2)) == (maskCtrl1 | maskCtrl2)) {
        g_activeProfile = static_cast<uint8_t>((g_activeProfile + 1) % NUM_PROFILES);
        context.configManager.setActiveProfile(g_activeProfile);
        context.configManager.loadProfile(g_activeProfile);
        context.potChannels.clear();
        for (uint8_t i = 0; i < context.configManager.getNumPots(); ++i) {
            context.potChannels.push_back(context.configManager.getPotChannel(i));
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "PROFILE %c", static_cast<char>('A' + g_activeProfile));
        context.displayManager.displayStatus(buf, 1500);
        g_profileChangeRequested = true;
    }
}

/**
 * Read a single button using the row-driven scanning method.
 * Caches the most recently scanned row so repeated calls within the
 * same row do not trigger additional ADC reads.
 */
uint8_t ButtonManager::readMuxButton(uint8_t buttonIndex) const {
    static uint8_t lastRow = 0xFF;
    static uint8_t rowValues[BUTTON_COLS] = {0};

    uint8_t row = buttonIndex / BUTTON_COLS;
    uint8_t col = buttonIndex % BUTTON_COLS;

    if (row != lastRow) {
        digitalWrite(_cfg.rowDriverPin, HIGH);
        setMuxFast(_cfg.muxrPins, row);
        waitForMuxSettle();
        for (uint8_t c = 0; c < BUTTON_COLS; ++c) {
            setMuxFast(_cfg.muxcPins, c);
            waitForMuxSettle();
            int v = hardware::readAnalog(_cfg.buttonMuxAnalogPin);
            rowValues[c] = (v < BUTTON_PRESS_THRESHOLD) ? HIGH : LOW;
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
    return (hardware::readDigital(_controlPins[buttonIndex]) == LOW);
}

void ButtonManager::scanControlInputs(ButtonManagerContext &context) {
    unsigned long now = ::now();
    for (uint8_t ch = 6; ch < 12; ++ch) {
        selectMux(0, ch);
        waitForMuxSettle();
        int val = hardware::readAnalog(_cfg.buttonMuxAnalogPin);
        bool pressed = (val < BUTTON_PRESS_THRESHOLD);
        uint8_t idx = ch - 6;
        bool stable =
            Utility::debounce(buttonStates[NUM_VIRTUAL_BUTTONS + idx], pressed,
                              lastDebounceTimes[NUM_VIRTUAL_BUTTONS + idx], now, DEBOUNCE_DELAY);
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
    const uint8_t jitterMask = maskCtrl0 | maskCtrl3 | maskCtrl4;
    const uint8_t arpEditMask = maskCtrl2 | maskCtrl4;
    const uint8_t swingMask = maskCtrl2 | maskCtrl3;
    bool jitterActive = (mask == jitterMask);
    g_jitterTuningActive = jitterActive;
    static uint8_t lastMask = 0;
    // Track combos that have long-press behaviors (arp edit, swing presets).
    bool multiPressed = (mask && (mask & (mask - 1)));
    bool longPressCombo = (mask == arpEditMask || mask == swingMask);

    // Combo transitions: handle short-press fallbacks and release behavior.
    if (mask != _comboHoldMask) {
        if (_comboHoldMask == arpEditMask) {
            if (!_comboLongPressFired) {
                handleMultiButtonPress(_comboHoldMask, context);
            }
            // Arp edit only stays active while the combo is held.
            if (g_arpEditActive) {
                g_arpEditActive = false;
                context.displayManager.displayStatus("Arp Edit Off", 1000);
            }
        } else if (_comboHoldMask == swingMask) {
            if (!_comboLongPressFired) {
                handleMultiButtonPress(_comboHoldMask, context);
            }
        }
        _comboHoldMask = longPressCombo ? mask : 0;
        _comboHoldTimestamp = now;
        _comboLongPressFired = false;
    }

    // Fire long-press actions once the combo crosses the hold threshold.
    if (longPressCombo && !_comboLongPressFired &&
        (now - _comboHoldTimestamp >= LONG_PRESS_DELAY)) {
        _comboLongPressFired = true;
        if (mask == arpEditMask) {
            if (arpeggiator.isActive()) {
                g_arpEditActive = true;
                context.displayManager.displayStatus("Arp Edit", 1000);
            } else {
                context.displayManager.displayStatus("Arp Off", 1000);
            }
        } else if (mask == swingMask) {
            // Cycle through swing presets for fast access.
            static uint8_t swingPreset = 0;
            static const uint8_t swingPresets[] = {0, 8, 16, 30};
            swingPreset = static_cast<uint8_t>((swingPreset + 1) %
                                               (sizeof(swingPresets) / sizeof(swingPresets[0])));
            uint8_t value = swingPresets[swingPreset];
            arpeggiator.setSwingPercent(static_cast<float>(value));
            char buf[24];
            snprintf(buf, sizeof(buf), "Swing: %u%%", value);
            context.displayManager.displayStatus(buf, 1000);
        }
    }
    if (multiPressed && !longPressCombo) {
        if (mask != _comboCandidateMask) {
            _comboCandidateMask = mask;
            _comboCandidateSince = now;
        }
        if ((mask != lastMask) && (now - _comboCandidateSince >= COMBO_SETTLE_MS)) {
            handleMultiButtonPress(mask, context);
            lastMask = mask;
        }
    } else {
        _comboCandidateMask = 0;
        _comboCandidateSince = 0;
        if (mask != lastMask) {
            lastMask = mask;
        }
    }

    for (uint8_t i = 0; i < 3; ++i) {
        uint8_t ch = 12 + i;
        selectMux(0, ch);
        waitForMuxSettle();
        int val = hardware::readAnalog(_cfg.buttonMuxAnalogPin);
        _ctrlPotValues[i] = Utility::exponentialMovingAverage(val, _ctrlPotValues[i], 0.1f);
    }

    if (jitterActive) {
        float depth =
            Utility::scale(static_cast<float>(_ctrlPotValues[0]), 0.0f, 1023.0f, 0.0f, 1.0f);
        float smooth =
            Utility::scale(static_cast<float>(_ctrlPotValues[1]), 0.0f, 1023.0f, 0.0f, 1.0f);
        depth = constrain(depth, 0.0f, 1.0f);
        smooth = constrain(smooth, 0.0f, 1.0f);

        bool depthChanged = (_lastJitterDepth < 0.0f) || (fabsf(depth - _lastJitterDepth) >= 0.01f);
        bool smoothChanged =
            (_lastJitterSmoothness < 0.0f) || (fabsf(smooth - _lastJitterSmoothness) >= 0.01f);

        g_jitterSettings.depth = depth;
        g_jitterSettings.smoothness = smooth;

        if (depthChanged) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Jitter: %.2f", depth);
            context.displayManager.displayStatus(buf, SHORT_DISPLAY_TIME);
            _lastJitterDepth = depth;
        } else if (smoothChanged) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Smooth: %.2f", smooth);
            context.displayManager.displayStatus(buf, SHORT_DISPLAY_TIME);
            _lastJitterSmoothness = smooth;
        }
    }
}

void ButtonManager::updateCtrlButton(uint8_t index, bool pressed, ButtonManagerContext &context) {
    updateButtonStateMachine(NUM_VIRTUAL_BUTTONS + index, pressed, context);
}

void ButtonManager::cancelPendingConfirm(ButtonManagerContext &context) {
    if (_confirmIndex >= 0) {
        _confirmIndex = -1;
    }
    if (context.ledManager.isWarningActive()) {
        context.ledManager.clearWarningAnimation();
    }
}

void ButtonManager::startWarningForIndex(uint8_t index, ButtonManagerContext &context) {
    if (index < NUM_VIRTUAL_BUTTONS) {
        if (context.ledManager.isWarningActive()) {
            context.ledManager.clearWarningAnimation();
        }
        return;
    }

    uint8_t ctrlIdx = index - NUM_VIRTUAL_BUTTONS;
    switch (ctrlIdx) {
    case 3:
        context.ledManager.beginWarningAnimation(LEDWarning::Destructive);
        break;
    case 5:
        context.ledManager.beginWarningAnimation(LEDWarning::Diagnostic);
        break;
    default:
        if (context.ledManager.isWarningActive()) {
            context.ledManager.clearWarningAnimation();
        }
        break;
    }
}

void ButtonManager::selectMux(uint8_t row, uint8_t col) {
    setMuxFast(_cfg.muxrPins, row);
    setMuxFast(_cfg.muxcPins, col);
}

bool ButtonManager::isMuxButtonPressed(uint8_t index) const {
    // Matrix scan normalizes pressed buttons to HIGH (see processButtons/readMuxButton).
    return readMuxButton(index) == HIGH;
}
