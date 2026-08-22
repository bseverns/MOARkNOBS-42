#include "MidiInputRouter.h"

#include <algorithm>
#include <cstdlib>

void MidiInputRouter::setCallbacks(ApplyCallback apply, ReadCallback read) {
    apply_ = apply;
    read_ = read;
}

bool MidiInputRouter::bindingIsValid(const MidiInputBinding &binding) {
    const auto port = static_cast<MidiInputPort>(binding.port);
    const auto mode = static_cast<MidiInputMode>(binding.mode);
    const auto target = static_cast<MachineParameterTarget>(binding.target);
    if (port > MidiInputPort::Usb || mode > MidiInputMode::Toggle ||
        target > MachineParameterTarget::JitterSmoothness) {
        return false;
    }
    if (binding.channel < 1 || binding.channel > 16 || binding.controller > 127 ||
        binding.minValue > binding.maxValue) {
        return false;
    }
    return target != MachineParameterTarget::SlotValue || binding.targetIndex < 42;
}

void MidiInputRouter::setBindings(const MidiInputBinding *bindings, size_t count) {
    count_ = 0;
    state_ = {};
    if (!bindings) return;
    const size_t bounded = std::min(count, bindings_.size());
    for (size_t i = 0; i < bounded; ++i) {
        if (!bindingIsValid(bindings[i])) continue;
        bindings_[count_++] = bindings[i];
    }
}

bool MidiInputRouter::getBinding(size_t index, MidiInputBinding &binding) const {
    if (index >= count_) return false;
    binding = bindings_[index];
    return true;
}

uint8_t MidiInputRouter::mapValue(const MidiInputBinding &binding, uint8_t input) {
    const uint16_t span = static_cast<uint16_t>(binding.maxValue - binding.minValue);
    return static_cast<uint8_t>(binding.minValue + ((span * input + 63U) / 127U));
}

bool MidiInputRouter::pickupAllows(size_t index, const MidiInputBinding &binding,
                                   uint8_t candidate) {
    RuntimeState &state = state_[index];
    if ((binding.flags & MIDI_INPUT_FLAG_SOFT_TAKEOVER) == 0 || !read_) {
        state.pickupAcquired = true;
        return true;
    }
    uint8_t current = 0;
    if (!read_(static_cast<MachineParameterTarget>(binding.target), binding.targetIndex, current)) {
        state.pickupAcquired = true;
        return true;
    }
    // Re-arm pickup when another control surface or the physical knob has
    // moved the destination away from the last value this binding applied.
    if (state.pickupAcquired && state.hasLastApplied &&
        std::abs(static_cast<int>(current) - static_cast<int>(state.lastApplied)) > 2) {
        state.pickupAcquired = false;
    }
    if (state.pickupAcquired) return true;
    const bool close = std::abs(static_cast<int>(candidate) - static_cast<int>(current)) <= 2;
    const uint8_t previousCandidate =
        state.hasPreviousInput ? mapValue(binding, state.previousInput) : candidate;
    const bool crossed = state.hasPreviousInput &&
                         ((previousCandidate <= current && candidate >= current) ||
                          (previousCandidate >= current && candidate <= current));
    if (close || crossed) {
        state.pickupAcquired = true;
        return true;
    }
    return false;
}

uint8_t MidiInputRouter::routeControlChange(MidiInputPort port, uint8_t channel,
                                            uint8_t controller, uint8_t value) {
    if (!apply_ || channel < 1 || channel > 16 || controller > 127 || value > 127) return 0;
    uint8_t applied = 0;
    for (size_t i = 0; i < count_; ++i) {
        const MidiInputBinding &binding = bindings_[i];
        RuntimeState &state = state_[i];
        const auto filter = static_cast<MidiInputPort>(binding.port);
        if ((filter != MidiInputPort::Any && filter != port) || binding.channel != channel ||
            binding.controller != controller) {
            continue;
        }

        const auto mode = static_cast<MidiInputMode>(binding.mode);
        uint8_t candidate = mapValue(binding, value);
        bool shouldApply = true;
        if (mode == MidiInputMode::Momentary) {
            candidate = value == 0 ? binding.minValue : binding.maxValue;
        } else if (mode == MidiInputMode::Toggle) {
            shouldApply = value > 0 && (!state.hasPreviousInput || state.previousInput == 0);
            if (shouldApply) {
                state.toggleOn = !state.toggleOn;
                candidate = state.toggleOn ? binding.maxValue : binding.minValue;
            }
        } else {
            shouldApply = pickupAllows(i, binding, candidate);
        }

        state.previousInput = value;
        state.hasPreviousInput = true;
        if (shouldApply && apply_(static_cast<MachineParameterTarget>(binding.target),
                                  binding.targetIndex, candidate, port)) {
            state.lastApplied = candidate;
            state.hasLastApplied = true;
            ++applied;
        }
    }
    return applied;
}
