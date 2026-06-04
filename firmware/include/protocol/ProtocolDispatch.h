#ifndef MN42_PROTOCOL_DISPATCH_H
#define MN42_PROTOCOL_DISPATCH_H

#include <Arduino.h>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>

// ProtocolDispatch is the command-router submachine.
//
// ParsedCommand gives the protocol layer a cheap view over one complete serial
// line: command name plus payload. The dispatch table then routes that line to
// the correct handler family without forcing every caller to re-split strings.

namespace ProtocolDispatchHandlers {
struct ParsedCommand {
    explicit ParsedCommand(const String &source)
        : command(source), data(source.c_str()), length(source.length()),
          nameLen(measureNameLength(data, length)) {}

    const String &fullCommand() const { return command; }
    const char *c_str() const { return data; }
    size_t size() const { return length; }
    size_t nameLength() const { return nameLen; }
    const char *payload() const { return data + nameLen; }
    size_t payloadLength() const { return (length > nameLen) ? (length - nameLen) : 0U; }

    int compareName(const char *target) const {
        size_t targetLen = std::strlen(target);
        size_t cmpLen = std::min(nameLen, targetLen);
        int cmp = std::memcmp(data, target, cmpLen);
        if (cmp != 0) {
            return cmp;
        }
        if (nameLen < targetLen) {
            return -1;
        }
        if (nameLen > targetLen) {
            return 1;
        }
        return 0;
    }

  private:
    static size_t measureNameLength(const char *text, size_t capacity) {
        size_t index = 0;
        while (index < capacity) {
            char c = text[index];
            if (c == ',' || std::isspace(static_cast<unsigned char>(c))) {
                break;
            }
            ++index;
        }
        return index;
    }

    const String &command;
    const char *data;
    size_t length;
    size_t nameLen;
};

void handleGetAllCommand(const ParsedCommand &cmd);
void handleArpStartCommand(const ParsedCommand &cmd);
void handleArpStopCommand(const ParsedCommand &cmd);
void handleGetArpCommand(const ParsedCommand &cmd);
void handleGetArgMethodCommand(const ParsedCommand &cmd);
void handleGetBrownoutsCommand(const ParsedCommand &cmd);
void handleGetClockCommand(const ParsedCommand &cmd);
void handleGetConfigCommand(const ParsedCommand &cmd);
void handleGetDiagnosticsCommand(const ParsedCommand &cmd);
void handleGetEfCommand(const ParsedCommand &cmd);
void handleGetJitterCommand(const ParsedCommand &cmd);
void handleGetLedCommand(const ParsedCommand &cmd);
void handleGetManifestCommand(const ParsedCommand &cmd);
void handleGetModMatrixCommand(const ParsedCommand &cmd);
void handleGetNoteDynamicsCommand(const ParsedCommand &cmd);
void handleGetProfileCommand(const ParsedCommand &cmd);
void handleGetSchemaCommand(const ParsedCommand &cmd);
void handleGetUsbMidiCommand(const ParsedCommand &cmd);
void handleHelloCommand(const ParsedCommand &cmd);
void handleEnterConfigModeCommand(const ParsedCommand &cmd);
void handleMidiTestCommand(const ParsedCommand &cmd);
void handleLoadProfileCommand(const ParsedCommand &cmd);
void handleRecallMacroSlotCommand(const ParsedCommand &cmd);
void handleResetProfileCommand(const ParsedCommand &cmd);
void handleSaveProfileCommand(const ParsedCommand &cmd);
void handleSaveMacroSlotCommand(const ParsedCommand &cmd);
void handleSetAllCommand(const ParsedCommand &cmd);
void handleSetArgMethodCommand(const ParsedCommand &cmd);
void handleSetArpCommand(const ParsedCommand &cmd);
void handleSetClockCommand(const ParsedCommand &cmd);
void handleSetEfCommand(const ParsedCommand &cmd);
void handleSetEfIdleFloorCommand(const ParsedCommand &cmd);
void handleSetJitterCommand(const ParsedCommand &cmd);
void handleSetLedCommand(const ParsedCommand &cmd);
void handleSetNoteDynamicsCommand(const ParsedCommand &cmd);
void handleSetPotCommand(const ParsedCommand &cmd);
void handleSetProfileCommand(const ParsedCommand &cmd);
void handleSetSlotValueCommand(const ParsedCommand &cmd);
void handleSetUsbMidiCommand(const ParsedCommand &cmd);
} // namespace ProtocolDispatchHandlers

namespace ProtocolDispatch {
bool dispatchCommand(const String &command);
}

#endif // MN42_PROTOCOL_DISPATCH_H
