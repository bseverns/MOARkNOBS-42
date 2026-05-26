#include "protocol/ProtocolDispatch.h"

#include "ConfigManager.h"
#include "Log.h"
#include "Protocol.h"

// ProtocolDispatch.cpp is the routing table for one fully assembled host line.
//
// It should stay mechanically simple:
// - parse the command name once
// - binary-search the sorted handler table
// - hand the request to the owning protocol submachine
// - fall back to ConfigManager's legacy command lane only if no named handler matches

namespace {
using ProtocolDispatchHandlers::ParsedCommand;

struct CommandHandler {
    const char *name;
    void (*handler)(const ParsedCommand &cmd);
};

// Keep this table lexicographically sorted; `findCommandHandler()` does a binary search.
const CommandHandler kCommandHandlers[] = {
    {"ARP_START", ProtocolDispatchHandlers::handleArpStartCommand},
    {"ARP_STOP", ProtocolDispatchHandlers::handleArpStopCommand},
    {"ENTER_CONFIG_MODE", ProtocolDispatchHandlers::handleEnterConfigModeCommand},
    {"GET_ALL", ProtocolDispatchHandlers::handleGetAllCommand},
    {"GET_ARGMETHOD", ProtocolDispatchHandlers::handleGetArgMethodCommand},
    {"GET_BROWNOUTS", ProtocolDispatchHandlers::handleGetBrownoutsCommand},
    {"GET_CLOCK", ProtocolDispatchHandlers::handleGetClockCommand},
    {"GET_CONFIG", ProtocolDispatchHandlers::handleGetConfigCommand},
    {"GET_EF", ProtocolDispatchHandlers::handleGetEfCommand},
    {"GET_JITTER", ProtocolDispatchHandlers::handleGetJitterCommand},
    {"GET_LED", ProtocolDispatchHandlers::handleGetLedCommand},
    {"GET_MANIFEST", ProtocolDispatchHandlers::handleGetManifestCommand},
    {"GET_NOTE_DYNAMICS", ProtocolDispatchHandlers::handleGetNoteDynamicsCommand},
    {"GET_PROFILE", ProtocolDispatchHandlers::handleGetProfileCommand},
    {"GET_SCHEMA", ProtocolDispatchHandlers::handleGetSchemaCommand},
    {"GET_USB_MIDI", ProtocolDispatchHandlers::handleGetUsbMidiCommand},
    {"HELLO", ProtocolDispatchHandlers::handleHelloCommand},
    {"LOAD_PROFILE", ProtocolDispatchHandlers::handleLoadProfileCommand},
    {"RECALL_MACRO_SLOT", ProtocolDispatchHandlers::handleRecallMacroSlotCommand},
    {"RESET_PROFILE", ProtocolDispatchHandlers::handleResetProfileCommand},
    {"SAVE_MACRO_SLOT", ProtocolDispatchHandlers::handleSaveMacroSlotCommand},
    {"SAVE_PROFILE", ProtocolDispatchHandlers::handleSaveProfileCommand},
    {"SET_ALL", ProtocolDispatchHandlers::handleSetAllCommand},
    {"SET_ARGMETHOD", ProtocolDispatchHandlers::handleSetArgMethodCommand},
    {"SET_CLOCK", ProtocolDispatchHandlers::handleSetClockCommand},
    {"SET_EF", ProtocolDispatchHandlers::handleSetEfCommand},
    {"SET_EF_IDLE_FLOOR", ProtocolDispatchHandlers::handleSetEfIdleFloorCommand},
    {"SET_JITTER", ProtocolDispatchHandlers::handleSetJitterCommand},
    {"SET_LED", ProtocolDispatchHandlers::handleSetLedCommand},
    {"SET_NOTE_DYNAMICS", ProtocolDispatchHandlers::handleSetNoteDynamicsCommand},
    {"SET_POT", ProtocolDispatchHandlers::handleSetPotCommand},
    {"SET_PROFILE", ProtocolDispatchHandlers::handleSetProfileCommand},
    {"SET_SLOT_VALUE", ProtocolDispatchHandlers::handleSetSlotValueCommand},
    {"SET_USB_MIDI", ProtocolDispatchHandlers::handleSetUsbMidiCommand},
};

constexpr size_t kCommandHandlerCount = sizeof(kCommandHandlers) / sizeof(kCommandHandlers[0]);

const CommandHandler *findCommandHandler(const ParsedCommand &cmd) {
    size_t low = 0;
    size_t high = kCommandHandlerCount;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        int comparison = cmd.compareName(kCommandHandlers[mid].name);
        if (comparison == 0) {
            return &kCommandHandlers[mid];
        }
        if (comparison < 0) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    return nullptr;
}

void logUnknownCommand(const String &command) {
    LOG_PRINTLN("Unknown command: " + command);
    LOG_PRINT("Available commands: ");
    for (size_t i = 0; i < kCommandHandlerCount; ++i) {
        LOG_PRINT(kCommandHandlers[i].name);
        if (i + 1 < kCommandHandlerCount) {
            LOG_PRINT(", ");
        }
    }
    LOG_PRINTLN("");
}
} // namespace

namespace ProtocolDispatch {
bool dispatchCommand(const String &command) {
    ParsedCommand parsed(command);
    if (const CommandHandler *handler = findCommandHandler(parsed)) {
        handler->handler(parsed);
        return true;
    }
    if (configManager.handleCommand(command)) {
        return true;
    }
    logUnknownCommand(command);
    return false;
}
} // namespace ProtocolDispatch

#if defined(UNIT_TEST)
bool testOnly_dispatchCommand(const String &command) {
    return ProtocolDispatch::dispatchCommand(command);
}
#endif
