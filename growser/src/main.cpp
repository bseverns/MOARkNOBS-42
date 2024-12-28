#include <Arduino.h>
#include "MIDIHandler.h"
#include "LEDManager.h"
#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "PotentiometerManager.h"
#include "name.c"
#include <TimerOne.h>
#include <queue>
#include <map> // For tracking pot-to-envelope associations

// Constants
#define LED_PIN 6
#define NUM_LEDS 42
#define NUM_BUTTONS 6
#define LCD_I2C_ADDRESS 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define SERIAL_BUFFER_SIZE 128
#define MIDI_TASK_INTERVAL 1      // 1ms for MIDI processing
#define SERIAL_TASK_INTERVAL 10   // 10ms for Serial processing
#define LED_TASK_INTERVAL 50      // 50ms for LED updates
#define ENVELOPE_TASK_INTERVAL 5  // 5ms for Envelope processing
uint8_t midiBeatPosition = 0;
char serialBuffer[SERIAL_BUFFER_SIZE];
uint8_t serialBufferIndex = 0;

// Pin assignments for primary and secondary mux layers
const uint8_t primaryMuxPins[] = {7, 8, 9};
const uint8_t secondaryMuxPins[] = {10, 11, 12};
const uint8_t analogPin = 22; //mux reader

// Global objects
MIDIHandler midiHandler;
ConfigManager configManager(sizeof(uint8_t) * NUM_POTS);
LEDManager ledManager(LED_PIN, NUM_LEDS);
DisplayManager displayManager(LCD_I2C_ADDRESS);

// Declare PotentiometerManager before ButtonManager
PotentiometerManager potentiometerManager(primaryMuxPins, secondaryMuxPins, analogPin);
ButtonManager buttonManager(primaryMuxPins, secondaryMuxPins, analogPin, &potentiometerManager);

// Envelope followers - assign to analog inputs
std::vector<EnvelopeFollower> envelopeFollowers = {
    EnvelopeFollower(A0, &potentiometerManager),
    EnvelopeFollower(A1, &potentiometerManager),
    EnvelopeFollower(A2, &potentiometerManager),
    EnvelopeFollower(A3, &potentiometerManager)
};

std::map<int, int> potToEnvelopeMap; // Map pot index to envelope index
std::queue<String> commandQueue; // Queue to store incoming commands

// Hardware states
uint8_t potChannels[NUM_POTS];
uint8_t activePot = 0xFF;
uint8_t activeChannel = 1;
bool envelopeFollowMode = false;

// Timers for processing
unsigned long lastMIDIProcess = 0;
unsigned long lastSerialProcess = 0;
unsigned long lastLEDUpdate = 0;
unsigned long lastEnvelopeProcess = 0;

// ButtonManagerContext
ButtonManagerContext buttonContext = {
    potChannels,
    activePot,
    activeChannel,
    envelopeFollowMode,
    configManager,
    ledManager,
    displayManager,
    envelopeFollowers,
    potToEnvelopeMap
};

void processMIDI() {
    midiHandler.processIncomingMIDI();

    if (midiHandler.isClockTick()) {
        midiBeatPosition = (midiBeatPosition + 1) % 8;

        // Perform clock-tied updates
        Utility::updateVisuals(
            midiBeatPosition,
            envelopeFollowers,
            envelopeFollowMode ? "EF ON" : "EF OFF",
            activePot,
            activeChannel,
            ledManager,
            displayManager
        );

        midiHandler.clearClockTick(); // Reset the clock signal flag
    }
}

void processSerial() {
    while (Serial.available()) {
        char received = Serial.read();

        // End of command or buffer overflow
        if (received == '\n' || serialBufferIndex >= SERIAL_BUFFER_SIZE - 1) {
            serialBuffer[serialBufferIndex] = '\0'; // Null-terminate the command
            commandQueue.push(String(serialBuffer)); // Add to queue
            serialBufferIndex = 0; // Reset buffer index
        } else {
            serialBuffer[serialBufferIndex++] = received;
        }

        // Handle overflow
        if (serialBufferIndex >= SERIAL_BUFFER_SIZE - 1) {
            Serial.println("Error: Command too long");
            serialBufferIndex = 0; // Reset buffer index
        }
    }

    // Process queued commands
    while (!commandQueue.empty()) {
        String command = commandQueue.front(); // Get the front command
        commandQueue.pop(); // Remove it from the queue

        // Process the command (example: Utility::processBulkUpdate)
        Utility::processBulkUpdate(command, NUM_POTS);
    }
}

