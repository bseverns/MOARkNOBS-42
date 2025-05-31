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
TaskScheduler scheduler;

//tempo
unsigned long lastClockTime = 0;
float g_tappedBPM = 120.0f; // Default to 120 BPM

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
int NORMAL_DISPLAY_TIME = 30000;
int SHORT_DISPLAY_TIME = 10000;

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

void processInternalClock() {
    // For 24 PPQN (like MIDI clock), you multiply BPM * 24 = pulses per minute
    // So each pulse is 60000 / (BPM*24) milliseconds
    static unsigned long lastTick = 0;
    static float msPerTick = 60000.0f / (g_tappedBPM * 24.0f);

    unsigned long now = millis();
    msPerTick = 60000.0f / (g_tappedBPM * 24.0f); // recalc in case BPM changed

    if (now - lastTick >= msPerTick) {
        lastTick += msPerTick; // schedule the next tick

        // do the same code you do on external MIDI Clock:
        midiBeatPosition = (midiBeatPosition + 1) % 8;

        // Optionally call display update or other “beat-based” logic:
        displayManager.updateDisplay(
            midiBeatPosition,
            std::vector<uint8_t>(), // envelope levels if desired
            envelopeFollowMode ? "EF ON" : "EF OFF",
            activePot,
            activeChannel,
            envelopeMode
        );
    }
}

void processMIDI() {
    midiHandler.processIncomingMIDI();

    if (midiHandler.isClockTick()) {
        // Record the time we received an external clock
        lastClockTime = millis();

        // Advance beat
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

        // Clear the clock flag
        midiHandler.clearClockTick();
    }
}

