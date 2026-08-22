#ifndef MN42_PROTOCOL_CONFIG_BULK_TRANSPORT_H
#define MN42_PROTOCOL_CONFIG_BULK_TRANSPORT_H

#include <Arduino.h>

// Stateful SET_ALL transport lifecycle. Complete payloads cross into
// ConfigJsonApply only after chunk assembly and JSON parsing succeed.
void handleSetAllBulkCommand(const String &command);
void handleAbortSetAllBulkCommand(const String &command);
void serviceBulkConfigAssemblerTimeout();

#if defined(UNIT_TEST)
void testOnlyResetConfigBulkTransport();
bool testOnlyConfigBulkTransportInProgress();
void testOnlySeedConfigBulkAck(uint32_t sequence, const String &configId,
                               const String &appliedChecksum, uint32_t storageGeneration);
void testOnlyForceConfigBulkTimeout();
#endif

#endif // MN42_PROTOCOL_CONFIG_BULK_TRANSPORT_H