void processEnvelopes() {
    for (const auto& [potIndex, envelopeIndex] : potToEnvelopeMap) {
        if (envelopeIndex < static_cast<int>(envelopeFollowers.size())) {
            EnvelopeFollower* envelope = &envelopeFollowers[envelopeIndex];

            if (envelope->getActiveState()) { // Process only active envelopes
                envelope->update(); // Update envelope values

                uint8_t ccValue = potentiometerManager.getCCNumber(potIndex);
                envelope->applyToCC(potIndex, ccValue); // Modulate CC value

                if (ccValue != potentiometerManager.getLastValue(potIndex)) { // Avoid redundant MIDI messages
                    midiHandler.sendControlChange(
                        potentiometerManager.getCCNumber(potIndex),
                        ccValue,
                        potentiometerManager.getChannel(potIndex)
                    );

                    ledManager.setPotValue(potIndex, ccValue); // Update corresponding LED
                }
            }
        }
    }
}

void monitorSystemLoad() {
    static unsigned long lastMonitorTime = 0;
    static unsigned long taskCounter = 0;

    taskCounter++;
    if (millis() - lastMonitorTime >= 1000) { // Log every second
        Serial.printf("Tasks per second: %lu\n", taskCounter);
        taskCounter = 0;
        lastMonitorTime = millis();
    }
}

void setup() {
    Serial.begin(31250);
    midiHandler.begin();
    ledManager.begin();
    displayManager.begin();
    displayManager.showText("Initializing...", true);
    potentiometerManager.loadFromEEPROM();
    Timer1.initialize(1000); // 1ms interrupt
    Timer1.attachInterrupt(processMIDI);

    // Initialize the pot-to-envelope map
    potToEnvelopeMap[0] = 0; // Pot 0 controls Envelope 0
    potToEnvelopeMap[1] = 1; // Pot 1 controls Envelope 1
    potToEnvelopeMap[2] = 2; // Pot 2 controls Envelope 2
    potToEnvelopeMap[3] = 3; // Pot 3 controls Envelope 3

    for (auto& envelope : envelopeFollowers) {
        envelope.toggleActive(true); // Ensure all envelopes are activated
    }
    // Check for EEPROM load errors (e.g., invalid data)
    potentiometerManager.loadFromEEPROM();
        for (int i = 0; i < NUM_POTS; i++) {
        if (potentiometerManager.getChannel(i) == 0) {
            potentiometerManager.setChannel(i, 1); // Default to channel 1
        }
        if (potentiometerManager.getCCNumber(i) > 127) {
            potentiometerManager.setCCNumber(i, i % 128); // Limit CC to valid range
        }
    }
    if (!configManager.loadConfig(potChannels)) {
        Serial.println("EEPROM data corrupted, resetting to defaults.");
        potentiometerManager.resetEEPROM();
    }
    buttonManager.initButtons();
    delay(1000);
    displayManager.clear();
    displayManager.showText("BENZ", true);
}

void loop() {
    unsigned long currentMillis = millis();

    // Process MIDI every 1ms
    if (currentMillis - lastMIDIProcess >= MIDI_TASK_INTERVAL) {
        processMIDI();
        lastMIDIProcess = currentMillis;
    }

    // Process Serial commands every 10ms
    if (currentMillis - lastSerialProcess >= SERIAL_TASK_INTERVAL) {
        processSerial();
        lastSerialProcess = currentMillis;
    }

    // Update LEDs every 50ms
    if (currentMillis - lastLEDUpdate >= LED_TASK_INTERVAL) {
        ledManager.update();
        lastLEDUpdate = currentMillis;
    }

    // Process Envelopes every 5ms
    if (currentMillis - lastEnvelopeProcess >= ENVELOPE_TASK_INTERVAL) {
        processEnvelopes();
        lastEnvelopeProcess = currentMillis;
    }

    // Process buttons and potentiometers (non-blocking)
    buttonManager.processButtons(buttonContext);
    potentiometerManager.processPots(ledManager, envelopeFollowers);
    monitorSystemLoad(); // Track and log task execution
}