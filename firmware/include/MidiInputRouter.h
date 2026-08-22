#ifndef MIDI_INPUT_ROUTER_H
#define MIDI_INPUT_ROUTER_H

#include "MidiInputTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>

class MidiInputRouter {
  public:
    using ApplyCallback = bool (*)(MachineParameterTarget, uint8_t, uint8_t, MidiInputPort);
    using ReadCallback = bool (*)(MachineParameterTarget, uint8_t, uint8_t &);

    void setCallbacks(ApplyCallback apply, ReadCallback read);
    void setBindings(const MidiInputBinding *bindings, size_t count);
    size_t bindingCount() const { return count_; }
    bool getBinding(size_t index, MidiInputBinding &binding) const;
    uint8_t routeControlChange(MidiInputPort port, uint8_t channel, uint8_t controller,
                               uint8_t value);

  private:
    struct RuntimeState {
        uint8_t previousInput = 0;
        uint8_t lastApplied = 0;
        bool hasPreviousInput = false;
        bool hasLastApplied = false;
        bool pickupAcquired = false;
        bool toggleOn = false;
    };

    static bool bindingIsValid(const MidiInputBinding &binding);
    static uint8_t mapValue(const MidiInputBinding &binding, uint8_t input);
    bool pickupAllows(size_t index, const MidiInputBinding &binding, uint8_t candidate);

    std::array<MidiInputBinding, MIDI_INPUT_MAX_BINDINGS> bindings_{};
    std::array<RuntimeState, MIDI_INPUT_MAX_BINDINGS> state_{};
    size_t count_ = 0;
    ApplyCallback apply_ = nullptr;
    ReadCallback read_ = nullptr;
};

#endif // MIDI_INPUT_ROUTER_H
