#ifndef PROTOCOL_CONFIG_JSON_APPLY_H
#define PROTOCOL_CONFIG_JSON_APPLY_H

#include <Arduino.h>

// ConfigJsonApply is the bulk-config apply submachine behind `SET_ALL`.
//
// It owns chunk assembly, checksum/ACK discipline, JSON parsing, and the
// mutation of live firmware state once a staged full-config payload is ready.

void handleSetAllBulkCommand(const String &command);
void handleAbortSetAllBulkCommand(const String &command);
void serviceBulkConfigAssemblerTimeout();

#if defined(UNIT_TEST)
String testOnlyAppliedStateChecksum();
#endif

#endif // PROTOCOL_CONFIG_JSON_APPLY_H
