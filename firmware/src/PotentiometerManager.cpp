// PotentiometerManager is the analog whisperer. Every comment is tuned to teach
// why multiplexers need settle time, how we smooth jitter without wrecking
// responsiveness, and how MIDI callbacks use both the mapped CC value and the
// raw ADC reading for richer control surfaces.

#include "PotentiometerManager.h"
#include "ConfigManager.h"
#include "Globals.h"
#include "Hardware/IO.h"
#include "Utility.h"
#include "Log.h"
#include "bench_log_latency.h"
#include <vector>

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

void PotentiometerManager::attachConfigManager(ConfigManager &cfg) {
    configManager = &cfg;
    syncChannelCacheFromConfig();
}

// Select the primary mux bank and avoid redundant GPIO writes when it has not changed.
void PotentiometerManager::selectMuxBank(uint8_t bank) {
    static uint8_t lastBank = 255; // Track the last selected bank
    if (bank != lastBank) {
        for (int i = 0; i < PRIMARY_MUX_PINS; i++) {
            digitalWrite(primaryMuxPins[i], (bank >> i) & 1);
        }
        lastBank = bank;
    }
}

// Select the secondary mux bank for the active pot inside the current primary group.
void PotentiometerManager::selectPotBank(uint8_t pot) {
    static uint8_t lastPot = 255; // Track the last selected pot
    if (pot != lastPot) {
        for (int i = 0; i < SECONDARY_MUX_PINS; i++) {
            digitalWrite(secondaryMuxPins[i], (pot >> i) & 1);
        }
        lastPot = pot;
    }
}

// Average a few analog samples to smooth mux/ADC noise before EWMA kicks in.
int PotentiometerManager::readAnalogFiltered(uint8_t pin) {
    int total = 0;
    const int numSamples = 4; // Number of samples for averaging

    for (int i = 0; i < numSamples; i++) {
        total += hardware::readAnalog(pin); // Read analog value
        delayMicroseconds(10);              // Small delay for stability
    }

    return total / numSamples; // Return the averaged value
}

// Update one pot's MIDI channel in both runtime cache and persisted config.
void PotentiometerManager::setChannel(int potIndex, uint8_t channel) {
    if (potIndex < NUM_POTS) {
        if (configManager) {
            configManager->setPotChannel(static_cast<uint8_t>(potIndex), channel);
        }
        potChannels[potIndex] = channel;
    }
}

// Return the most recent raw-ish reading cached for one pot.
int PotentiometerManager::getLastValue(int potIndex) const {
    if (potIndex >= 0 && potIndex < NUM_POTS) {
        return potLastValues[potIndex];
    } else {
        return -1; // Return a sentinel value for invalid index
    }
}

// Update one pot's CC number in both runtime cache and persisted config.
void PotentiometerManager::setCCNumber(int potIndex, uint8_t ccNumber) {
    if (potIndex < NUM_POTS) {
        if (configManager) {
            configManager->setPotCCNumber(static_cast<uint8_t>(potIndex), ccNumber);
        }
        potCCNumbers[potIndex] = ccNumber;
    }
}

// Resolve the current MIDI channel, preferring ConfigManager when attached.
uint8_t PotentiometerManager::getChannel(int potIndex) {
    if (configManager) {
        return configManager->getPotChannel(static_cast<uint8_t>(potIndex));
    }
    return (potIndex < NUM_POTS) ? potChannels[potIndex] : 0;
}

// Resolve the current CC number, preferring ConfigManager when attached.
uint8_t PotentiometerManager::getCCNumber(int potIndex) {
    if (configManager) {
        return configManager->getPotCCNumber(static_cast<uint8_t>(potIndex));
    }
    return (potIndex < NUM_POTS) ? potCCNumbers[potIndex] : 0;
}

// Inject a host-driven MIDI value as if the physical pot had moved there.
void PotentiometerManager::injectMidiValue(uint8_t potIndex, uint8_t midiValue) {
    if (potIndex >= NUM_POTS) {
        return;
    }
    const uint8_t bounded = static_cast<uint8_t>(constrain(static_cast<int>(midiValue), 0, 127));
    const uint16_t rawValue =
        static_cast<uint16_t>(map(static_cast<int>(bounded), 0, 127, 0, 1023));
    potLastValues[potIndex] = static_cast<int>(rawValue);
    smoothedValue[potIndex] = static_cast<int>(rawValue);
    dirtyFlags[potIndex] = true;
    if (midiCallback) {
        midiCallback(getCCNumber(potIndex), bounded, rawValue, potIndex);
    }
}

