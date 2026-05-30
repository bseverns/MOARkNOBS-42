#include "CommandQueue.h"

#include <Arduino.h>
#include <cstring>

#include "Globals.h"
#include "Log.h"

namespace {
// Full SET_ALL payloads can span 100+ serial lines; keep enough headroom so
// chunked apply traffic does not drop older fragments before reassembly.
constexpr size_t kMaxCommandQueueSize = 192;

struct CommandQueueStorage {
    char entries[kMaxCommandQueueSize][SERIAL_BUFFER_SIZE] = {{0}};
    size_t head = 0;
    size_t tail = 0;
    size_t count = 0;
};

// The queue is large because chunked SET_ALL frames can arrive as many serial
// lines. Keep it in RAM2 so it doesn't crowd out the tighter RAM1 budget used
// by the Unity test image and hot runtime state.
DMAMEM CommandQueueStorage commandQueue;

// Drop the oldest queued command so fresh operator input wins during overload.
void dropOldestCommand() {
    if (commandQueue.count == 0) {
        return;
    }
    commandQueue.head = (commandQueue.head + 1) % kMaxCommandQueueSize;
    --commandQueue.count;
}

// Push one complete serial line into the ring buffer.
void enqueueSerialCommand(const char *line) {
    if (!line) {
        return;
    }
    if (commandQueue.count >= kMaxCommandQueueSize) {
        // Keep newest commands under overload; interactive control is more useful than
        // preserving stale backlog lines.
        LOG_PRINTLN("Warning: Command queue overflow, dropping oldest command");
        dropOldestCommand();
    }

    char *slot = commandQueue.entries[commandQueue.tail];
    const size_t copyLen = strnlen(line, SERIAL_BUFFER_SIZE - 1);
    std::memcpy(slot, line, copyLen);
    slot[copyLen] = '\0';
    commandQueue.tail = (commandQueue.tail + 1) % kMaxCommandQueueSize;
    ++commandQueue.count;
}

// Accumulate serial bytes into newline-delimited commands.
void ingestSerialByte(char received) {
    if (received == '\n' || serialBufferIndex >= SERIAL_BUFFER_SIZE - 1) {
        serialBuffer[serialBufferIndex] = '\0';
        if (serialBufferIndex >= SERIAL_BUFFER_SIZE - 1) {
            LOG_PRINTLN("Error: Command too long");
        }
        enqueueSerialCommand(serialBuffer);
        serialBufferIndex = 0;
    } else if (received != '\r') {
        serialBuffer[serialBufferIndex++] = received;
    }
}
} // namespace

// Pop the next queued command into the caller-provided buffer.
bool dequeueSerialCommand(char *outBuffer, size_t outBufferSize) {
    if (!outBuffer || outBufferSize == 0 || commandQueue.count == 0) {
        return false;
    }

    const char *entry = commandQueue.entries[commandQueue.head];
    const size_t copyLen = strnlen(entry, outBufferSize - 1);
    std::memcpy(outBuffer, entry, copyLen);
    outBuffer[copyLen] = '\0';
    commandQueue.head = (commandQueue.head + 1) % kMaxCommandQueueSize;
    --commandQueue.count;
    return true;
}

// Drain the hardware serial input into the command ring buffer.
void pollSerialInput() {
    while (Serial.available()) {
        ingestSerialByte(static_cast<char>(Serial.read()));
    }
}

#if defined(UNIT_TEST)
// Reset queue storage so tests start from a blank serial-command state.
void testOnly_resetCommandQueue() {
    commandQueue = CommandQueueStorage{};
    std::memset(serialBuffer, 0, sizeof(serialBuffer));
    serialBufferIndex = 0;
}

// Inject a full command line directly into the queue for tests.
void testOnly_enqueueSerialCommand(const char *line) { enqueueSerialCommand(line); }

// Feed one raw serial byte into the queue parser for tests.
void testOnly_ingestSerialByte(char received) { ingestSerialByte(received); }
#endif
