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
#include "BiquadFilter.h"
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
DisplayManager displayManager(SSD1306_I2C_ADDRESS, 128, 64); // 128x64 for SSD1306
ConfigManager configManager(NUM_POTS, NUM_BUTTONS);
BiquadFilter filter;

// Declare PotentiometerManager before ButtonManager
const uint8_t controlPins[NUM_CONTROL_BUTTONS] = {2, 3, 4, 5, 6, 13}; // Add actual GPIO pins
PotentiometerManager potentiometerManager(primaryMuxPins, secondaryMuxPins, analogPin);
ButtonManager buttonManager(primaryMuxPins, secondaryMuxPins, analogPin, controlPins, &potentiometerManager);

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
unsigned long lastDisplayUpdate = 0;

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
        displayManager.updateDisplay(
            midiBeatPosition,
            std::vector<uint8_t>(), // Pass envelope levels if applicable
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

        if (command.startsWith("SET_POT")) {
            // Parse "SET_POT" command
            int firstComma = command.indexOf(',');
            int lastComma = command.lastIndexOf(',');

            if (firstComma == -1 || lastComma == -1 || firstComma == lastComma) {
                Serial.println("Error: Malformed SET_POT command");
                continue; // Skip invalid command
            }

            int potIndex = command.substring(8, firstComma).toInt();
            int channel = command.substring(firstComma + 1, lastComma).toInt();
            int ccNumber = command.substring(lastComma + 1).toInt();

            if (potIndex >= 0 && potIndex < NUM_POTS && channel >= 1 && channel <= 16 && ccNumber >= 0 && ccNumber <= 127) {
                configManager.setPotChannel(potIndex, channel);
                configManager.setPotCCNumber(potIndex, ccNumber);
                configManager.saveConfiguration();
                Serial.println("Pot configuration updated!");
            } else {
                Serial.println("Error: Invalid values for SET_POT");
            }

        } else if (command.startsWith("SET_ALL")) {
            Utility::processBulkUpdate(command, configManager.getNumPots());

        } else if (command.startsWith("GET_ALL")) {
            // Send all pot settings
            Serial.print("POTS:");
            for (int i = 0; i < NUM_POTS; i++) {
                int envelopeValue = (potToEnvelopeMap.count(i)) ? potToEnvelopeMap[i] : -1;
                Serial.print(configManager.getPotCCNumber(i));
                Serial.print(",");
                Serial.print(configManager.getPotChannel(i));
                Serial.print(",");
                Serial.print(envelopeValue);
                Serial.print(";");
            }

            // Send LED settings
            CRGB ledColor = ledManager.getColor();
            Serial.print(" LED:");
            Serial.print(ledManager.getBrightness());
            Serial.print(",");
            Serial.print(ledColor.r);
            Serial.print(",");
            Serial.print(ledColor.g);
            Serial.print(",");
            Serial.println(ledColor.b); // `println` ensures newline
        } 
        else {
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
    configManager.begin(potChannels);
    configManager.loadEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
    midiHandler.begin();

    ledManager.begin();
    uint8_t ledBrightness;
    CRGB ledColor;
    configManager.loadLEDSettings(ledBrightness, ledColor);
    ledManager.setBrightness(ledBrightness);
    ledManager.setColor(ledColor);

    displayManager.begin();
    displayManager.showText("Initializing...");
    potentiometerManager.loadFromEEPROM();
    Timer1.initialize(1000); // 1ms interrupt
    Timer1.attachInterrupt(processMIDI);
    filter.configure(BiquadFilter::LOWPASS, 1000, 44100); // Configure as 1kHz low-pass filter

    for (auto& envelope : envelopeFollowers) {
        envelope.toggleActive(true);
    }

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
        potentiometerManager.resetEEPROM();
    }

    buttonManager.initButtons();
    delay(1000);
    displayManager.clear();
    displayManager.showText("MOAR");

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

    // Process Display every 100ms
    if (millis() - lastDisplayUpdate > 100) {
        lastDisplayUpdate = millis();
        displayManager.updateFromContext(buttonContext);
    }

    // Process buttons and potentiometers
    buttonManager.processButtons(buttonContext);
    potentiometerManager.processPots(ledManager, envelopeFollowers);
    monitorSystemLoad(); // Track and log task execution
}