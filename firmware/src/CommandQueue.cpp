#include "CommandQueue.h"

#include <Arduino.h>
#include <cstring>

#include "Globals.h"
#include "Log.h"

namespace {
constexpr size_t kMaxCommandQueueSize = 64;

struct CommandQueueStorage {
    char entries[kMaxCommandQueueSize][SERIAL_BUFFER_SIZE] = {{0}};
    size_t head = 0;
    size_t tail = 0;
    size_t count = 0;
};

CommandQueueStorage commandQueue;

void dropOldestCommand() {
    if (commandQueue.count == 0) {
        return;
    }
    commandQueue.head = (commandQueue.head + 1) % kMaxCommandQueueSize;
    --commandQueue.count;
}

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

    std::strncpy(commandQueue.entries[commandQueue.tail], line, SERIAL_BUFFER_SIZE - 1);
    commandQueue.entries[commandQueue.tail][SERIAL_BUFFER_SIZE - 1] = '\0';
    commandQueue.tail = (commandQueue.tail + 1) % kMaxCommandQueueSize;
    ++commandQueue.count;
}
} // namespace

bool dequeueSerialCommand(char *outBuffer, size_t outBufferSize) {
    if (!outBuffer || outBufferSize == 0 || commandQueue.count == 0) {
        return false;
    }

    std::strncpy(outBuffer, commandQueue.entries[commandQueue.head], outBufferSize - 1);
    outBuffer[outBufferSize - 1] = '\0';
    commandQueue.head = (commandQueue.head + 1) % kMaxCommandQueueSize;
    --commandQueue.count;
    return true;
}

void pollSerialInput() {
    while (Serial.available()) {
        char received = Serial.read();

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
}
