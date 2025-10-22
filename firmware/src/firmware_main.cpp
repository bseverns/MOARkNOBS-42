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
#include "Log.h"
#include "TimeUtils.h"
#include "name.c"
#include "Globals.h" // contains all pin definitions
#include "version.h"
#include "BiquadFilter.h"
#include "Arpeggiator.h"
#include "interop/SeedBoxLink.h"
#include <TimerOne.h>
#include <ArduinoJson.h>
#include <queue>
#include <map>
#include <imxrt.h>

// Sneaky static that kicks in before setup() even thinks about stretching.
// It pulls in pin maps and timing constants from Globals.h so the rest of this
// file can swagger with real values. If you need to rewrite the defaults,
// hunt down loadHardwareConfig() in firmware/src/Globals.cpp
// and make your mark.
struct HardwareConfigInitializer {
    HardwareConfigInitializer() { loadHardwareConfig(); }
} _hwInit;

uint8_t midiBeatPosition = 0; // 0-7 beat slot; bumps each MIDI clock tick then wraps on the 8th
char serialBuffer[SERIAL_BUFFER_SIZE]; // Holding pen where serial graffiti waits for judgement
uint8_t serialBufferIndex = 0;   // Cursor into serialBuffer; resets on newline or when it overflows
bool webSerialStreaming = false; // Goes true when the browser hollers HELLO and stays that way

// Global objects
std::vector<uint8_t> potChannels;    // 42-slot table: each entry stores a slot's MIDI channel
std::map<int, int> potToEnvelopeMap; // Crosswalk from pot index to its envelope follower partner
std::queue<String> commandQueue;     // Serial command backlog waiting for mid-tier processing
MIDIHandler midiHandler;             // Central MIDI traffic cop slinging bytes over USB + DIN
LEDManager ledManager(hwConfig);     // Whips the WS2812 strip into obedient patterns
DisplayManager displayManager(SSD1306_I2C_ADDRESS, 128, 64); // Bosses around the 128x64 OLED
ConfigManager configManager(NUM_POTS, NUM_BUTTONS); // Persists slot + button config to EEPROM
BiquadFilter filter;     // Shared filter template for envelope follower shaping
TaskScheduler scheduler; // Legacy scheduler kept for posterity (most work lives in Utility)
Arpeggiator arpeggiator; // Keeps notes chugging along in time

// Declare PotentiometerManager before ButtonManager
// Pin 6 is reserved for the LED strip
// Control buttons are direct-wired (not part of the mux matrix)
// and must not share the mux select pins.
const uint8_t controlPins[NUM_CONTROL_BUTTONS] = {12, 13, 14,
                                                  15, 24, 25}; // Direct-wired control buttons
PotentiometerManager potentiometerManager(primaryMuxPins, secondaryMuxPins,
                                          potMuxAnalogPin); // Scans pot muxes & EEPROM slots
ButtonManager buttonManager(hwConfig, controlPins,
                            &potentiometerManager); // Wrangles the button matrix

// Envelope followers – six ADC spies that turn audio/CV into modulation
std::vector<EnvelopeFollower> envelopeFollowers = {
    EnvelopeFollower(A0, &potentiometerManager, 0), EnvelopeFollower(A1, &potentiometerManager, 1),
    EnvelopeFollower(A2, &potentiometerManager, 2), EnvelopeFollower(A3, &potentiometerManager, 3),
    EnvelopeFollower(A6, &potentiometerManager, 4), EnvelopeFollower(A7, &potentiometerManager, 5),
};

// Hardware/UI state trackers
uint8_t activePot = 0xFF;            // Current slot index; 0xFF means "none selected"
uint8_t activeChannel = 1;           // MIDI channel currently under the spotlight
bool envelopeFollowMode = false;     // True when EFs are allowed to hijack a slot
const char *envelopeMode = "LINEAR"; // Default envelope mode
int NORMAL_DISPLAY_TIME = 30000;     // ms duration for full-size messages
int SHORT_DISPLAY_TIME = 10000;      // ms duration for terse status flashes
bool diagnosticMode = false;         // Self-test mode flag
uint8_t diagnosticPage = 0;          // Active diagnostic page

// Timers for processing
unsigned long lastMIDIProcess = 0;
unsigned long lastSerialProcess = 0;
unsigned long lastLEDUpdate = 0;
unsigned long lastEnvelopeProcess = 0;
unsigned long lastDisplayUpdate = 0;

