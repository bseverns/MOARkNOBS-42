// Oversees reading all analog pots via multiplexers.
// Calls a user-supplied callback for MIDI output and drives the LEDManager.
#ifndef POTENTIOMETER_MANAGER_H
#define POTENTIOMETER_MANAGER_H

#include <Arduino.h>
#include <functional>
#include <vector> // For std::vector
#include "LEDManager.h"
#include "LedAnimator.h"
#include "Utility.h"
#include "Globals.h"

// Forward declarations to avoid circular dependency
class EnvelopeFollower;
class ConfigManager;

inline constexpr uint8_t PRIMARY_MUX_PINS =
    4; // Address lines for the "upstream" mux selecting which pot bank hits the bus
inline constexpr uint8_t SECONDARY_MUX_PINS =
    4; // Address lines for the "downstream" mux choosing a single pot within that bank

/*
Reads all potentiometers via a pair of analog multiplexers and
       forwards the values as MIDI messages.
*/
class PotentiometerManager {
  private:
    const uint8_t *primaryMuxPins; // Drives the primary mux: pick which secondary mux is talking
    const uint8_t
        *secondaryMuxPins;         // Drives the secondary mux: pick the actual pot within that bank
    const uint8_t analogPin;       // Analog pin for mux output
    uint8_t potChannels[NUM_POTS]; // MIDI channel for each pot
    uint8_t potCCNumbers[NUM_POTS]; // MIDI CC number for each pot
    int potLastValues[NUM_POTS];    // Last read values for each pot
    ConfigManager *configManager = nullptr;

    // Exponential Weighted Moving Average (EWMA) smoothing
    // keeps analog jitter down without killing responsiveness.
    static constexpr float ALPHA = 0.1f; // weight of the newest sample
    int smoothedValue[NUM_POTS];         // running EWMA for each pot
    bool scanInitialized[NUM_POTS];      // first physical sample seeds the filter, never a gesture
    bool remoteTakeoverActive[NUM_POTS];
    int remoteTargetRaw[NUM_POTS];
    int remotePreviousPhysicalRaw[NUM_POTS];

    bool dirtyFlags[NUM_POTS]; // pots that moved enough to matter

    void selectMuxBank(uint8_t bank); // Drive the primary mux address lines
    void selectPotBank(uint8_t pot);  // Drive the secondary mux address lines

    // Callback for sending MIDI messages.
    // Args: CC number, mapped MIDI value, smoothed ADC reading, slot index.
    std::function<void(uint8_t, uint8_t, uint16_t, uint8_t)> midiCallback;

    // Helper for filtered analog reads
    // Grabs several fast ADC samples, averages them, then later code runs an EWMA
    // over the result. It's a cheap low-pass filter: jagged noise gets smashed,
    // but every extra layer of smoothing adds a few milliseconds of lag.
    int readAnalogFiltered(uint8_t pin); // Low-pass filtered ADC read

    int argEnvA = -1;
    int argEnvB = -1;
    void syncChannelCacheFromConfig();

  public:
    /*
    Construct the manager with the mux address pin arrays and analog input.
    */
    PotentiometerManager(const uint8_t *primaryPins, const uint8_t *secondaryPins,
                         uint8_t analogPin);

    // Link the manager with ConfigManager so channels/CCs stay in sync.
    void attachConfigManager(ConfigManager &cfg);

    /*
    Register a callback to send MIDI when a pot changes.
    The callback receives the slot's CC number, the mapped MIDI value,
    the smoothed ADC reading (0-1023-ish depending on calibration), and
    the slot index. If you need the MIDI channel, grab it from your slot
    configuration rather than expecting it here.
    */
    void setMidiCallback(std::function<void(uint8_t /*ccNumber*/, uint8_t /*mappedValue*/,
                                            uint16_t /*smoothedAdc*/, uint8_t /*slotIndex*/
                                            )>
                             callback);

    // Read pot/channel settings from EEPROM.
    void loadFromEEPROM();
    // Persist current pot settings to EEPROM.
    void saveToEEPROM();
    // Reset EEPROM mappings to defaults.
    void resetEEPROM();

    // Return the last raw value read from a pot.
    int getLastValue(int potIndex) const;

    // Set the MIDI channel for a pot.
    void setChannel(int potIndex, uint8_t channel);
    void setChannelLive(int potIndex, uint8_t channel);
    // Set the CC number for a pot.
    void setCCNumber(int potIndex, uint8_t ccNumber);
    void setCCNumberLive(int potIndex, uint8_t ccNumber);

    uint8_t getChannel(int potIndex);
    uint8_t getCCNumber(int potIndex);

    // Inject a host-driven value. The physical pot regains control only after
    // crossing that value, preventing the next mux scan from causing a jump.
    void injectMidiValue(uint8_t potIndex, uint8_t midiValue, bool emitCallback = true);

    /*
    Read every pot via the muxes and invoke the MIDI callback for changes.
    Also updates LEDs and envelope followers.
    */
    void processPots(LedAnimator &ledAnimator, std::vector<EnvelopeFollower> &envelopes);

    // Specify the envelope pair used for ARG operations.
    void setArgEnvelopePair(int a, int b);

    // Retrieve the current envelope pair selection.
    void getArgEnvelopePair(int &a, int &b) const;

    // Direct raw ADC read helper used in tests.
    int readRawPot(uint8_t potIndex);
};

#endif // POTENTIOMETER_MANAGER_H
