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
bool discardingOverlongLine = false;

void resetCommandQueueState() {
    commandQueue = CommandQueueStorage{};
    std::memset(serialBuffer, 0, sizeof(serialBuffer));
    serialBufferIndex = 0;
    discardingOverlongLine = false;
}

bool commandQueueStateLooksValid() {
    return commandQueue.head < kMaxCommandQueueSize && commandQueue.tail < kMaxCommandQueueSize &&
           commandQueue.count <= kMaxCommandQueueSize;
}

void sanitizeCommandQueueState() {
    if (commandQueueStateLooksValid()) {
        return;
    }
    LOG_PRINTLN("{\"type\":\"warning\",\"code\":\"command_queue_state_reset\",\"reason\":\"invalid_"
                "queue_metadata\"}");
    resetCommandQueueState();
}

// Push one complete serial line into the ring buffer.
void enqueueSerialCommand(const char *line) {
    if (!line) {
        return;
    }
    sanitizeCommandQueueState();
    if (commandQueue.count >= kMaxCommandQueueSize) {
        // Never discard an arbitrary earlier line: that can turn a framed
        // SET_ALL transaction into a corrupted partial payload. Reject the
        // new line and let the assembler time out/abort as one transaction.
        LOG_PRINTLN("{\"type\":\"error\",\"code\":\"command_queue_overflow\","
                    "\"message\":\"incoming command rejected; queued frame preserved\"}");
        return;
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
    sanitizeCommandQueueState();
    if (discardingOverlongLine) {
        if (received == '\n') {
            discardingOverlongLine = false;
        }
        return;
    }
    if (received == '\n') {
        serialBuffer[serialBufferIndex] = '\0';
        enqueueSerialCommand(serialBuffer);
        serialBufferIndex = 0;
    } else if (serialBufferIndex >= SERIAL_BUFFER_SIZE - 1) {
        // A truncated line is not a command.  Discard until its delimiter so
        // the tail cannot be interpreted as unrelated protocol commands.
        serialBufferIndex = 0;
        discardingOverlongLine = true;
        LOG_PRINTLN("Error: Command too long");
    } else if (received != '\r') {
        serialBuffer[serialBufferIndex++] = received;
    }
}
} // namespace

void initializeCommandQueue() { resetCommandQueueState(); }

// Pop the next queued command into the caller-provided buffer.
bool dequeueSerialCommand(char *outBuffer, size_t outBufferSize) {
    sanitizeCommandQueueState();
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
    constexpr uint16_t kMaxSerialBytesPerPass = 256;
    uint16_t consumed = 0;
    while (consumed < kMaxSerialBytesPerPass && Serial.available()) {
        ingestSerialByte(static_cast<char>(Serial.read()));
        ++consumed;
    }
}

#if defined(UNIT_TEST)
// Reset queue storage so tests start from a blank serial-command state.
void testOnly_resetCommandQueue() { resetCommandQueueState(); }

// Inject a full command line directly into the queue for tests.
void testOnly_enqueueSerialCommand(const char *line) { enqueueSerialCommand(line); }

// Feed one raw serial byte into the queue parser for tests.
void testOnly_ingestSerialByte(char received) { ingestSerialByte(received); }

void testOnly_corruptCommandQueueState(size_t head, size_t tail, size_t count) {
    commandQueue.head = head;
    commandQueue.tail = tail;
    commandQueue.count = count;
}
#endif
