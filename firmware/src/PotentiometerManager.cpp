// PotentiometerManager is the analog whisperer. Every comment is tuned to teach
// why multiplexers need settle time, how we smooth jitter without wrecking
// responsiveness, and how MIDI callbacks use both the mapped CC value and the
// raw ADC reading for richer control surfaces.

#include "PotentiometerManager.h"
#include <EEPROM.h>
#include "ConfigManager.h"
#include "Globals.h"
#include "Hardware/IO.h"
#include "Utility.h"
#include "Log.h"
#include "bench_log_latency.h"

// Reads all potentiometers via a pair of multiplexers. The most recent values
// feed LEDManager for visual feedback and trigger MIDI messages through the
// callback registered by firmware_main.cpp.

static constexpr int CHANGE_THRESHOLD = 2; // Adjust based on your noise tolerance

PotentiometerManager::PotentiometerManager(const uint8_t *primaryPins, const uint8_t *secondaryPins,
                                           uint8_t analogPin)
    : primaryMuxPins(primaryPins), secondaryMuxPins(secondaryPins), analogPin(analogPin) {
    // Initialize pot default values
    for (int i = 0; i < NUM_POTS; i++) {
        potChannels[i] = 1;    // Default MIDI channel
        potCCNumbers[i] = i;   // Default MIDI CC number
        potLastValues[i] = -1; // Ensure the first read updates
        smoothedValue[i] = 0;  // EWMA starting point
        dirtyFlags[i] = false; // Nothing dirty yet
    }
}

void PotentiometerManager::selectMuxBank(uint8_t bank) {
    static uint8_t lastBank = 255; // Track the last selected bank
    if (bank != lastBank) {
        for (int i = 0; i < PRIMARY_MUX_PINS; i++) {
            digitalWrite(primaryMuxPins[i], (bank >> i) & 1);
        }
        lastBank = bank;
    }
}

void PotentiometerManager::selectPotBank(uint8_t pot) {
    static uint8_t lastPot = 255; // Track the last selected pot
    if (pot != lastPot) {
        for (int i = 0; i < SECONDARY_MUX_PINS; i++) {
            digitalWrite(secondaryMuxPins[i], (pot >> i) & 1);
        }
        lastPot = pot;
    }
}

int PotentiometerManager::readAnalogFiltered(uint8_t pin) {
    int total = 0;
    const int numSamples = 4; // Number of samples for averaging

    for (int i = 0; i < numSamples; i++) {
        total += hardware::readAnalog(pin); // Read analog value
        delayMicroseconds(10);              // Small delay for stability
    }

    return total / numSamples; // Return the averaged value
}

void PotentiometerManager::setChannel(int potIndex, uint8_t channel) {
    if (potIndex < NUM_POTS) {
        potChannels[potIndex] = channel;
    }
}

int PotentiometerManager::getLastValue(int potIndex) const {
    if (potIndex >= 0 && potIndex < NUM_POTS) {
        return potLastValues[potIndex];
    } else {
        return -1; // Return a sentinel value for invalid index
    }
}

void PotentiometerManager::setCCNumber(int potIndex, uint8_t ccNumber) {
    if (potIndex < NUM_POTS) {
        potCCNumbers[potIndex] = ccNumber;
    }
}

uint8_t PotentiometerManager::getChannel(int potIndex) {
    return (potIndex < NUM_POTS) ? potChannels[potIndex] : 0;
}

uint8_t PotentiometerManager::getCCNumber(int potIndex) {
    return (potIndex < NUM_POTS) ? potCCNumbers[potIndex] : 0;
}

