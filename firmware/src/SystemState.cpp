#include "FirmwareState.h"

#include <Arduino.h>
#include <array>
#include <map>
#include <vector>

#include "Globals.h"
#include "ButtonManager.h"
#include "PotentiometerManager.h"
#include "LEDManager.h"
#include "LedAnimator.h"
#include "DisplayManager.h"
#include "ConfigManager.h"
#include "MIDIHandler.h"
#include "BiquadFilter.h"
#include "Arpeggiator.h"
#include "LFO/LFOManager.h"
#include "EnvelopeFollower.h"
#include "PerlinNoise.h"
#include "Utility.h"

#if defined(ARDUINO)
struct HardwareConfigInitializer {
    HardwareConfigInitializer() { loadHardwareConfig(); }
} _hwInit;
#endif

// This source collects every bit of shared state that used to live in firmware_main.cpp.
// Keeping the definitions here keeps the globals close to their comments while `firmware_main.cpp`
// becomes a clean bootstrapper. The header `FirmwareState.h` exposes references so other modules
// can reach the same instances without replicating the constructors.

// Shared routing tables that every slot reader/writer expects to reach.
std::vector<uint8_t> potChannels;
std::map<int, MIDISlot::EfSettings> potToEnvelopeMap;

// Hardware managers that stay alive for the lifetime of the firmware.
MIDIHandler midiHandler;
LEDManager ledManager(hwConfig);
LedAnimator ledAnimator(ledManager);
DisplayManager displayManager(SSD1306_I2C_ADDRESS, 128, 64);
ConfigManager configManager(NUM_POTS, NUM_BUTTONS);
BiquadFilter filter;
TaskScheduler scheduler;
Arpeggiator arpeggiator;
LFOManager lfoManager;
std::array<EfVoice, NUM_SLOTS> efVoices;

// GPIO wiring for the front-panel controls.
const uint8_t controlPins[NUM_CONTROL_BUTTONS] = {12, 13, 14, 15, 24, 25};
PotentiometerManager potentiometerManager(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin);
ButtonManager buttonManager(hwConfig, controlPins, &potentiometerManager);

// Envelope follower instances and their cached telemetry.
std::vector<EnvelopeFollower> envelopeFollowers = {
    EnvelopeFollower(A0, &potentiometerManager, 0), EnvelopeFollower(A1, &potentiometerManager, 1),
    EnvelopeFollower(A2, &potentiometerManager, 2), EnvelopeFollower(A3, &potentiometerManager, 3),
    EnvelopeFollower(A6, &potentiometerManager, 4), EnvelopeFollower(A7, &potentiometerManager, 5),
};
std::array<int, NUM_ENVELOPES> envelopeFollowerLevels{};
std::array<bool, NUM_ENVELOPES> envelopeFollowerReady{};

// Runtime UI knobs shared across the OLED, button context, and WebSerial.
uint8_t activePot = 0xFF;
uint8_t activeChannel = 1;
bool envelopeFollowMode = false;
String g_envelopeModeLabel = "LINEAR";
const char *envelopeMode = g_envelopeModeLabel.c_str();
int NORMAL_DISPLAY_TIME = 30000;
int SHORT_DISPLAY_TIME = 10000;
bool diagnosticMode = false;
uint8_t diagnosticPage = 0;

// Timing reports used by `monitorSystemLoad()` and diagnostics.
unsigned long lastMIDIProcess = 0;
unsigned long lastSerialProcess = 0;
unsigned long lastLEDUpdate = 0;
unsigned long lastEnvelopeProcess = 0;
unsigned long lastDisplayUpdate = 0;

// Shared context object passed to `ButtonManager` so button handlers can mutate UI state.
ButtonManagerContext buttonContext = {potChannels,        activePot,      activeChannel,
                                      envelopeFollowMode, envelopeMode,   configManager,
                                      ledManager,         displayManager, envelopeFollowers,
                                      potToEnvelopeMap,   diagnosticMode, diagnosticPage};

SystemDiagnostics captureDiagnosticsSnapshot() {
    SystemDiagnostics snapshot;
    noInterrupts();
    snapshot = g_systemDiagnostics;
    interrupts();
    return snapshot;
}
