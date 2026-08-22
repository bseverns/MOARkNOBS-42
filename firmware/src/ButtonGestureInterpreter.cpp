#include "ButtonGestureInterpreter.h"

void ButtonGestureInterpreter::reset() {
    for (uint8_t index = 0; index < NUM_BUTTON_INPUTS; ++index) {
        _machines[index] = {};
    }
    _confirmIndex = -1;
    _confirmDeadline = 0;
    _consumedControlMask = 0;
    _comboHoldMask = 0;
    _comboHoldTimestamp = 0;
    _comboLongPressFired = false;
    _comboCandidateMask = 0;
    _comboCandidateSince = 0;
    _lastComboMask = 0;
    _suppressComboSubsetsUntilRelease = false;
}

bool ButtonGestureInterpreter::cancelConfirmation() {
    const bool cancelled = _confirmIndex >= 0;
    _confirmIndex = -1;
    _confirmDeadline = 0;
    return cancelled;
}

ButtonState ButtonGestureInterpreter::state(uint8_t index) const {
    return index < NUM_BUTTON_INPUTS ? _machines[index].state : ButtonState::IDLE;
}

bool ButtonGestureInterpreter::longPressFired(uint8_t index) const {
    return index < NUM_BUTTON_INPUTS && _machines[index].longPressFired;
}

ButtonGestureEvents ButtonGestureInterpreter::updateButton(uint8_t index, bool pressed,
                                                           unsigned long now,
                                                           ButtonGestureMode mode) {
    ButtonGestureEvents events;
    if (index >= NUM_BUTTON_INPUTS) return events;

    if (_confirmIndex >= 0 && now > _confirmDeadline) {
        cancelConfirmation();
        events.add(ButtonGestureEventType::ConfirmationCancelled, index);
    }
    if (_confirmIndex >= 0 && index != static_cast<uint8_t>(_confirmIndex) && pressed) {
        cancelConfirmation();
        events.add(ButtonGestureEventType::ConfirmationCancelled, index);
    }

    ButtonStateMachine &machine = _machines[index];
    switch (machine.state) {
    case ButtonState::IDLE:
        if (pressed) {
            machine.state = ButtonState::PRESSED;
            machine.pressTimestamp = now;
            machine.longPressFired = false;
            events.add(ButtonGestureEventType::PhysicalPress, index);
        }
        break;

    case ButtonState::PRESSED:
        if (!pressed) {
            machine.releaseTimestamp = now;
            machine.state = ButtonState::IDLE;
            events.add(ButtonGestureEventType::PhysicalRelease, index);
            resolveRelease(index, now, mode, events);
        } else if (mode.longPressesEnabled && !machine.longPressFired &&
                   (now - machine.pressTimestamp >= BUTTON_LONG_PRESS_DELAY)) {
            machine.state = ButtonState::LONG_PRESS;
            machine.longPressFired = true;
            _confirmIndex = static_cast<int8_t>(index);
            _confirmDeadline = now + BUTTON_CONFIRM_WINDOW_MS;
            events.add(ButtonGestureEventType::LongPressArmed, index);
        }
        break;

    case ButtonState::LONG_PRESS:
        if (!pressed) {
            machine.state = ButtonState::IDLE;
            machine.releaseTimestamp = now;
            events.add(ButtonGestureEventType::PhysicalRelease, index);
        }
        break;
    }
    return events;
}

void ButtonGestureInterpreter::resolveRelease(uint8_t index, unsigned long now,
                                              ButtonGestureMode mode,
                                              ButtonGestureEvents &events) {
    ButtonStateMachine &machine = _machines[index];
    if (machine.longPressFired) return;

    if (index >= NUM_VIRTUAL_BUTTONS) {
        const uint8_t controlIndex = static_cast<uint8_t>(index - NUM_VIRTUAL_BUTTONS);
        const uint8_t controlMask = static_cast<uint8_t>(1U << controlIndex);
        if ((_consumedControlMask & controlMask) != 0) {
            _consumedControlMask &= static_cast<uint8_t>(~controlMask);
            consumeDeferredPress(machine.shortPressPending, machine.lastShortRelease);
            return;
        }
    }

    if (_confirmIndex == static_cast<int8_t>(index)) {
        const bool withinWindow = now <= _confirmDeadline;
        cancelConfirmation();
        events.add(ButtonGestureEventType::ConfirmationCancelled, index);
        if (withinWindow) events.add(ButtonGestureEventType::LongPressConfirmed, index);
        return;
    }

    if (mode.immediateShortPresses) {
        events.add(ButtonGestureEventType::SinglePress, index);
        consumeDeferredPress(machine.shortPressPending, machine.lastShortRelease);
        return;
    }

    const bool deferredControl = index >= NUM_VIRTUAL_BUTTONS + 3;
    if (deferredControl) {
        const DeferredPressDecision decision =
            registerDeferredRelease(machine.shortPressPending, machine.lastShortRelease, now);
        if (decision.fireSingle) events.add(ButtonGestureEventType::SinglePress, index);
        if (decision.fireDouble) events.add(ButtonGestureEventType::DoublePress, index);
        return;
    }

    if (machine.lastShortRelease != 0 &&
        (now - machine.lastShortRelease) < DOUBLE_PRESS_DELAY) {
        events.add(ButtonGestureEventType::DoublePress, index);
        machine.lastShortRelease = 0;
    } else {
        events.add(ButtonGestureEventType::SinglePress, index);
        machine.lastShortRelease = now;
    }
}

