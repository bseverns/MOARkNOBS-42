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

// This source collects every bit of shared state that used to live in firmware_main.cpp.
// Keeping the definitions here keeps the globals close to their comments while `firmware_main.cpp`
// becomes a clean bootstrapper. The header `FirmwareState.h` exposes references so other modules
// can reach the same instances without replicating the constructors.
//
// Keep this file aligned with FirmwareState.h. A good reading rhythm is:
// declaration in FirmwareState.h -> definition here -> first runtime use in
// Runtime.cpp, UI.cpp, or the protocol handlers.

// 1. Slot-routing tables and shared caches.
std::vector<uint8_t> potChannels;
std::map<int, MIDISlot::EfSettings> potToEnvelopeMap;

// 2. Long-lived managers and services instantiated once for the life of the board.
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

// 3. Front-panel input wiring and the managers that read it.
const uint8_t controlPins[NUM_CONTROL_BUTTONS] = {12, 13, 14, 15, 24, 25};
PotentiometerManager potentiometerManager(primaryMuxPins, secondaryMuxPins, potMuxAnalogPin);
ButtonManager buttonManager(hwConfig, controlPins, &potentiometerManager);

// 4. Signal-processing runtime state: follower instances plus cached telemetry.
std::vector<EnvelopeFollower> envelopeFollowers = {
    EnvelopeFollower(A0, &potentiometerManager, 0), EnvelopeFollower(A1, &potentiometerManager, 1),
    EnvelopeFollower(A2, &potentiometerManager, 2), EnvelopeFollower(A3, &potentiometerManager, 3),
    EnvelopeFollower(A6, &potentiometerManager, 4), EnvelopeFollower(A7, &potentiometerManager, 5),
};
std::array<int, NUM_ENVELOPES> envelopeFollowerLevels{};
std::array<bool, NUM_ENVELOPES> envelopeFollowerReady{};

// 5. On-device UI and runtime control state shared across OLED, buttons, and host telemetry.
uint8_t activePot = 0;
uint8_t activeChannel = 1;
bool envelopeFollowMode = false;
String g_envelopeModeLabel = "LINEAR";
const char *envelopeMode = g_envelopeModeLabel.c_str();
int NORMAL_DISPLAY_TIME = 30000;
int SHORT_DISPLAY_TIME = 10000;
bool diagnosticMode = false;
uint8_t diagnosticPage = 0;

// 6. Timing scratch values used by runtime diagnostics and load monitoring.
unsigned long lastMIDIProcess = 0;
unsigned long lastSerialProcess = 0;
unsigned long lastLEDUpdate = 0;
unsigned long lastEnvelopeProcess = 0;
unsigned long lastDisplayUpdate = 0;

// 7. Shared context object passed into ButtonManager so button handlers mutate the same state the
// rest of the runtime sees.
ButtonManagerContext buttonContext = {potChannels,        activePot,      activeChannel,
                                      envelopeFollowMode, envelopeMode,   configManager,
                                      ledManager,         displayManager, envelopeFollowers,
                                      potToEnvelopeMap,   diagnosticMode, diagnosticPage};

// 8. Diagnostics snapshot helper used by UI/reporting code so it can inspect counters without
// racing the ISR and service loops that update them.
SystemDiagnostics captureDiagnosticsSnapshot() {
    SystemDiagnostics snapshot;
    noInterrupts();
    snapshot = g_systemDiagnostics;
    interrupts();
    return snapshot;
}