// Scan the full mux tree, update smoothing state, and emit callbacks for meaningful pot moves.
void PotentiometerManager::processPots(LedAnimator &ledAnimator,
                                       std::vector<EnvelopeFollower> &envelopes) {
    (void)envelopes; // explicitly marked unused

    static uint8_t currentPotIndex = 0;
    const uint8_t potsPerFrame = 14;

    for (uint8_t i = 0; i < potsPerFrame; ++i) {
        uint8_t primaryBank = currentPotIndex >> SECONDARY_MUX_PINS;
        uint8_t secondaryBank = currentPotIndex & ((1 << SECONDARY_MUX_PINS) - 1);

        // Stage 1a: the primary mux selects which gang of pots we're sniffing.
        selectMuxBank(primaryBank);

        // Stage 1b: secondary mux dials in the exact pot within that gang.
        selectPotBank(secondaryBank);

        // Stage 2: snag the raw voltage and run it through our tiny RC filter.
        int rawValue = readAnalogFiltered(analogPin);

        // EWMA smoothing – ALPHA (see header) leans toward fresh readings.
        smoothedValue[currentPotIndex] =
            Utility::exponentialMovingAverage(rawValue, smoothedValue[currentPotIndex], ALPHA);
        int smoothedReading = smoothedValue[currentPotIndex];

        // Stage 3: bail if the movement is smaller than the noise floor.
        if (abs(smoothedReading - potLastValues[currentPotIndex]) > CHANGE_THRESHOLD) {
            potLastValues[currentPotIndex] = smoothedReading; // lock in the latest value
            dirtyFlags[currentPotIndex] = true;

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
            ledAnimator.setPotTarget(currentPotIndex, midiValue);

            if (midiCallback) {
#if BENCH_LATENCY_LOG
                benchLatencyLog(currentPotIndex, t_scan_us, "MIDI", "");
#endif
                midiCallback(getCCNumber(currentPotIndex), midiValue,
                             static_cast<uint16_t>(smoothedReading), currentPotIndex);
            }
        }

        currentPotIndex++;
        if (currentPotIndex >= NUM_POTS) {
            currentPotIndex = 0;
            break;
        }
    }
}

// Pull pot routing state from EEPROM or ConfigManager into the live cache.
void PotentiometerManager::loadFromEEPROM() {
    LOG_PRINTLN(
        "{\"type\":\"info\",\"message\":\"Loading potentiometer settings from EEPROM...\"}");
    if (configManager) {
        std::vector<uint8_t> channels;
        if (!configManager->loadConfiguration(channels)) {
            configManager->resetConfiguration(channels);
        }
        syncChannelCacheFromConfig();
        return;
    }
    for (uint8_t i = 0; i < NUM_POTS; i++) {
        uint16_t channelAddress = EEPROM_POT_CHANNELS + i;
        uint16_t ccAddress = EEPROM_POT_CC + i;
        potChannels[i] = ConfigManager::getStorageBackend()->read(channelAddress);
        potCCNumbers[i] = ConfigManager::getStorageBackend()->read(ccAddress);
    }
}

// Reset pot routing storage back to the simple channel-1 / CC-index baseline.
void PotentiometerManager::resetEEPROM() {
    LOG_PRINTLN(
        "{\"type\":\"info\",\"message\":\"Resetting EEPROM settings for potentiometers...\"}");
    if (configManager) {
        std::vector<uint8_t> channels;
        configManager->resetConfiguration(channels);
        syncChannelCacheFromConfig();
        return;
    }
    for (uint8_t i = 0; i < NUM_POTS; i++) {
        potChannels[i] = 1;  // Default MIDI channel
        potCCNumbers[i] = i; // Default CC number
        uint16_t channelAddress = EEPROM_POT_CHANNELS + i;
        uint16_t ccAddress = EEPROM_POT_CC + i;
        ConfigManager::getStorageBackend()->update(channelAddress, potChannels[i]);
        ConfigManager::getStorageBackend()->update(ccAddress, potCCNumbers[i]);
    }
}

// Persist the current pot routing cache when ConfigManager is not acting as the owner.
void PotentiometerManager::saveToEEPROM() {
    if (configManager) {
        configManager->saveConfiguration();
        return;
    }
    for (uint8_t i = 0; i < NUM_POTS; i++) {
        uint16_t channelAddress = EEPROM_POT_CHANNELS + i;
        uint16_t ccAddress = EEPROM_POT_CC + i;
        ConfigManager::getStorageBackend()->update(channelAddress, potChannels[i]);
        ConfigManager::getStorageBackend()->update(ccAddress, potCCNumbers[i]);
    }
}

// Store the two envelope followers currently feeding the ARG combiner.
void PotentiometerManager::setArgEnvelopePair(int a, int b) {
    argEnvA = a;
    argEnvB = b;
}

// Return the currently selected ARG envelope pair.
void PotentiometerManager::getArgEnvelopePair(int &a, int &b) const {
    a = argEnvA;
    b = argEnvB;
}

// Force a direct immediate read of one pot without touching the wider scan loop.
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

// Refresh the local pot routing caches after config/profile loads.
void PotentiometerManager::syncChannelCacheFromConfig() {
    if (!configManager) {
        return;
    }
    for (uint8_t i = 0; i < NUM_POTS; ++i) {
        potChannels[i] = configManager->getPotChannel(i);
        potCCNumbers[i] = configManager->getPotCCNumber(i);
    }
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
