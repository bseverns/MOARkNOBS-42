#include "MachineParameterService.h"

#include <Arduino.h>
#include <algorithm>

#include "Arpeggiator.h"
#include "FirmwareState.h"
#include "ConfigManager.h"
#include "Globals.h"
#include "LFO/LFOManager.h"
#include "MIDIHandler.h"
#include "PotentiometerManager.h"

namespace {
uint8_t scaleToMidi(float value, float low, float high) {
    if (high <= low) return 0;
    return static_cast<uint8_t>(constrain(lroundf(((value - low) * 127.0f) / (high - low)), 0L,
                                          127L));
}

float scaleFromMidi(uint8_t value, float low, float high) {
    return low + (static_cast<float>(value) / 127.0f) * (high - low);
}
} // namespace

namespace MachineParameterService {
bool apply(MachineParameterTarget target, uint8_t targetIndex, uint8_t value,
           MidiInputPort origin) {
    (void)origin;
    value = static_cast<uint8_t>(constrain(static_cast<int>(value), 0, 127));
    switch (target) {
    case MachineParameterTarget::SlotValue:
        if (targetIndex >= NUM_SLOTS) return false;
        {
            MIDISlot &slot = configManager.getSlot(targetIndex);
            const bool hasFixedLfo = std::any_of(
                slot.lfo.lfo.begin(), slot.lfo.lfo.end(),
                [](const SlotLfoLane &lane) { return lane.enabled(); });
            // The first input slice intentionally owns only direct CC lanes.
            // Note/NRPN/SysEx triggering and modulated baselines need richer
            // origin tracking before they can be made loop-safe.
            if (!slot.active || slot.type != MIDIMessageType::CC ||
                slot.getEnvelopeFollowerIndex() >= 0 || hasFixedLfo ||
                lfoManager.slotIsRouted(targetIndex)) {
                return false;
            }
            potentiometerManager.injectMidiValue(targetIndex, value, false);
            midiHandler.sendControlChange(slot.data1, value, slot.midiChannel, origin);
        }
        return true;
    case MachineParameterTarget::ArpSwing:
        arpeggiator.setSwingPercent(scaleFromMidi(value, 0.0f, 80.0f));
        return true;
    case MachineParameterTarget::VelocityShift:
        velocityShift = static_cast<int8_t>(map(value, 0, 127, -64, 63));
        g_noteDynamicsRemoteControlActive = true;
        g_noteDynamicsShiftLatched = false;
        return true;
    case MachineParameterTarget::NoteChance:
        changeProbability = static_cast<uint8_t>(map(value, 0, 127, 0, 100));
        g_noteDynamicsRemoteControlActive = true;
        g_noteDynamicsProbabilityLatched = false;
        return true;
    case MachineParameterTarget::ArpGate:
        arpeggiator.setGatePercent(scaleFromMidi(value, 5.0f, 100.0f));
        return true;
    case MachineParameterTarget::JitterDepth:
        g_jitterSettings.depth = scaleFromMidi(value, 0.0f, 1.0f);
        g_jitterRemoteControlActive = true;
        g_jitterDepthLatched = false;
        return true;
    case MachineParameterTarget::JitterSmoothness:
        g_jitterSettings.smoothness = scaleFromMidi(value, 0.0f, 1.0f);
        g_jitterRemoteControlActive = true;
        g_jitterSmoothnessLatched = false;
        return true;
    }
    return false;
}

bool read(MachineParameterTarget target, uint8_t targetIndex, uint8_t &value) {
    switch (target) {
    case MachineParameterTarget::SlotValue:
        if (targetIndex >= NUM_SLOTS) return false;
        value = static_cast<uint8_t>(constrain(map(potentiometerManager.getLastValue(targetIndex),
                                                   0, 1023, 0, 127),
                                               0L, 127L));
        return true;
    case MachineParameterTarget::ArpSwing:
        value = scaleToMidi(arpeggiator.getSwingPercent(), 0.0f, 80.0f);
        return true;
    case MachineParameterTarget::VelocityShift:
        value = static_cast<uint8_t>(map(velocityShift, -64, 63, 0, 127));
        return true;
    case MachineParameterTarget::NoteChance:
        value = static_cast<uint8_t>(map(changeProbability, 0, 100, 0, 127));
        return true;
    case MachineParameterTarget::ArpGate:
        value = scaleToMidi(arpeggiator.getGatePercent(), 5.0f, 100.0f);
        return true;
    case MachineParameterTarget::JitterDepth:
        value = scaleToMidi(g_jitterSettings.depth, 0.0f, 1.0f);
        return true;
    case MachineParameterTarget::JitterSmoothness:
        value = scaleToMidi(g_jitterSettings.smoothness, 0.0f, 1.0f);
        return true;
    }
    return false;
}
} // namespace MachineParameterService