ButtonGestureEvents ButtonGestureInterpreter::flushDeferred(unsigned long now) {
    ButtonGestureEvents events;
    for (uint8_t controlIndex = 3; controlIndex < NUM_CONTROL_BUTTONS; ++controlIndex) {
        const uint8_t index = static_cast<uint8_t>(NUM_VIRTUAL_BUTTONS + controlIndex);
        ButtonStateMachine &machine = _machines[index];
        if (flushDeferredPress(machine.shortPressPending, machine.lastShortRelease, now)) {
            events.add(ButtonGestureEventType::SinglePress, index);
        }
    }
    return events;
}

bool ButtonGestureInterpreter::isMultiButtonMask(uint8_t mask) {
    return mask != 0 && (mask & static_cast<uint8_t>(mask - 1)) != 0;
}

bool ButtonGestureInterpreter::isLongControlCombo(uint8_t mask) {
    return mask == kArpEditMask || mask == kSwingMask;
}

void ButtonGestureInterpreter::consumeChordMembers(uint8_t mask) {
    _consumedControlMask |= mask;
    for (uint8_t controlIndex = 0; controlIndex < NUM_CONTROL_BUTTONS; ++controlIndex) {
        if ((mask & (1U << controlIndex)) == 0) continue;
        ButtonStateMachine &machine = _machines[NUM_VIRTUAL_BUTTONS + controlIndex];
        consumeDeferredPress(machine.shortPressPending, machine.lastShortRelease);
    }
}

ButtonGestureEvents ButtonGestureInterpreter::updateControlMask(uint8_t mask, unsigned long now,
                                                                bool longCombosEnabled) {
    ButtonGestureEvents events;
    const bool multiPressed = isMultiButtonMask(mask);
    const bool longCombo = longCombosEnabled && isLongControlCombo(mask);

    if (multiPressed) consumeChordMembers(mask);

    // Once a settled ordinary chord fires, rolling fingers off it must not
    // reinterpret any remaining subset as a fresh chord. Re-arm only after
    // every control member has been released.
    if (_suppressComboSubsetsUntilRelease) {
        if (mask == 0) {
            _suppressComboSubsetsUntilRelease = false;
            _comboHoldMask = 0;
            _comboCandidateMask = 0;
            _comboCandidateSince = 0;
            _lastComboMask = 0;
        }
        return events;
    }

    if (mask != _comboHoldMask) {
        const bool heldSpecialCombo = isLongControlCombo(_comboHoldMask);
        if (heldSpecialCombo) {
            const bool allHeldMembersRemainDown =
                (mask & _comboHoldMask) == _comboHoldMask;
            if (!allHeldMembersRemainDown) {
                if (!_comboLongPressFired) {
                    events.add(ButtonGestureEventType::ComboPress, _comboHoldMask);
                }
                events.add(ButtonGestureEventType::ComboRelease, _comboHoldMask);
            }
            if (allHeldMembersRemainDown && _comboLongPressFired) {
                // The long action is already active. Keep tracking its members
                // through added fingers so its eventual release still arrives.
                return events;
            }
        }
        _comboHoldMask = longCombo ? mask : 0;
        _comboHoldTimestamp = now;
        _comboLongPressFired = false;
    }

    if (longCombo && !_comboLongPressFired &&
        (now - _comboHoldTimestamp >= BUTTON_LONG_PRESS_DELAY)) {
        _comboLongPressFired = true;
        consumeChordMembers(mask);
        events.add(ButtonGestureEventType::ComboLongPress, mask);
    }

    if (multiPressed && !longCombo) {
        if (mask != _comboCandidateMask) {
            _comboCandidateMask = mask;
            _comboCandidateSince = now;
        }
        if (mask != _lastComboMask && (now - _comboCandidateSince >= BUTTON_COMBO_SETTLE_MS)) {
            events.add(ButtonGestureEventType::ComboPress, mask);
            _lastComboMask = mask;
            _suppressComboSubsetsUntilRelease = true;
        }
    } else {
        _comboCandidateMask = 0;
        _comboCandidateSince = 0;
        if (mask != _lastComboMask) _lastComboMask = mask;
    }
    return events;
}