// ButtonManagerContext: glue struct passed around to avoid global rummaging
ButtonManagerContext buttonContext = {potChannels,        activePot,      activeChannel,
                                      envelopeFollowMode, envelopeMode,   configManager,
                                      ledManager,         displayManager, envelopeFollowers,
                                      potToEnvelopeMap,   diagnosticMode, diagnosticPage};

void processMIDI() {
    midiHandler.processIncomingMIDI();

    static uint32_t lastDisplayTick = 0;
    uint32_t tickCount = midiHandler.clockTickCount();
    if (tickCount != lastDisplayTick) {
        uint32_t diff = tickCount - lastDisplayTick;
        lastDisplayTick = tickCount;

        lastClockTime = now();
        // Advance beat by however many ticks landed since the last pass
        midiBeatPosition = (midiBeatPosition + diff) % 8;

        // Perform clock-tied updates
        displayManager.updateDisplay(midiBeatPosition,
                                     std::vector<uint8_t>(), // Pass envelope levels if applicable
                                     envelopeFollowMode ? "EF ON" : "EF OFF", activePot,
                                     activeChannel, envelopeMode);

        // Record the last time a clock tick landed
        lastClockTime = now();

        midiHandler.clearClockTick();
    }
}

/*
 * Serial command rodeo — every lasso ends with a newline:
 *   HELLO                             : kick off WebSerial streaming
 *   GET_SCHEMA                        : cough up the config schema
 *   GET_BROWNOUTS                     : report how many times power sagged
 *   SET_POT,<slot>,<chan>,<cc>        : bind slot to MIDI channel+CC
 *   SET_ALL,<payload>                 : blast a JSON blob or bulk slot dump
 *   GET_ALL                           : dump every slot and LED setting
 *   GET_LED                           : spit back brightness,r,g,b
 *   SET_LED,<bri>,<r>,<g>,<b>         : 0‑255 each, paints the strip
 *   GET_ARGMETHOD                     : report current ARG blend
 *   SET_ARGMETHOD,<n>                 : n=0‑6 picks the blend
 *   GET_EF,<slot>                     : who’s modding that slot (‑1 means none)
 *   SET_EF,<slot>,<ef>                : patch an envelope follower
 *   CAL_ENVS                          : recalibrate all envelope spies
 *   GET_FILTER                        : reply with type,freq,q for EF filter
 *   SET_FILTER,<type>,<freq>,<q>      : stash envelope filter settings
 *   GET_ARGPAIR                       : echo ARG pair enable,envA,envB
 *   SET_ARGPAIR,<on>,<envA>,<envB>    : wire two envelopes together
 */
