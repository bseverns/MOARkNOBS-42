#ifndef BUTTON_GESTURE_INTERPRETER_H
#define BUTTON_GESTURE_INTERPRETER_H

#include <stdint.h>

#include "ButtonGestureTiming.h"
#include "ButtonInputConstants.h"

inline constexpr unsigned long BUTTON_LONG_PRESS_DELAY = 500;
inline constexpr unsigned long BUTTON_CONFIRM_WINDOW_MS = 2000;
inline constexpr unsigned long BUTTON_COMBO_SETTLE_MS = 80;

enum class ButtonState : uint8_t {
    IDLE,
    PRESSED,
    LONG_PRESS
};

struct ButtonStateMachine {
    ButtonState state = ButtonState::IDLE;
    unsigned long pressTimestamp = 0;
    unsigned long releaseTimestamp = 0;
    bool longPressFired = false;
    unsigned long lastShortRelease = 0;
    bool shortPressPending = false;
};

enum class ButtonGestureEventType : uint8_t {
    PhysicalPress,
    PhysicalRelease,
    SinglePress,
    DoublePress,
    LongPressArmed,
    LongPressConfirmed,
    ConfirmationCancelled,
    ComboPress,
    ComboLongPress,
    ComboRelease
};

struct ButtonGestureEvent {
    ButtonGestureEventType type;
    uint8_t value;
};

struct ButtonGestureEvents {
    static constexpr uint8_t kCapacity = 8;
    ButtonGestureEvent items[kCapacity] = {};
    uint8_t count = 0;

    void add(ButtonGestureEventType type, uint8_t value) {
        if (count < kCapacity) items[count++] = {type, value};
    }
};

struct ButtonGestureMode {
    bool immediateShortPresses = false;
    bool longPressesEnabled = true;
};

// Pure timing/grammar machine. It consumes stable physical states and emits
// semantic events without touching GPIO, display, persistence, or runtime state.
class ButtonGestureInterpreter {
  public:
    void reset();
    ButtonGestureEvents updateButton(uint8_t index, bool pressed, unsigned long now,
                                     ButtonGestureMode mode = {});
    ButtonGestureEvents flushDeferred(unsigned long now);
    ButtonGestureEvents updateControlMask(uint8_t mask, unsigned long now,
                                          bool longCombosEnabled = true);

    bool cancelConfirmation();
    ButtonState state(uint8_t index) const;
    bool longPressFired(uint8_t index) const;
    int8_t pendingConfirmationIndex() const { return _confirmIndex; }

  private:
    static constexpr uint8_t kArpEditMask = (1U << 2) | (1U << 4);
    static constexpr uint8_t kSwingMask = (1U << 2) | (1U << 3);

    ButtonStateMachine _machines[NUM_BUTTON_INPUTS] = {};
    int8_t _confirmIndex = -1;
    unsigned long _confirmDeadline = 0;
    uint8_t _consumedControlMask = 0;
    uint8_t _comboHoldMask = 0;
    unsigned long _comboHoldTimestamp = 0;
    bool _comboLongPressFired = false;
    uint8_t _comboCandidateMask = 0;
    unsigned long _comboCandidateSince = 0;
    uint8_t _lastComboMask = 0;

    void resolveRelease(uint8_t index, unsigned long now, ButtonGestureMode mode,
                        ButtonGestureEvents &events);
    void consumeChordMembers(uint8_t mask);
    static bool isMultiButtonMask(uint8_t mask);
    static bool isLongControlCombo(uint8_t mask);
};

#endif // BUTTON_GESTURE_INTERPRETER_H
