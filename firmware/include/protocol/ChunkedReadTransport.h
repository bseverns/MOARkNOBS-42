#ifndef MN42_PROTOCOL_CHUNKED_READ_TRANSPORT_H
#define MN42_PROTOCOL_CHUNKED_READ_TRANSPORT_H

#include <Arduino.h>

namespace ChunkedReadTransport {
// Replace any pending read with a new payload and reset frame numbering.
void begin(const char *command, const String &payload);

// Emit a bounded number of frames so large reads cannot monopolize the loop.
void service();
} // namespace ChunkedReadTransport

#endif // MN42_PROTOCOL_CHUNKED_READ_TRANSPORT_H
