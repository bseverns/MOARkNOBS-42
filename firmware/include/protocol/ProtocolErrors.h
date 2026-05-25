#ifndef MN42_PROTOCOL_ERRORS_H
#define MN42_PROTOCOL_ERRORS_H

#include <Arduino.h>
#include <cstdint>

void emitBulkError(const char *code, const char *message, uint32_t seq = 0);

#endif // MN42_PROTOCOL_ERRORS_H
