#ifndef MN42_PROTOCOL_SIMPLE_HANDLERS_H
#define MN42_PROTOCOL_SIMPLE_HANDLERS_H

#include <Arduino.h>

namespace ProtocolSimpleHandlers {
void handleGetAllCommand(const String &command);
void handleGetArgMethodCommand(const String &command);
void handleGetBrownoutsCommand(const String &command);
void handleGetClockCommand(const String &command);
void handleGetConfigCommand(const String &command);
void handleGetEfCommand(const String &command);
void handleGetJitterCommand(const String &command);
void handleGetLedCommand(const String &command);
void handleGetManifestCommand(const String &command);
void handleGetNoteDynamicsCommand(const String &command);
void handleGetSchemaCommand(const String &command);
void handleGetUsbMidiCommand(const String &command);
void handleHelloCommand(const String &command);
void handleSetArgMethodCommand(const String &command);
void handleSetClockCommand(const String &command);
void handleSetEfCommand(const String &command);
void handleSetJitterCommand(const String &command);
void handleSetLedCommand(const String &command);
void handleSetNoteDynamicsCommand(const String &command);
void handleSetPotCommand(const String &command);
void handleSetSlotValueCommand(const String &command);
void handleSetUsbMidiCommand(const String &command);
} // namespace ProtocolSimpleHandlers

#endif // MN42_PROTOCOL_SIMPLE_HANDLERS_H
