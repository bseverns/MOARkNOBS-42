#include <Arduino.h>
#include "MIDIHandler.h"
#include "LEDManager.h"
#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "PotentiometerManager.h"
#include "name.c"
#include "Globals.h"
#include <TimerOne.h>
#include <queue>
#include <map> // For tracking pot-to-envelope associations

uint8_t midiBeatPosition = 0;
char serialBuffer[SERIAL_BUFFER_SIZE];
uint8_t serialBufferIndex = 0;

// Global objects
std::vector<uint8_t> potChannels;
std::map<int, int> potToEnvelopeMap; // Map pot index to envelope index
std::queue<String> commandQueue; // Queue to store incoming commands
MIDIHandler midiHandler;
LEDManager ledManager(LED_PIN, NUM_LEDS);
DisplayManager displayManager(OLED_I2C_ADDRESS, 128, 64); // 128x64 for SSD1306
ConfigManager configManager(NUM_POTS, NUM_BUTTONS);

// Declare PotentiometerManager before ButtonManager
PotentiometerManager potentiometerManager(primaryMuxPins, secondaryMuxPins, analogPin);
ButtonManager buttonManager(primaryMuxPins, secondaryMuxPins, analogPin, &potentiometerManager);

// Envelope followers - assign to analog inputs
std::vector<EnvelopeFollower> envelopeFollowers = {
    EnvelopeFollower(A0, &potentiometerManager),
    EnvelopeFollower(A1, &potentiometerManager),
    EnvelopeFollower(A2, &potentiometerManager),
    EnvelopeFollower(A3, &potentiometerManager),
    EnvelopeFollower(A6, &potentiometerManager),
    EnvelopeFollower(A7, &potentiometerManager),
};

// Hardware states
uint8_t activePot = 0xFF;
uint8_t activeChannel = 1;
bool envelopeFollowMode = false;
const char* envelopeMode = "LINEAR"; // Default envelope mode

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
        Utility::updateDisplay(
            displayManager.getDisplay(),       // Access the display object
            midiBeatPosition,
            envelopeFollowers,                 // Pass the vector of EnvelopeFollowers
            envelopeFollowMode ? "EF ON" : "EF OFF",
            activePot,
            activeChannel,
            envelopeMode
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
            commandQueue.push(String(serialBuffer)); // Add the full command to the queue
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

        // Handle specific commands
        if (command.startsWith("SET_POT")) {
            // Parse the "SET_POT" command
            int potIndex = command.substring(8, command.indexOf(',')).toInt();
            int channel = command.substring(command.indexOf(',') + 1, command.lastIndexOf(',')).toInt();
            int ccNumber = command.substring(command.lastIndexOf(',') + 1).toInt();

            configManager.setPotChannel(potIndex, channel);
            configManager.setPotCCNumber(potIndex, ccNumber);
            configManager.saveConfiguration();

            Serial.println("Pot configuration updated!");
        } else if (command.startsWith("SET_ALL")) {
            Utility::processBulkUpdate(command, configManager.getNumPots());

        } else if (command.startsWith("GET_ALL")) {
            // Send all pot settings
            Serial.print("POTS:");
            for (int i = 0; i < NUM_POTS; i++) {
                Serial.printf("%d,%d,%d;", configManager.getPotCCNumber(i), configManager.getPotChannel(i), potToEnvelopeMap[i]);
            }

            // Send LED settings
            Serial.printf(" LED:%d,%d,%d,%d\n", ledManager.getBrightness(), ledManager.getColor().r, ledManager.getColor().g, ledManager.getColor().b);
        }
    } else {
            Serial.println("Unknown command: " + command);
        }
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
    // Load stored settings from EEPROM
    configManager.begin(potChannels);
    configManager.loadEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
    midiHandler.begin();

    ledManager.begin();
    uint8_t ledBrightness;
    CRGB ledColor;
    configManager.loadLEDSettings(ledBrightness, ledColor);
    // Apply stored settings
    ledManager.setBrightness(ledBrightness);
    ledManager.setColor(ledColor);

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
    potToEnvelopeMap[4] = 4;
    potToEnvelopeMap[5] = 5;

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
    if (!configManager.loadConfiguration(potChannels)) {
    Serial.println("EEPROM data corrupted, resetting to defaults.");
    //configManager.resetConfiguration(potChannels);
    potentiometerManager.resetEEPROM();
    }
    buttonManager.initButtons();
    delay(1000);
    displayManager.clear();
    displayManager.showText("MOAR", true);

    // Print loaded configuration for debugging
    Serial.println("Verifying loaded pot channels:");
    for (int i = 0; i < NUM_POTS; i++) {
        Serial.print("Pot ");
        Serial.print(i);
        Serial.print(": CC=");
        Serial.println(potChannels[i]);
    }
    Serial.println("Setup complete!");
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