void processSerial() {
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

    // Process queued commands
    while (!commandQueue.empty()) {
        String command = commandQueue.front(); // Get the front command
        commandQueue.pop();                    // Remove it from the queue

        command.trim();

        if (command == "HELLO") {
            webSerialStreaming = true;
            LOG_PRINTLN("{\"hello\":\"mn42\"}");

        } else if (command == "GET_SCHEMA") {
            LOG_PRINTLN(ConfigManager::makeSchema());

        } else if (command == "GET_BROWNOUTS") {
            LOG_PRINTLN(g_brownoutCount);

        } else if (command.startsWith("SET_POT")) {
            // Parse "SET_POT" command
            int firstComma = command.indexOf(',');
            int lastComma = command.lastIndexOf(',');

            if (firstComma == -1 || lastComma == -1 || firstComma == lastComma) {
                LOG_PRINTLN("Error: Malformed SET_POT command");
                continue; // Skip invalid command
            }

            int potIndex = command.substring(8, firstComma).toInt();
            int channel = command.substring(firstComma + 1, lastComma).toInt();
            int ccNumber = command.substring(lastComma + 1).toInt();

            if (potIndex >= 0 && potIndex < NUM_POTS && channel >= 1 && channel <= 16 &&
                ccNumber >= 0 && ccNumber <= 127) {
                configManager.setPotChannel(potIndex, channel);
                configManager.setPotCCNumber(potIndex, ccNumber);
                potentiometerManager.setChannel(potIndex, channel);
                if (static_cast<size_t>(potIndex) < potChannels.size()) {
                    potChannels[potIndex] = channel;
                }
                configManager.saveConfiguration();
                LOG_PRINTLN("Pot configuration updated!");
            } else {
                LOG_PRINTLN("Error: Invalid values for SET_POT");
            }

        } else if (command.startsWith("SET_ALL")) {
            String payload = command.substring(8);
            if (payload.startsWith("{")) {
                StaticJsonDocument<256> doc;
                DeserializationError err = deserializeJson(doc, payload);
                if (err) {
                    LOG_PRINTLN("ERR");
                } else {
                    if (doc.containsKey("led")) {
                        JsonObject led = doc["led"];
                        uint8_t brightness = led.containsKey("brightness")
                                                 ? led["brightness"].as<uint8_t>()
                                                 : ledManager.getBrightness();
                        ledManager.setBrightness(brightness);
                        CRGB color = ledManager.getColor();
                        if (led.containsKey("color")) {
                            const char *cstr = led["color"];
                            if (cstr && cstr[0] == '#' && strlen(cstr) == 7) {
                                long rgb = strtol(cstr + 1, nullptr, 16);
                                color = CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
                                ledManager.setColor(color);
                            }
                        }
                        configManager.saveLEDSettings(brightness, color);
                    }
                    LOG_PRINTLN("OK");
                }
            } else {
                Utility::processBulkUpdate(command, configManager.getNumPots());
            }

        } else if (command.startsWith("GET_ALL")) {
#ifdef SERIAL_LOGGING
            // Send all pot settings
            LOG_PRINT("POTS:");
            for (int i = 0; i < NUM_POTS; i++) {
                int envelopeValue =
                    potToEnvelopeMap.count(i) ? potToEnvelopeMap[i] : ENVELOPE_UNASSIGNED;
                LOG_PRINT(configManager.getPotCCNumber(i));
                LOG_PRINT(",");
                LOG_PRINT(configManager.getPotChannel(i));
                LOG_PRINT(",");
                LOG_PRINT(envelopeValue);
                LOG_PRINT(";");
            }

            // Send LED settings
            CRGB ledColor = ledManager.getColor();
            LOG_PRINT(" LED:");
            LOG_PRINT(ledManager.getBrightness());
            LOG_PRINT(",");
            LOG_PRINT(ledColor.r);
            LOG_PRINT(",");
            LOG_PRINT(ledColor.g);
            LOG_PRINT(",");
            LOG_PRINTLN(ledColor.b);
#endif
        } else if (command == "GET_LED") {
#ifdef SERIAL_LOGGING
            CRGB c = ledManager.getColor();
            LOG_PRINT(ledManager.getBrightness());
            LOG_PRINT(",");
            LOG_PRINT(c.r);
            LOG_PRINT(",");
            LOG_PRINT(c.g);
            LOG_PRINT(",");
            LOG_PRINTLN(c.b);
#endif
        } else if (command.startsWith("SET_LED")) {
            int first = command.indexOf(',');
            int second = command.indexOf(',', first + 1);
            int third = command.indexOf(',', second + 1);
            if (first == -1 || second == -1 || third == -1) {
                LOG_PRINTLN("ERR");
            } else {
                int brightness = command.substring(8, first).toInt();
                int r = command.substring(first + 1, second).toInt();
                int g = command.substring(second + 1, third).toInt();
                int b = command.substring(third + 1).toInt();
                if (brightness >= 0 && brightness <= 255 && r >= 0 && r <= 255 && g >= 0 &&
                    g <= 255 && b >= 0 && b <= 255) {
                    CRGB color(r, g, b);
                    ledManager.setBrightness(brightness);
                    ledManager.setColor(color);
                    configManager.saveLEDSettings(brightness, color);
                    LOG_PRINTLN("OK");
                } else {
                    LOG_PRINTLN("ERR");
                }
            }
        } else if (command == "GET_ARGMETHOD") {
            LOG_PRINTLN(configManager.getARGMethod());
        } else if (command.startsWith("SET_ARGMETHOD")) {
            // SET_ARGMETHOD <method>
            // method: 0-13 mapping to EnvelopeFollower::ARG_Method; see
            // firmware/include/EnvelopeFollower/README.md#arg-methods for the math.
            // Side effects: blasts method into every follower and burns it into
            // EEPROM via ConfigManager.
            int method = command.substring(14).toInt();
            if (method >= 0 && method <= 13) {
                for (auto &ef : envelopeFollowers) {
                    ef.setARGMethod(static_cast<EnvelopeFollower::ARG_Method>(method));
                }
                configManager.setARGMethod(method);
                LOG_PRINTLN("OK");
            } else {
                // out-of-range method? we spit ERR
                LOG_PRINTLN("ERR");
            }
        } else if (command.startsWith("GET_EF")) {
            // GET_EF <slot>
            // slot: 0..NUM_POTS-1. Reports which envelope (or -1) owns it.
            // See firmware/README.md#L730-L744 or docs/WebSerial.md#L60-L72 for
            // the whole WebSerial spiel.
            int potIndex = command.substring(7).toInt();
            if (potIndex >= 0 && potIndex < NUM_POTS) {
                int env = potToEnvelopeMap.count(potIndex) ? potToEnvelopeMap[potIndex]
                                                           : ENVELOPE_UNASSIGNED;
#ifdef SERIAL_LOGGING
                LOG_PRINTLN(env);
#else
                (void)env;
#endif
            } else {
                // bogus slot index
                LOG_PRINTLN("ERR");
            }
        } else if (command.startsWith("SET_EF")) {
            // SET_EF <slot,env>
            // slot: 0..NUM_POTS-1, env: 0..envelopeFollowers.size()-1
            // Side effects: maps slot to follower, flips it active, and
            // saves mapping/baseline to EEPROM. See firmware/README.md#L730-L744
            // and firmware/include/EnvelopeFollower/README.md for EF guts.
            int comma = command.indexOf(',');
            if (comma == -1) {
                LOG_PRINTLN("ERR");
            } else {
                int potIndex = command.substring(7, comma).toInt();
                int envIndex = command.substring(comma + 1).toInt();
                if (potIndex >= 0 && potIndex < NUM_POTS && envIndex >= 0 &&
                    envIndex < (int)envelopeFollowers.size()) {
                    potToEnvelopeMap[potIndex] = envIndex;
                    envelopeFollowers[envIndex].toggleActive(true);
                    configManager.saveEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);
                    LOG_PRINTLN("OK");
                } else {
                    // numbers don't line up? it's an ERR
                    LOG_PRINTLN("ERR");
                }
            }
        } else if (configManager.handleCommand(command)) {
            // handled inside ConfigManager
        } else {
            LOG_PRINTLN("Unknown command: " + command);
        }
    }
}

