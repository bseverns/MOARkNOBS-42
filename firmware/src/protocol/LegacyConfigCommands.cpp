#include "protocol/LegacyConfigCommands.h"

#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "Log.h"

bool handleLegacyConfigCommand(const String &command, ConfigManager &config) {
    if (command.startsWith("CAL_ENVS")) {
        for (auto &ef : envelopeFollowers) ef.calibrate();
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
        return true;
    }
    if (command.startsWith("GET_FILTER")) {
        // Filters are slot-specific now; retain the old response for older hosts.
        LOG_PRINTLN("{\"type\":\"response\",\"message\":\"GET_FILTER deprecated\"}");
        return true;
    }
    if (command.startsWith("SET_FILTER")) {
        const int firstComma = command.indexOf(',');
        const int secondComma = command.indexOf(',', firstComma + 1);
        if (firstComma == -1 || secondComma == -1) {
            LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
            return true;
        }
        const uint8_t efType = command.substring(10, firstComma).toInt();
        const float freq = command.substring(firstComma + 1, secondComma).toFloat();
        const float q = command.substring(secondComma + 1).toFloat();
        config.setAllSlotEnvelopePayloads(efType, freq, q);
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
        return true;
    }
    if (command.startsWith("GET_SLOT_FILTER")) {
        const int slotIndex = command.substring(16).toInt();
        if (slotIndex < 0 || slotIndex >= static_cast<int>(NUM_SLOTS)) {
            LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
            return true;
        }
        LOG_PRINTLN(
            "{\"type\":\"response\",\"message\":\"GET_SLOT_FILTER deprecated\"}");
        return true;
    }
    if (command.startsWith("SET_SLOT_FILTER")) {
        const int first = command.indexOf(',');
        const int second = command.indexOf(',', first + 1);
        const int third = command.indexOf(',', second + 1);
        if (first == -1 || second == -1 || third == -1) {
            LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
            return true;
        }
        const int slotIndex = command.substring(16, first).toInt();
        const int rawType = command.substring(first + 1, second).toInt();
        const float freq = command.substring(second + 1, third).toFloat();
        const float q = command.substring(third + 1).toFloat();
        if (slotIndex < 0 || slotIndex >= static_cast<int>(NUM_SLOTS)) {
            LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
            return true;
        }
        SlotEnvelopePayload payload =
            config.getSlotEnvelopePayload(static_cast<uint8_t>(slotIndex));
        payload.filterType = static_cast<uint8_t>(rawType);
        payload.frequency = freq;
        payload.q = q;
        config.setSlotEnvelopePayload(static_cast<uint8_t>(slotIndex), payload);
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
        return true;
    }
    if (command.startsWith("GET_ARGPAIR")) {
        LOG_PRINTLN("{\"type\":\"response\",\"message\":\"GET_ARGPAIR deprecated\"}");
        return true;
    }
    if (command.startsWith("SET_ARGPAIR")) {
        const int first = command.indexOf(',');
        const int second = command.indexOf(',', first + 1);
        if (first == -1 || second == -1) {
            LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
            return true;
        }
        const uint8_t enable = command.substring(11, first).toInt();
        const uint8_t envA = command.substring(first + 1, second).toInt();
        const uint8_t envB = command.substring(second + 1).toInt();
        config.setARGEnable(enable);
        config.setEnvelopePair(envA, envB);

        int idxA = envelopeIndexFromAnalogPin(envA);
        if (idxA < 0) idxA = constrain(envA, 0, NUM_ENVELOPES - 1);
        int idxB = envelopeIndexFromAnalogPin(envB);
        if (idxB < 0) idxB = constrain(envB, 0, NUM_ENVELOPES - 1);

        for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; ++slotIndex) {
            MIDISlot &slot = config.getSlot(slotIndex);
            slot.arg.enabled = enable ? 1 : 0;
            slot.arg.sourceA = static_cast<uint8_t>(idxA);
            slot.arg.sourceB = static_cast<uint8_t>(idxB);
            config.saveSlot(slotIndex, slot);
        }
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
        return true;
    }
    return false;
}
