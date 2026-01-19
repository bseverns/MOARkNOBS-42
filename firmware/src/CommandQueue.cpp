#include "CommandQueue.h"

#include <Arduino.h>

#include "FirmwareState.h"
#include "Globals.h"
#include "Log.h"

// Holds fully received Serial lines so `processCommandQueue()` can parse them
// off the mid-tier task without blocking the ISR-bound serial handler.
std::queue<String> commandQueue;

void pollSerialInput() {
    while (Serial.available()) {
        char received = Serial.read();

        if (received == '\n' || serialBufferIndex >= SERIAL_BUFFER_SIZE - 1) {
            serialBuffer[serialBufferIndex] = '\0';
            if (serialBufferIndex >= SERIAL_BUFFER_SIZE - 1) {
                LOG_PRINTLN("Error: Command too long");
            }
            commandQueue.push(String(serialBuffer));
            serialBufferIndex = 0;
        } else if (received != '\r') {
            serialBuffer[serialBufferIndex++] = received;
        }
    }
}