void processEnvelopes() {
    // Stroll through the pot→envelope map; every pair says which envelope
    // rides shotgun with which physical pot.
    for (const auto &[potIndex, envelopeIndex] : potToEnvelopeMap) {
        // Trust no one: make sure the map didn't hand us a bogus index
        // before poking the envelope array.
        if (envelopeIndex < 0 || envelopeIndex >= static_cast<int>(envelopeFollowers.size())) {
            continue;
        }
        EnvelopeFollower *envelope = &envelopeFollowers[envelopeIndex];

        // Only waste cycles on envelopes that are actually lit up.
        // Sleeping envelopes don't get CPU time or make noise.
        if (envelope->getActiveState()) {
            envelope->update(); // Pull in the latest peak/decay stats.

            uint8_t ccValue = potentiometerManager.getCCNumber(potIndex);
            // applyToCC mutates ccValue with the envelope's swagger and will
            // sling a CC at the target if that modulation changed anything.
            envelope->applyToCC(potIndex, ccValue);
            ledManager.setEnvelopeLevel(envelopeIndex, envelope->getEnvelopeLevel());

            // If the tweaked value differs from what the pot last screamed,
            // fire off a fresh CC and light the pot LED accordingly.
            if (ccValue != potentiometerManager.getLastValue(potIndex)) {
                midiHandler.sendControlChange(potentiometerManager.getCCNumber(potIndex), ccValue,
                                              potentiometerManager.getChannel(potIndex));

                ledManager.setPotValue(potIndex, ccValue);
            }
        }
    }

    // After the dust settles, mirror the active pot's MIDI-scaled value on
    // every indicator LED so the panel shows exactly what that knob is yelling.
    uint8_t potMidiValue =
        Utility::mapToMidiValue(potentiometerManager.getLastValue(buttonContext.activePot));
    for (uint8_t i = 0; i < POT_LED_COUNT; ++i) {
        ledManager.setPotIndicator(i, potMidiValue);
    }
}

