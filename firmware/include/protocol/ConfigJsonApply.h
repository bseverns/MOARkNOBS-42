#ifndef PROTOCOL_CONFIG_JSON_APPLY_H
#define PROTOCOL_CONFIG_JSON_APPLY_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Transactionally validate and apply one complete SET_ALL config object.
// Chunk staging, timeout, idempotency, and ACK behavior live at the
// ConfigBulkTransport boundary.
bool parseConfigJsonPayload(const String &payload, uint32_t sequence, JsonDocument &document);
bool applyConfigJsonObject(JsonObject config, uint32_t sequence);

#if defined(UNIT_TEST)
String testOnlyAppliedStateChecksum();
#endif

#endif // PROTOCOL_CONFIG_JSON_APPLY_H