void PotentiometerManager::processPots(LEDManager &ledManager,
                                       std::vector<EnvelopeFollower> &envelopes) {
    for (uint8_t primaryBank = 0; primaryBank < (1 << PRIMARY_MUX_PINS); primaryBank++) {
        if ((primaryBank << SECONDARY_MUX_PINS) >= NUM_POTS)
            break;
        // Stage 1a: the primary mux selects which gang of pots we're sniffing.
        selectMuxBank(primaryBank);

        for (uint8_t secondaryBank = 0; secondaryBank < (1 << SECONDARY_MUX_PINS);
             secondaryBank++) {
            // Stage 1b: secondary mux dials in the exact pot within that gang.
            selectPotBank(secondaryBank);

            // Mash the two bank numbers together to get the global pot index.
            uint8_t potIndex = (primaryBank << SECONDARY_MUX_PINS) | secondaryBank;

            if (potIndex >= NUM_POTS)
                break;

            // Stage 2: snag the raw voltage and run it through our tiny RC filter.
            int rawValue = readAnalogFiltered(analogPin);

            // EWMA smoothing – ALPHA (see header) leans toward fresh readings.
            smoothedValue[potIndex] =
                Utility::exponentialMovingAverage(rawValue, smoothedValue[potIndex], ALPHA);
            int smoothedReading = smoothedValue[potIndex];

            // Stage 3: bail if the movement is smaller than the noise floor.
            if (abs(smoothedReading - potLastValues[potIndex]) > CHANGE_THRESHOLD) {
                potLastValues[potIndex] = smoothedReading; // lock in the latest value
                dirtyFlags[potIndex] = true;

#if BENCH_LATENCY_LOG
                static bool headerPrinted = false;
                if (!headerPrinted) {
                    benchLatencyHeader();
                    headerPrinted = true;
                }
                uint32_t t_scan_us = micros();
#endif

                // Stage 4: map to MIDI, light the LED, then shout over MIDI.
                int midiValue = Utility::mapToMidiValue(smoothedReading);
                ledManager.setPotValue(potIndex, midiValue);

                if (midiCallback) {
#if BENCH_LATENCY_LOG
                    benchLatencyLog(potIndex, t_scan_us, "MIDI", "");
#endif
                    midiCallback(potCCNumbers[potIndex], midiValue,
                                 static_cast<uint16_t>(smoothedReading), potIndex);
                }
            }
        }
    }
}

void PotentiometerManager::loadFromEEPROM() {
    LOG_PRINTLN("Loading potentiometer settings from EEPROM...");
    for (uint8_t i = 0; i < NUM_POTS; i++) {
        uint16_t channelAddress = EEPROM_POT_CHANNELS + i;
        uint16_t ccAddress = EEPROM_POT_CC + i;
        potChannels[i] = EEPROM.read(channelAddress);
        potCCNumbers[i] = EEPROM.read(ccAddress);
    }
}

void PotentiometerManager::resetEEPROM() {
    LOG_PRINTLN("Resetting EEPROM settings for potentiometers...");
    for (uint8_t i = 0; i < NUM_POTS; i++) {
        potChannels[i] = 1;  // Default MIDI channel
        potCCNumbers[i] = i; // Default CC number
        uint16_t channelAddress = EEPROM_POT_CHANNELS + i;
        uint16_t ccAddress = EEPROM_POT_CC + i;
        EEPROM.update(channelAddress, potChannels[i]);
        EEPROM.update(ccAddress, potCCNumbers[i]);
    }
}

void PotentiometerManager::saveToEEPROM() {
    for (uint8_t i = 0; i < NUM_POTS; i++) {
        uint16_t channelAddress = EEPROM_POT_CHANNELS + i;
        uint16_t ccAddress = EEPROM_POT_CC + i;
        EEPROM.update(channelAddress, potChannels[i]);
        EEPROM.update(ccAddress, potCCNumbers[i]);
    }
}

void PotentiometerManager::setArgEnvelopePair(int a, int b) {
    argEnvA = a;
    argEnvB = b;
}

void PotentiometerManager::getArgEnvelopePair(int &a, int &b) const {
    a = argEnvA;
    b = argEnvB;
}

int PotentiometerManager::readRawPot(uint8_t potIndex) {
    // decode into bank and pot bits
    uint8_t bank = potIndex >> SECONDARY_MUX_PINS;
    uint8_t pot = potIndex & ((1 << SECONDARY_MUX_PINS) - 1);

    // do the private selects
    selectMuxBank(bank);
    selectPotBank(pot);
    delayMicroseconds(5);                   // settle time
    return hardware::readAnalog(analogPin); // direct raw read
}

void PotentiometerManager::setMidiCallback(
    std::function<void(uint8_t, uint8_t, uint16_t, uint8_t)> cb) {
    // Callback signature is: (ccNumber, midiValue, rawAdc, potIndex).
    // The double payload is intentional—classroom time gets a lot juicier when
    // you can show the raw 0‑1023 data next to the scaled 0‑127 output, and it
    // lets external schedulers reuse the analog voltage without re-reading the
    // mux.
    midiCallback = cb;
}
