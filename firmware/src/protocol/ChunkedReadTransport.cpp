#include "protocol/ChunkedReadTransport.h"

#include <ArduinoJson.h>
#include <algorithm>
#include <cstdint>

#include "Log.h"

namespace ChunkedReadTransport {
namespace {
// Headers consume roughly 80 bytes and JSON escaping can double the payload.
// Keep every response comfortably inside the native serial line guard.
constexpr size_t kPayloadBytes = 20;
constexpr uint8_t kFramesPerService = 2;

struct PendingRead {
    String payload;
    String command;
    uint32_t checksum = 0;
    size_t index = 0;
    size_t total = 0;
    bool active = false;
} pendingRead;

uint32_t fnv1a(const String &value) {
    uint32_t hash = 2166136261UL;
    for (size_t i = 0; i < value.length(); ++i) {
        hash ^= static_cast<uint8_t>(value[i]);
        hash *= 16777619UL;
    }
    return hash;
}
} // namespace

void begin(const char *command, const String &payload) {
    pendingRead.payload = payload;
    pendingRead.command = command;
    pendingRead.checksum = fnv1a(payload);
    pendingRead.index = 0;
    pendingRead.total = (payload.length() + kPayloadBytes - 1) / kPayloadBytes;
    pendingRead.active = pendingRead.total > 0;
}

void service() {
    if (!pendingRead.active) return;
    for (uint8_t emitted = 0;
         emitted < kFramesPerService && pendingRead.index < pendingRead.total;
         ++emitted, ++pendingRead.index) {
        StaticJsonDocument<192> frame;
        frame["type"] = "read_chunk";
        frame["command"] = pendingRead.command;
        frame["index"] = pendingRead.index;
        frame["total"] = pendingRead.total;
        frame["checksum"] = pendingRead.checksum;
        frame["data"] = pendingRead.payload.substring(
            pendingRead.index * kPayloadBytes,
            std::min(pendingRead.payload.length(), (pendingRead.index + 1) * kPayloadBytes));
        String line;
        serializeJson(frame, line);
        LOG_PRINTLN(line);
    }
    if (pendingRead.index >= pendingRead.total) {
        pendingRead.payload = "";
        pendingRead.active = false;
    }
}
} // namespace ChunkedReadTransport
