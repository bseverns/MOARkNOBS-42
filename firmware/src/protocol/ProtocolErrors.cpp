#include "protocol/ProtocolErrors.h"

#include "Log.h"

void emitBulkError(const char *code, const char *message, uint32_t seq) {
    String out = "{\"type\":\"error\"";
    if (code && code[0] != '\0') {
        out += ",\"code\":\"";
        out += code;
        out += "\"";
    }
    if (seq != 0) {
        out += ",\"seq\":";
        out += seq;
    }
    if (message && message[0] != '\0') {
        out += ",\"message\":\"";
        out += message;
        out += "\"";
    }
    out += "}";
    LOG_PRINTLN(out);
}