// Kick out MIDI clock pulses if the outside world bails on us.
void processInternalClock() {
    static unsigned long lastInternalTick = 0;
    if (g_tappedBPM <= 0.0f)
        return; // No tempo tapped, nothing to do

    float msPerTick = 60000.0f / (g_tappedBPM * 24.0f); // 24 PPQN
    unsigned long now = ::now();
    if (now - lastInternalTick >= msPerTick) {
        lastInternalTick = now;
        lastClockTime = now;

        // Chuck out a clock tick if we're allowed to shout. The MIDI handler will
        // mirror it out and bump the shared counter so processMIDI() can advance
        // beats and refresh the display exactly once per pulse.
        midiHandler.generateClockTick();
    }
}

// Measure how often the main loop cycles each second—our quick-and-dirty load meter.
// On a healthy Teensy 4.0 we usually see about a kilospin per second; if it tanks,
// you're choking the core.
void monitorSystemLoad() {
    static unsigned long lastMonitorTime = 0;
    static unsigned long taskCounter = 0; // main loop iterations

    taskCounter++;                                          // count another lap around the loop
    if (now() - lastMonitorTime >= 1000UL) {                // Log every second
        LOG_PRINTF("Tasks per second: %lu\n", taskCounter); // ~1000 on a chill rig
        taskCounter = 0;
        lastMonitorTime = now();
    }
}

