// Entry point for the MN42 firmware.
// Instantiates all managers and drives the scheduler loop.
// Coordinates interactions between machine sub-systems.

#include <Arduino.h>
#include "MIDIHandler.h"
#include "LEDManager.h"
#include "ConfigManager.h"
#include "EnvelopeFollower.h"
#include "DisplayManager.h"
#include "ButtonManager.h"
#include "PotentiometerManager.h"
#include "WebSerial.h"
#include "Utility.h"
#include "name.c"
#include "Globals.h"  // contains all pin definitions
#include "BiquadFilter.h"
#include "Arpeggiator.h"
#include <TimerOne.h>
#include <queue>
#include <map> // For tracking pot-to-envelope associations

struct HardwareConfigInitializer { HardwareConfigInitializer() { loadHardwareConfig(); } } _hwInit;

uint8_t midiBeatPosition = 0;
char serialBuffer[SERIAL_BUFFER_SIZE];
uint8_t serialBufferIndex = 0;
bool webSerialStreaming = false;            // True once the browser says HELLO

// Global objects
std::vector<uint8_t> potChannels;             // 42-slot table: each entry stores a slot's MIDI CC value
std::map<int, int> potToEnvelopeMap;          // Crosswalk from pot index to its envelope follower partner
std::queue<String> commandQueue;              // Serial command backlog waiting for mid-tier processing
MIDIHandler midiHandler;                      // Central MIDI traffic cop slinging bytes over USB + DIN
LEDManager ledManager(hwConfig);              // Whips the WS2812 strip into obedient patterns
DisplayManager displayManager(SSD1306_I2C_ADDRESS, 128, 64); // Bosses around the 128x64 OLED
ConfigManager configManager(NUM_POTS, NUM_BUTTONS); // Persists slot + button config to EEPROM
BiquadFilter filter;                          // Shared filter template for envelope follower shaping
TaskScheduler scheduler;                      // Legacy scheduler kept for posterity (most work lives in Utility)
Arpeggiator arpeggiator;                      // Keeps notes chugging along in time

// tempo
unsigned long lastClockTime = 0;              // ms timestamp of the most recent external MIDI clock

// Declare PotentiometerManager before ButtonManager
// Pin 6 is reserved for the LED strip
// Control buttons are direct-wired (not part of the mux matrix)
// and must not share the mux select pins.
const uint8_t controlPins[NUM_CONTROL_BUTTONS] = {12, 13, 14, 15, 24, 25}; // Direct-wired control buttons
PotentiometerManager potentiometerManager(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin); // Scans pot muxes & EEPROM slots
ButtonManager buttonManager(hwConfig, controlPins, &potentiometerManager); // Wrangles the button matrix

// Envelope followers – six ADC spies that turn audio/CV into modulation
std::vector<EnvelopeFollower> envelopeFollowers = {
    EnvelopeFollower(A0, &potentiometerManager),
    EnvelopeFollower(A1, &potentiometerManager),
    EnvelopeFollower(A2, &potentiometerManager),
    EnvelopeFollower(A3, &potentiometerManager),
    EnvelopeFollower(A6, &potentiometerManager),
    EnvelopeFollower(A7, &potentiometerManager),
};

// Hardware/UI state trackers
uint8_t activePot = 0xFF;                 // Current slot index; 0xFF means "none selected"
uint8_t activeChannel = 1;                // MIDI channel currently under the spotlight
bool envelopeFollowMode = false;          // True when EFs are allowed to hijack a slot
const char* envelopeMode = "LINEAR";      // Default envelope mode
int NORMAL_DISPLAY_TIME = 30000;          // ms duration for full-size messages
int SHORT_DISPLAY_TIME = 10000;           // ms duration for terse status flashes

// Timers for processing
unsigned long lastMIDIProcess = 0;
unsigned long lastSerialProcess = 0;
unsigned long lastLEDUpdate = 0;
unsigned long lastEnvelopeProcess = 0;
unsigned long lastDisplayUpdate = 0;

