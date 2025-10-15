#include "Globals.h"
#include "ConfigManager.h"
#include "PotentiometerManager.h"
#include "EnvelopeFollower.h"
#include "interop/SeedBoxLink.h"
#include "PerlinNoise.h"

#include <array>
#include <cmath>
#include <vector>

// Hardware snapshot so the tests can lean on mux pin definitions without
// hauling in the full Globals.cpp (and its SD/Serial baggage).
HardwareConfig hwConfig = {0, 0, 0, 16, 6, 8, NUM_BUTTONS, 1, 1, 1, 1, {2, 3, 4, 5},
                           {6, 7, 8, 9}, A0, A1, A2};

// Globals that the firmware normally defines in Globals.cpp.
uint32_t g_resetCause = 0;
uint16_t g_brownoutCount = 0;
float g_tappedBPM = 120.0f;
bool g_clockOutEnabled = false;
bool g_usbMidiOutEnabled = false;
unsigned long lastClockTime = 0;
float g_vref = 3.3f;
EnvelopeConfig envelopeConfig = {};
std::vector<EnvelopeFollower> envelopeFollowers;
int NORMAL_DISPLAY_TIME = 0;
int SHORT_DISPLAY_TIME = 0;
int8_t velocityShift = 0;
uint8_t changeProbability = 100;

const std::array<std::pair<int, int>, ARG_PAIR_COUNT> ARG_PAIRS = {};
const size_t ARG_PAIRS_LEN = ARG_PAIRS.size();

// ConfigManager normally gets its constructor from ConfigManager.cpp, but the
// Unity slice doesn't compile that translation unit. Provide a skinny version
// that keeps the tests humming without poking EEPROM.
ConfigManager::ConfigManager(uint8_t numPots, uint8_t numButtons)
    : _numPots(numPots), _numButtons(numButtons) {
    for (auto &slot : slots) {
        slot.active = false;
        slot.type = MIDIMessageType::OFF;
        slot.midiChannel = 1;
        slot.data1 = 0;
        slot.arpNote = 0;
    }
    _stored.potChannels.fill(0);
    _stored.potCCNumbers.fill(0);
    _stored.version = CONFIG_VERSION;
    _stored.crc = 0;
}

// Ditto for PotentiometerManager: we just need a predictable sandbox for
// potLastValues, so the constructor skips any real hardware twiddling.
PotentiometerManager::PotentiometerManager(const uint8_t *primaryPins,
                                           const uint8_t *secondaryPins,
                                           uint8_t analog)
    : primaryMuxPins(primaryPins), secondaryMuxPins(secondaryPins),
      analogPin(analog) {
    for (uint8_t i = 0; i < NUM_POTS; ++i) {
        potChannels[i] = 1;
        potCCNumbers[i] = i;
        potLastValues[i] = 0;
        smoothedValue[i] = 0;
        dirtyFlags[i] = false;
    }
}

int PotentiometerManager::getLastValue(int potIndex) const {
    if (potIndex < 0 || potIndex >= static_cast<int>(NUM_POTS))
        return -1;
    return potLastValues[potIndex];
}

// The arpeggiator leans on Perlin noise for its RANDOM mode. The full noise
// implementation is overkill for these unit tests, so collapse it to a simple
// deterministic ramp that still exercises the call path.
float perlinNoise1D(float x) {
    // Keep the output in [-1, 1] while providing repeatable variation.
    float phase = std::fmod(x, 8.0f) / 8.0f;
    return 2.0f * phase - 1.0f;
}

namespace seedbox {
namespace interop {
namespace mn42 {

SeedBoxLink &SeedBoxLink::instance() {
    static SeedBoxLink link;
    return link;
}

void SeedBoxLink::begin(::MIDIHandler *handler) {
    _midi = handler;
    _hasAck = false;
}

void SeedBoxLink::update() {}

bool SeedBoxLink::handleControlChange(uint8_t, uint8_t, uint8_t) {
    return false;
}

void SeedBoxLink::handleSysEx(const uint8_t *, uint16_t) {}

bool SeedBoxLink::peerAlive() const { return false; }

void SeedBoxLink::sendHello() {}
void SeedBoxLink::sendAck() {}
void SeedBoxLink::sendKeepAlive() {}
void SeedBoxLink::sendIdentityPing() {}
void SeedBoxLink::markPeerPulse() {}

} // namespace mn42
} // namespace interop
} // namespace seedbox
