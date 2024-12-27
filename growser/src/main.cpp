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
#include <map> // For tracking pot-to-envelope associations

// Constants
#define LED_PIN 6
#define NUM_LEDS 42
#define NUM_BUTTONS 6
#define LCD_I2C_ADDRESS 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define SERIAL_BUFFER_SIZE 128
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

// Pot-to-envelope mapping
std::map<int, int> potToEnvelopeMap; // Map pot index to envelope index

// Hardware states
uint8_t potChannels[NUM_POTS];
uint8_t activePot = 0xFF;
uint8_t activeChannel = 1;
bool envelopeFollowMode = false;

//timers for processing
unsigned long lastMIDIProcess = 0;
unsigned long lastSerialProcess = 0;

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

        // Handle end of command
        if (received == '\n' || serialBufferIndex >= SERIAL_BUFFER_SIZE - 1) {
            serialBuffer[serialBufferIndex] = '\0'; // Null-terminate string
            serialBufferIndex = 0;

            // Process bulk update
            Utility::processBulkUpdate(serialBuffer, NUM_POTS);
        } else {
            serialBuffer[serialBufferIndex++] = received;
        }
    if (serialBufferIndex >= SERIAL_BUFFER_SIZE - 1) {
    Serial.println("Error: Command too long");
    serialBufferIndex = 0;
        }

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

    // Process MIDI every 1 ms
    if (currentMillis - lastMIDIProcess >= 1) {
        processMIDI();
        lastMIDIProcess = currentMillis;
    }

    // Process Serial commands every 10 ms
    if (currentMillis - lastSerialProcess >= 10) {
        processSerial();
        lastSerialProcess = currentMillis;
    }

    // Process buttons
    buttonManager.processButtons(buttonContext);

    // Process potentiometers and envelopes
    potentiometerManager.processPots(ledManager, envelopeFollowers);

    // Process envelope followers
    for (const auto& [potIndex, envelopeIndex] : potToEnvelopeMap) {
        if (envelopeIndex < static_cast<int>(envelopeFollowers.size())) {
            EnvelopeFollower* envelope = &envelopeFollowers[envelopeIndex];
            if (envelope->getActiveState()) {
                uint8_t ccValue = potentiometerManager.getCCNumber(potIndex);
                envelope->applyToCC(potIndex, ccValue); // Modulate only if necessary
                if (ccValue != potentiometerManager.getLastValue(potIndex)) { // Avoid redundant MIDI messages
                    midiHandler.sendControlChange(
                        potentiometerManager.getCCNumber(potIndex),
                        ccValue,
                        potentiometerManager.getChannel(potIndex)
                    );
                    ledManager.setPotValue(potIndex, ccValue);
                }
            }
        }
    }
}