// ButtonManagerContext: glue struct passed around to avoid global rummaging
ButtonManagerContext buttonContext = {
    potChannels,
    activePot,
    activeChannel,
    envelopeFollowMode,
    envelopeMode,
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

        if (received == '\n' || serialBufferIndex >= SERIAL_BUFFER_SIZE - 1) {
            serialBuffer[serialBufferIndex] = '\0';
            if (serialBufferIndex >= SERIAL_BUFFER_SIZE - 1) {
                Serial.println("Error: Command too long");
            }
            commandQueue.push(String(serialBuffer));
            serialBufferIndex = 0;
        } else if (received != '\r') {
            serialBuffer[serialBufferIndex++] = received;
        }
    }

    // Process queued commands
    while (!commandQueue.empty()) {
        String command = commandQueue.front(); // Get the front command
        commandQueue.pop(); // Remove it from the queue

        command.trim();

        if (command == "HELLO") {
            webSerialStreaming = true;
            Serial.println("{\"hello\":\"mn42\"}");

        } else if (command == "GET_SCHEMA") {
            Serial.println(ConfigManager::makeSchema());

        } else if (command.startsWith("SET_POT")) {
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
                ledManager.setEnvelopeLevel(envelopeIndex, envelope->getEnvelopeLevel());

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

    // Reflect the current pot's MIDI-scaled value on the indicators
    uint8_t potMidiValue = Utility::mapToMidiValue(
        potentiometerManager.getLastValue(buttonContext.activePot));
    for (uint8_t i = 0; i < POT_LED_COUNT; ++i) {
        ledManager.setPotIndicator(i, potMidiValue);
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
    int rawFreq = buttonManager.getControlPotValue(1);  // MUXC channel 13
    // 2. Read raw ADC from Q pot
    int rawQ = buttonManager.getControlPotValue(2);      // MUXC channel 14

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
    // Provide labels for the on-screen filter tuning display
    displayManager.showFilterTuning("Freq", freq, "Q", q);

    // Optionally display or debug-print
    // Serial.printf("EF %d => freq=%.1f Q=%.2f\n", efIndex, freq, q);
}

void updateArpTuning() {
    if (!arpeggiator.isActive()) return;

    int rawLen   = buttonManager.getControlPotValue(1);
    int rawShape = buttonManager.getControlPotValue(2);

    float lengthMs = map(rawLen, 0, 1023, 80, 800);
    int shapeIdx   = map(rawShape, 0, 1023, 0, 3);
    static const char* names[] = {"UP", "DOWN", "UPDN", "RAND"};
    Arpeggiator::Shape shapes[] = {Arpeggiator::UP, Arpeggiator::DOWN, Arpeggiator::UPDOWN, Arpeggiator::RANDOM};

    arpeggiator.setLength(lengthMs);
    arpeggiator.setShape(shapes[shapeIdx]);

    displayManager.showArpSettings(lengthMs, names[shapeIdx]);
}

void streamWebSerialState() {
    if (!webSerialStreaming) return;
    WebSerial::sendStateSnapshot(potentiometerManager, envelopeFollowers);
}

void setup() {
    // — Serial & Config —
    Serial.begin(31250);

    // Configure status LED
    pinMode(hwConfig.statusLedPin, OUTPUT);
    digitalWrite(hwConfig.statusLedPin, LOW);

    // Measure VREF for baseline calibration
    pinMode(VREF_ADC_PIN, INPUT);
    g_vref = Utility::readVrefADC(VREF_ADC_PIN);

    // Load per-slot EEPROM into RAM, and pot→CC into potChannels[]
    configManager.begin(potChannels);
    configManager.loadMIDISlots(&configManager.getSlot(0), NUM_SLOTS);
    configManager.loadEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);

    // — MIDI Handler —
    midiHandler.begin();
    midiHandler.setDisplayManager(&displayManager);

    // — Pot → MIDI routing callback —
    potentiometerManager.setMidiCallback(
      [&](uint8_t /*ignored*/, uint8_t value, uint8_t rawValue, uint8_t potIdx){
        auto& slot = configManager.getSlots()[potIdx];
        if (!slot.active) return;

        switch (slot.type) {
          case MIDIMessageType::CC:
            midiHandler.sendControlChange(slot.data1, value, slot.midiChannel);
            break;

          case MIDIMessageType::Note: {
            uint8_t velo = (slot.efIndex < envelopeFollowers.size())
                           ? envelopeFollowers[slot.efIndex].getEnvelopeLevel()
                           : 125;
            midiHandler.sendNoteOn(slot.data1, velo, slot.midiChannel);
            // schedule Note-Off in 100 ms
            Utility::schedulerHigh.addTask([=](){
              midiHandler.sendNoteOff(slot.data1, 0, slot.midiChannel);
            }, 100);
            break;
          }

          case MIDIMessageType::PitchBend: {
            int16_t bend = map(rawValue, 0, 1023, -8192, 8191);
            midiHandler.sendPitchBend(bend, slot.midiChannel);
            break;
          }

          case MIDIMessageType::ProgramChange:
            midiHandler.sendProgramChange(slot.data1, slot.midiChannel);
            break;

          case MIDIMessageType::Aftertouch: {
            uint8_t pres = Utility::mapToMidiValue(rawValue);
            midiHandler.sendAftertouch(pres, slot.midiChannel);
            break;
          }

          default:
            break;
        }
      }
    );

    // — LEDs & Display —
    ledManager.begin();
    uint8_t ledB;
    CRGB    ledC;
    configManager.loadLEDSettings(ledB, ledC);
    ledManager.setBrightness(ledB);
    ledManager.setColor(ledC);

    displayManager.begin();
    displayManager.showText("Initializing...");

    // — EEPROM & Mux init —
    potentiometerManager.loadFromEEPROM();

    // — Timer (1 ms base for MIDI & internal clock) —
    Timer1.initialize(1000);
    Timer1.attachInterrupt(processMIDI);

    // — Filter hardware —
    filter.configure(BiquadFilter::LOWPASS, 1000, 44100);

    // — Envelope followers —
    for (auto& ef : envelopeFollowers) {
        ef.toggleActive(true);
        ef.calibrateBaseline();
    }
    float sf, sq;
    EEPROM.get(EEPROM_FILTER_FREQ, sf);
    EEPROM.get(EEPROM_FILTER_Q,    sq);
    sf = constrain(sf, 20.0f, 5000.0f);
    sq = constrain(sq, 0.5f, 4.0f);
    for (auto& ef : envelopeFollowers) ef.configureFilter(sf, sq);

    // — Slot sanity check (channel & CC) —
    for (uint8_t i = 0; i < NUM_SLOTS; i++) {
      if (potentiometerManager.getChannel(i) == 0)
        potentiometerManager.setChannel(i, 1);
      if (potentiometerManager.getCCNumber(i) > 127)
        potentiometerManager.setCCNumber(i, i % 128);
    }

    // — Load or reset full config —
    if (!configManager.loadConfiguration(potChannels)) {
      Serial.println("EEPROM corrupted → resetting.");
      potentiometerManager.resetEEPROM();
    }

    // — Buttons & splash —
    buttonManager.initButtons();
    delay(1000);
    displayManager.clear();
    displayManager.showText("MOAR");
    ledManager.blinkStatusLED(2, 100);

    // — Debug dump —
    for (uint8_t i = 0; i < NUM_SLOTS; i++) {
      Serial.printf("Slot %u → CC %u\n", i, potChannels[i]);
    }
    Serial.println("Setup complete!");
    displayManager.runStartupAnimation();
    
    // — Scheduler tasks —
    // Three cooperative schedulers slice time so nothing blocks:
    // High-priority (1 ms):
    Utility::schedulerHigh.addTask(processMIDI,          hwConfig.midiTaskInterval);
    Utility::schedulerHigh.addTask([](){
      if (millis() - lastClockTime > CLOCK_TIMEOUT_MS)
        processInternalClock();
    }, hwConfig.midiTaskInterval);
    Utility::schedulerHigh.addTask([](){
      arpeggiator.update(midiHandler, configManager, potentiometerManager);
    }, hwConfig.midiTaskInterval);

    // Mid-priority (~5 ms):
    Utility::schedulerMid.addTask(processSerial,        hwConfig.serialTaskInterval);
    Utility::schedulerMid.addTask(processEnvelopes,     hwConfig.envelopeTaskInterval);

    // Low-priority (~50 ms):
    Utility::schedulerLow.addTask([](){
      ledManager.update();
      updateFilterTuning(buttonContext);
      updateArpTuning();
    }, hwConfig.ledTaskInterval);

    Utility::schedulerLow.addTask([](){
      if (!displayManager.shouldRunScreensaver()) {
        displayManager.beginDraw();
        displayManager.updateFromContext(buttonContext);
        auto it = potToEnvelopeMap.find(buttonContext.activePot);
        if (it != potToEnvelopeMap.end()) {
          displayManager.showEnvelopeLevel(
            envelopeFollowers[it->second].getEnvelopeLevel()
          );
        }
        displayManager.highlightActivePot(buttonContext.activePot);
        displayManager.highlightActiveMode(envelopeMode);
        displayManager.endDraw();
      } else {
        displayManager.runIdleScreensaver();
      }
    }, 100);

    // WebSerial telemetry every ~100 ms once the browser says hello
    Utility::schedulerLow.addTask(streamWebSerialState, 100, true);
}

/*
 * Main loop groove:
 * 1. Kick the schedulers in priority order so time-critical MIDI work happens first.
 *    - schedulerHigh → 1 ms tick: MIDI parsing, internal clock, arpeggiator.
 *    - schedulerMid  → 5–10 ms chores: serial command parsing and envelope tracking.
 *    - schedulerLow  → 50–100 ms eye candy: LEDs, filter tweaks, and display drawing.
 * 2. After the schedulers run, poll buttons and pots every spin for instant UI feel.
 * 3. Finish by checking system load so we know if we're pushing the MCU too hard.
 * Tasks never preempt each other; everyone plays nice and yields fast for the next riff.
 */
void loop() {
    Utility::schedulerHigh.update();
    Utility::schedulerMid.update();
    Utility::schedulerLow.update();
    buttonManager.processButtons(buttonContext);
    potentiometerManager.processPots(ledManager, envelopeFollowers);
    monitorSystemLoad();
}
