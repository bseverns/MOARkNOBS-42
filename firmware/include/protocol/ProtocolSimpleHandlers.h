#ifndef MN42_PROTOCOL_SIMPLE_HANDLERS_H
#define MN42_PROTOCOL_SIMPLE_HANDLERS_H

#include <Arduino.h>

// ProtocolSimpleHandlers is the direct read-handler family.
//
// These handlers serve the narrow host lanes that do not need the heavier
// profile/scene/bulk-config machinery. In practice that means:
// - identity/config export reads
// - live runtime inspection reads
// - a few deprecated compatibility shims still kept for older tools

namespace ProtocolSimpleHandlers {
// Deprecated compatibility shims.
void handleGetAllCommand(const String &command);
void handleGetArgMethodCommand(const String &command);
void handleGetBrownoutsCommand(const String &command);

// Identity and config export reads.
void handleHelloCommand(const String &command);
void handleGetManifestCommand(const String &command);
void handleGetSchemaCommand(const String &command);
void handleGetConfigCommand(const String &command);
void handleGetConfigChunkedCommand(const String &command);
void handleGetDiagnosticsCommand(const String &command);
void handleGetModMatrixCommand(const String &command);
void handleGetModMatrixChunkedCommand(const String &command);
void serviceChunkedReadOutput();

// Live runtime inspection reads.
void handleGetClockCommand(const String &command);
void handleGetArpCommand(const String &command);
void handleGetEfCommand(const String &command);
void handleGetJitterCommand(const String &command);
void handleGetLedCommand(const String &command);
void handleGetNoteDynamicsCommand(const String &command);
void handleGetUsbMidiCommand(const String &command);
void handleMidiTestCommand(const String &command);

} // namespace ProtocolSimpleHandlers

#endif // MN42_PROTOCOL_SIMPLE_HANDLERS_H