void updateFilterTuning(ButtonManagerContext &context) {
    // 1. Read raw ADC from freq pot
    int rawFreq = buttonManager.getControlPotValue(1); // MUXC channel 13
    // 2. Read raw ADC from Q pot
    int rawQ = buttonManager.getControlPotValue(2); // MUXC channel 14

    // 3. Map rawFreq => 20..5000 Hz (pick a range that feels good)
    float freq = map(rawFreq, 0, 1023, 20, 5000);

    // 4. Map rawQ => 0.5..4.0 (a typical resonance range)
    //    - For instance, map from 0..1023 => 50..400, then /100
    float q = map(rawQ, 0, 1023, 50, 400) / 100.0f; // => 0.50..4.00

    // 5. Which EF are we tuning?
    //    We'll tune the EF assigned to the “activePot” in the context
    auto it = context.potToEnvelopeMap.find(context.activePot);
    if (it == context.potToEnvelopeMap.end() || it->second == ENVELOPE_UNASSIGNED) {
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
    // LOG_PRINTF("EF %d => freq=%.1f Q=%.2f\n", efIndex, freq, q);
}

void updateArpTuning() {
    if (!arpeggiator.isActive())
        return;

    int rawLen = buttonManager.getControlPotValue(1);
    int rawShape = buttonManager.getControlPotValue(2);

    // Knob #1 owns the step length. Its raw 10‑bit reading gets linearly remapped
    // to MIDI clock ticks so the riff stays welded to the global tempo:
    //   0     -> 1 tick   (every pulse)
    //   1023  -> MAX_LENGTH ticks (a whole quarter note at 24 PPQN)
    // Future hackers: tweak Arpeggiator::MAX_LENGTH if you want longer gaps.
    uint8_t lengthTicks = map(rawLen, 0, 1023, 1, Arpeggiator::MAX_LENGTH);
    int shapeIdx = map(rawShape, 0, 1023, 0, 3);
    static const char *names[] = {"UP", "DOWN", "UPDN", "RAND"};
    Arpeggiator::Shape shapes[] = {Arpeggiator::UP, Arpeggiator::DOWN, Arpeggiator::UPDOWN,
                                   Arpeggiator::RANDOM};

    // Pump the tick count into the arp engine and shape selector.
    arpeggiator.setLength(lengthTicks);
    arpeggiator.setShape(shapes[shapeIdx]);

    // Flash the current groove math on the OLED so humans can vibe too.
    displayManager.showArpSettings(lengthTicks, names[shapeIdx]);
}

void updateNoteDynamics() {
    if (arpeggiator.isActive())
        return;

    int rawShift = buttonManager.getControlPotValue(1);
    int rawProb = buttonManager.getControlPotValue(2);

    velocityShift = map(rawShift, 0, 1023, -64, 63);
    changeProbability = static_cast<uint8_t>(map(rawProb, 0, 1023, 0, 100));

    String line2 = String("Vel ") + String(velocityShift);
    String line3 = String("Prob ") + String(changeProbability) + "%";
    displayManager.showText("Note Dyn", line2.c_str(), line3.c_str());
}

void streamWebSerialState() {
    if (!webSerialStreaming)
        return;
    WebSerial::sendStateSnapshot(potentiometerManager, envelopeFollowers);
}

void setup() {
    // — Serial & Config —
    Serial.begin(SERIAL_BAUD);
    Serial.printf("MN42 FW %s %s\n", FW_VERSION_STR, GIT_SHA_STR);
    g_resetCause = SRC_SRSR;
    EEPROM.get(EEPROM_BROWNOUT_COUNT, g_brownoutCount);
    if (g_resetCause & 0x40) {
        g_brownoutCount++;
        EEPROM.put(EEPROM_BROWNOUT_COUNT, g_brownoutCount);
    }
    Serial.printf("MN42 FW %s schema %04X UID %08lX%08lX%08lX%08lX\n", FW_VERSION_STR,
                  CONFIG_VERSION, HW_OCOTP_CFG0, HW_OCOTP_CFG1, HW_OCOTP_CFG2, HW_OCOTP_CFG3);
    Serial.printf("Reset 0x%08lX Brownouts %u\n", g_resetCause, g_brownoutCount);

    // Configure status LED
    pinMode(hwConfig.statusLedPin, OUTPUT);
    digitalWrite(hwConfig.statusLedPin, LOW);

    // Measure VREF for baseline calibration
    pinMode(VREF_ADC_PIN, INPUT);
    g_vref = Utility::readVrefADC(VREF_ADC_PIN);

    // Load per-slot EEPROM into RAM, and pot→channel data into potChannels[]
    configManager.begin(potChannels);
    configManager.loadMIDISlots(&configManager.getSlot(0), NUM_SLOTS);
    bool baselinesLoaded = configManager.loadEnvelopeSettings(potToEnvelopeMap, envelopeFollowers);

    // — MIDI Handler —
    midiHandler.begin();
    midiHandler.setDisplayManager(&displayManager);
    seedbox::interop::mn42::SeedBoxLink::instance().begin(&midiHandler);

    // — Pot → MIDI routing callback —
    potentiometerManager.setMidiCallback(
        [&](uint8_t /*ccNumber*/, uint8_t value, uint16_t rawValue, uint8_t potIdx) {
            auto &slot = configManager.getSlot(potIdx);
            if (!slot.active)
                return;

            switch (slot.type) {
            case MIDIMessageType::CC:
                midiHandler.sendControlChange(slot.data1, value, slot.midiChannel);
                break;

            case MIDIMessageType::Note: {
                uint8_t note = Utility::mapToMidiValue(rawValue) % 128;
                slot.arpNote = note; // stash for the arpeggiator
                uint8_t velo = (slot.efIndex < envelopeFollowers.size())
                                   ? envelopeFollowers[slot.efIndex].getEnvelopeLevel()
                                   : 125;
                int shifted = velo + velocityShift;
                if (shifted < 0)
                    shifted = 0;
                if (shifted > 127)
                    shifted = 127;
                if (random(100U) >= changeProbability)
                    break;
                midiHandler.sendNoteOn(note, shifted, slot.midiChannel);
                // schedule Note-Off in 100 ms
                Utility::schedulerHigh.addTask(
                    [=]() { midiHandler.sendNoteOff(note, 0, slot.midiChannel); }, 100);
                break;
            }

            case MIDIMessageType::PitchBend: {
                int16_t bend = map(static_cast<int>(rawValue), 0, 1023, -8192, 8191);
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

            case MIDIMessageType::ModWheel: {
                uint8_t mod = Utility::mapToMidiValue(rawValue);
                midiHandler.sendModWheel(mod, slot.midiChannel);
                break;
            }

            case MIDIMessageType::NRPN: {
                uint16_t param = static_cast<uint16_t>(slot.data1) << 7; // LSB zeroed
                uint16_t val = static_cast<uint16_t>(Utility::mapToMidiValue(rawValue)) << 7;
                midiHandler.sendNRPN(param, val, slot.midiChannel);
                break;
            }

            case MIDIMessageType::RPN: {
                uint16_t param = static_cast<uint16_t>(slot.data1) << 7; // LSB zeroed
                uint16_t val = static_cast<uint16_t>(Utility::mapToMidiValue(rawValue)) << 7;
                midiHandler.sendRPN(param, val, slot.midiChannel);
                break;
            }

            case MIDIMessageType::SysEx: {
                uint8_t msg[4] = {0xF0, slot.data1, Utility::mapToMidiValue(rawValue), 0xF7};
                midiHandler.sendSysEx(msg, 4);
                break;
            }

            default:
                break;
            }
        });

    // — LEDs & Display —
    ledManager.begin();
    uint8_t ledB;
    CRGB ledC;
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
    for (auto &ef : envelopeFollowers) {
        ef.toggleActive(true);
        if (!baselinesLoaded) {
            ef.calibrate();
        }
    }
    float sf, sq;
    EEPROM.get(EEPROM_FILTER_FREQ, sf);
    EEPROM.get(EEPROM_FILTER_Q, sq);
    sf = constrain(sf, 20.0f, 5000.0f);
    sq = constrain(sq, 0.5f, 4.0f);
    for (auto &ef : envelopeFollowers)
        ef.configureFilter(sf, sq);

    // — Slot sanity check (channel & CC) —
    for (uint8_t i = 0; i < NUM_SLOTS; i++) {
        if (potentiometerManager.getChannel(i) == 0)
            potentiometerManager.setChannel(i, 1);
        if (potentiometerManager.getCCNumber(i) > 127)
            potentiometerManager.setCCNumber(i, i % 128);
    }

    // — Load or reset full config —
    if (!configManager.loadConfiguration(potChannels)) {
        LOG_PRINTLN("EEPROM corrupted → resetting.");
        potentiometerManager.resetEEPROM();
    }

    // — Buttons & splash —
    buttonManager.initButtons();
    delay(1000);
    displayManager.clear();
    displayManager.showText("MOAR");
    ledManager.blinkStatusLED(2, 100);

    displayManager.runStartupAnimation();

    // — Scheduler tasks —
    // Three cooperative schedulers slice time so nothing blocks:
    // High-priority (1 ms):
    Utility::schedulerHigh.addTask(processMIDI, hwConfig.midiTaskInterval);
    Utility::schedulerHigh.addTask(
        []() {
            if (now() - lastClockTime > CLOCK_TIMEOUT_MS)
                processInternalClock();
        },
        hwConfig.midiTaskInterval);
    Utility::schedulerHigh.addTask(
        []() { arpeggiator.update(midiHandler, configManager, potentiometerManager); },
        hwConfig.midiTaskInterval);

    // Mid-priority (~5 ms):
    Utility::schedulerMid.addTask(processSerial, hwConfig.serialTaskInterval);
    Utility::schedulerMid.addTask(processEnvelopes, hwConfig.envelopeTaskInterval);

    // Low-priority (~50 ms):
    Utility::schedulerLow.addTask(
        []() {
            ledManager.update();
            updateFilterTuning(buttonContext);
            updateArpTuning();
            updateNoteDynamics();
        },
        hwConfig.ledTaskInterval);

    Utility::schedulerLow.addTask(
        []() {
            if (diagnosticMode) {
                displayManager.beginDraw();
                displayManager.showDiagnostic(diagnosticPage, buttonManager, buttonContext,
                                              midiHandler);
                displayManager.endDraw();
            } else if (!displayManager.shouldRunScreensaver()) {
                displayManager.beginDraw();
                displayManager.updateFromContext(buttonContext);
                auto it = potToEnvelopeMap.find(buttonContext.activePot);
                if (it != potToEnvelopeMap.end() && it->second != ENVELOPE_UNASSIGNED) {
                    displayManager.showEnvelopeLevel(
                        envelopeFollowers[it->second].getEnvelopeLevel());
                }
                displayManager.highlightActivePot(buttonContext.activePot);
                displayManager.highlightActiveMode(envelopeMode);
                displayManager.endDraw();
            } else {
                displayManager.runIdleScreensaver();
            }
        },
        100);

    Utility::schedulerLow.addTask(
        []() { seedbox::interop::mn42::SeedBoxLink::instance().update(); }, 500);

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