void processSerial() {
    while (Serial.available()) {
        char received = Serial.read();
String command = String(serialBuffer);
      command.trim();
        // End of command or buffer overflow
        if (received == '\n' || serialBufferIndex >= SERIAL_BUFFER_SIZE - 1) {
            serialBuffer[serialBufferIndex] = '\0'; // Null-terminate the command
            commandQueue.push(String(serialBuffer)); // Add the full command to the queue
            serialBufferIndex = 0; // Reset buffer index
        } else if (command == "GET_SCHEMA") {
            Serial.println(ConfigManager::makeSchema());
        }else {
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
            Serial.println(ledColor.b);
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

void updateFilterTuning(ButtonManagerContext& context) {
    // 1. Read raw ADC from freq pot
    int rawFreq = analogRead(FILTER_FREQ_POT_PIN);  // 0..1023
    // 2. Read raw ADC from Q pot
    int rawQ = analogRead(FILTER_RES_POT_PIN);        // 0..1023

    // 3. Map rawFreq => 20..5000 Hz (pick a range that feels good)
    float freq = map(rawFreq, 0, 1023, 20, 5000);

    // 4. Map rawQ => 0.5..4.0 (a typical resonance range)
    //    - For instance, map from 0..1023 => 50..400, then /100
    float q = map(rawQ, 0, 1023, 50, 400) / 100.0f; // => 0.50..4.00

    // 5. Which EF are we tuning?
    //    We'll tune the EF assigned to the “activePot” in the context
    auto it = context.potToEnvelopeMap.find(context.activePot);
    if (it == context.potToEnvelopeMap.end()) {
        // If no EF assigned to active pot, do nothing
        return;
    }
    int efIndex = it->second; // e.g. 0..5 if you have 6 EFs total

    // 6. Actually set that EF’s filter freq/Q
    //    BUT remember, it only affects EFs whose filterType is
    //    LOWPASS, HIGHPASS, or BANDPASS.
    context.envelopes[efIndex].configureFilter(freq, q);
    EEPROM.put(EEPROM_FILTER_FREQ, freq);
    EEPROM.put(EEPROM_FILTER_Q, q);

    // Optionally display or debug-print
    // Serial.printf("EF %d => freq=%.1f Q=%.2f\n", efIndex, freq, q);
}

void setup() {
    Serial.begin(31250);
    configManager.begin(potChannels);
    configManager.loadEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
    midiHandler.begin();
    midiHandler.setDisplayManager(&displayManager);

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
    pinMode(FILTER_FREQ_POT_PIN, INPUT);
    pinMode(FILTER_RES_POT_PIN, INPUT);
    filter.configure(BiquadFilter::LOWPASS, 1000, 44100);

    for (auto& envelope : envelopeFollowers) {
        envelope.toggleActive(true);
    }

    float savedFreq, savedQ;
    EEPROM.get(EEPROM_FILTER_FREQ, savedFreq);
    EEPROM.get(EEPROM_FILTER_Q, savedQ);
    savedFreq = constrain(savedFreq, 20.0f, 5000.0f);
    savedQ    = constrain(savedQ, 0.5f, 4.0f);
    for (auto& ef : envelopeFollowers) {
        ef.configureFilter(savedFreq, savedQ);
    }

    for (int i = 0; i < NUM_POTS; i++) {
        if (potentiometerManager.getChannel(i) == 0) {
            potentiometerManager.setChannel(i, 1);
        }
        if (potentiometerManager.getCCNumber(i) > 127) {
            potentiometerManager.setCCNumber(i, i % 128);
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

    Serial.println("Verifying loaded pot channels:");
    for (int i = 0; i < NUM_POTS; i++) {
        Serial.print("Pot ");
        Serial.print(i);
        Serial.print(": CC=");
        Serial.println(potChannels[i]);
    }
    Serial.println("Setup complete!");

    // --- Schedule repeating tasks ---
    // High-priority tasks (1ms interval)
      Utility::schedulerHigh.addTask([] { processMIDI(); }, MIDI_TASK_INTERVAL);
      Utility::schedulerHigh.addTask([] {
        if (millis() - lastClockTime > CLOCK_TIMEOUT_MS) {
          processInternalClock();
        }
      }, MIDI_TASK_INTERVAL);

      // Mid-priority tasks (~5-10ms intervals)
      Utility::schedulerMid.addTask([] { processSerial(); }, SERIAL_TASK_INTERVAL);
      Utility::schedulerMid.addTask([] { processEnvelopes(); }, ENVELOPE_TASK_INTERVAL);

      // Low-priority tasks (~30-100ms intervals)
      Utility::schedulerLow.addTask([] {
        ledManager.update();
        updateFilterTuning(buttonContext);
      }, LED_TASK_INTERVAL);

      Utility::schedulerLow.addTask([] {
        if (!displayManager.shouldRunScreensaver()) {
          displayManager.beginDraw();
          displayManager.updateFromContext(buttonContext);

          auto it = potToEnvelopeMap.find(activePot);
          if (it != potToEnvelopeMap.end()) {
            uint8_t lvl = envelopeFollowers[it->second].getEnvelopeLevel();
            displayManager.showEnvelopeLevel(lvl);
          }

          displayManager.highlightActivePot(activePot);
          displayManager.highlightActiveMode(envelopeMode);
          displayManager.endDraw();
        } else {
          displayManager.runIdleScreensaver();
        }
      }, 100);
}

void loop() {
   if (Serial.available()) {
    // read up to newline, drop the '\n'
    String command = Serial.readStringUntil('\n');
    command.trim();  // remove any '\r' or extra whitespace

    if (command == "GET_SCHEMA") {
      // call the method on your instance:
      Serial.println(configManager.makeSchema());
    }
    else if (command == "GET_ALL") {
      Serial.println(configManager.serializeAll());
    } else {
      // end of line reached
      serialBuffer[serialBufferIndex] = '\0';
      String command = String(serialBuffer);
      serialBufferIndex = 0;

      

      // clear buffer for next command
      memset(serialBuffer, 0, SERIAL_BUFFER_SIZE);
    }
  }

    Utility::schedulerHigh.update();
    Utility::schedulerMid.update();
    Utility::schedulerLow.update();
    buttonManager.processButtons(buttonContext);
    potentiometerManager.processPots(ledManager, envelopeFollowers);
    monitorSystemLoad();
}
