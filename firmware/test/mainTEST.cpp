#include "MockArduino.h"
#include "MockLEDManager.h"
#include "ConfigManager.h"
#include "MIDIHandler.h"
#include "Utility.h"
#include "PotentiometerManager.h"
#include "ButtonManager.h"
#include "EnvelopeFollower.h"
#include "DisplayManager.h"

// Constants for Simulation
#define NUM_LEDS 42
#define NUM_BUTTONS 6
#define SERIAL_BUFFER_SIZE 128

char serialBuffer[SERIAL_BUFFER_SIZE];
uint8_t serialBufferIndex = 0;

// Pin assignments
const uint8_t primaryMuxPins[] = {7, 8, 9};
const uint8_t secondaryMuxPins[] = {10, 11, 12};
const uint8_t analogPin = 22;

// Global Objects
MockLEDManager ledManager(NUM_LEDS);
ConfigManager configManager(sizeof(uint8_t) * NUM_POTS);
MIDIHandler midiHandler;
std::vector<EnvelopeFollower> envelopeFollowers = {
    EnvelopeFollower(0, nullptr),
    EnvelopeFollower(1, nullptr),
    EnvelopeFollower(2, nullptr),
    EnvelopeFollower(3, nullptr)
};
MockDisplayManager displayManager(0x3C, 16, 2);

// PotentiometerManager for testing
PotentiometerManager potentiometerManager(primaryMuxPins, secondaryMuxPins, analogPin);

// Mock Testing Utility
void mockProcessSerial() {
    while (Serial.available()) {
        char received = Serial.read();

        // Handle end of command
        if (received == '\n' || serialBufferIndex >= SERIAL_BUFFER_SIZE - 1) {
            serialBuffer[serialBufferIndex] = '\0';
            serialBufferIndex = 0;

            // Process bulk update
            Utility::processBulkUpdate(serialBuffer, NUM_POTS);
        } else {
            serialBuffer[serialBufferIndex++] = received;
        }
    }
}

// Mock Test Setup
void setup() {
    Serial.begin(31250);
    ledManager.begin();
    displayManager.begin();

    // Initialize testing environment
    Serial.println("Mock Test Setup Complete");
    for (int i = 0; i < NUM_POTS; i++) {
        potentiometerManager.setChannel(i, 1);
        potentiometerManager.setCCNumber(i, i);
    }
    displayManager.showText("Testing...");
}

// Mock Test Loop
void loop() {
    static unsigned long lastMillis = 0;

    unsigned long currentMillis = millis();
    if (currentMillis - lastMillis > 1000) {
        Serial.println("Simulating hardware tick...");
        ledManager.show();
        lastMillis = currentMillis;
    }

    // Simulate a serial command
    static bool sentCommand = false;
    if (!sentCommand) {
        Serial.println("Simulating Serial Command: SET_ALL 12,1;34,2;");
        strcpy(serialBuffer, "SET_ALL 12,1;34,2;\n");
        mockProcessSerial();
        sentCommand = true;
    }
}

// Entry Point for Simulation
int main() {
    setup();
    while (true) {
        loop();
    }
    return 0;
}
