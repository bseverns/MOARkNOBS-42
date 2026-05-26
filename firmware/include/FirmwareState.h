#ifndef FIRMWARE_STATE_H
#define FIRMWARE_STATE_H

#include <Arduino.h>
#include <array>
#include <map>
#include <vector>

#include "Globals.h"
#include "MIDITypes.h"
#include "EfVoice.h"

// FirmwareState.h is the "living machine" header.
//
// If Globals.h answers "what hardware and persisted facts exist?", this file
// answers "which runtime objects currently embody those facts?" New readers
// arriving from firmware_main.cpp should treat this as the inventory of the
// always-on actors: config, MIDI, LEDs, pots, followers, display, scheduler,
// and the small pile of live UI/runtime flags that bind them together.

class ConfigManager;
class DisplayManager;
class LEDManager;
class PotentiometerManager;
class ButtonManager;
class EnvelopeFollower;
class Arpeggiator;
class LFOManager;
class BiquadFilter;
class MIDIHandler;
class String;
class TaskScheduler;
class LedAnimator;
struct ButtonManagerContext;

// Definitions for these externs live in src/SystemState.cpp.
//
// Read top-to-bottom:
// 1. routing tables and small shared caches
// 2. long-lived managers that own the instrument
// 3. signal-processing runtime state
// 4. UI/runtime control state
// 5. diagnostics snapshots used by displays and host tools

// Slot-routing tables and small shared caches.
extern std::vector<uint8_t> potChannels;
extern std::map<int, MIDISlot::EfSettings> potToEnvelopeMap;

// Long-lived managers and services instantiated once for the life of the board.
extern ConfigManager configManager;
extern MIDIHandler midiHandler;
extern LEDManager ledManager;
extern LedAnimator ledAnimator;
extern DisplayManager displayManager;
extern PotentiometerManager potentiometerManager;
extern ButtonManager buttonManager;
extern BiquadFilter filter;
extern TaskScheduler scheduler;
extern Arpeggiator arpeggiator;
extern LFOManager lfoManager;

// Signal-processing runtime state: voices, followers, and follower telemetry.
extern std::array<EfVoice, NUM_SLOTS> efVoices;
extern std::vector<EnvelopeFollower> envelopeFollowers;
extern std::array<int, NUM_ENVELOPES> envelopeFollowerLevels;
extern std::array<bool, NUM_ENVELOPES> envelopeFollowerReady;
extern std::array<float, NUM_ENVELOPES> efBaseGains;

// On-device UI and runtime control state shared across managers.
extern uint8_t activePot;
extern uint8_t activeChannel;
extern bool envelopeFollowMode;
extern String g_envelopeModeLabel;
extern const char *envelopeMode;
extern int NORMAL_DISPLAY_TIME;
extern int SHORT_DISPLAY_TIME;
extern bool diagnosticMode;
extern uint8_t diagnosticPage;
extern ButtonManagerContext buttonContext;

// Diagnostics view: copy the live counters into a stable snapshot for UI/reporting code.
SystemDiagnostics captureDiagnosticsSnapshot();

#endif // FIRMWARE_STATE_H
