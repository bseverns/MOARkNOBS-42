#ifndef MN42_PROTOCOL_CONFIG_BULK_TRANSPORT_H
#define MN42_PROTOCOL_CONFIG_BULK_TRANSPORT_H

#include <Arduino.h>

// Stateful SET_ALL transport lifecycle. Complete payloads cross into
// ConfigJsonApply only after chunk assembly and JSON parsing succeed.
void handleSetAllBulkCommand(const String &command);
void handleAbortSetAllBulkCommand(const String &command);
void serviceBulkConfigAssemblerTimeout();

#endif // MN42_PROTOCOL_CONFIG_BULK_TRANSPORT_H
