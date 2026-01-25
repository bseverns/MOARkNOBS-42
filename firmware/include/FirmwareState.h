#ifndef FIRMWARE_STATE_H
#define FIRMWARE_STATE_H

#include <Arduino.h>
#include <array>
#include <map>
#include <queue>
#include <vector>

#include "Globals.h"
#include "MIDITypes.h"
#include "EfVoice.h"

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

extern std::vector<uint8_t> potChannels;
extern std::map<int, MIDISlot::EfSettings> potToEnvelopeMap;
extern std::queue<String> commandQueue;
extern MIDIHandler midiHandler;
extern LEDManager ledManager;
extern LedAnimator ledAnimator;
extern DisplayManager displayManager;
extern ConfigManager configManager;
extern BiquadFilter filter;
extern TaskScheduler scheduler;
extern Arpeggiator arpeggiator;
extern LFOManager lfoManager;
extern std::array<EfVoice, NUM_SLOTS> efVoices;
extern PotentiometerManager potentiometerManager;
extern ButtonManager buttonManager;
extern std::vector<EnvelopeFollower> envelopeFollowers;
extern std::array<int, NUM_ENVELOPES> envelopeFollowerLevels;
extern std::array<bool, NUM_ENVELOPES> envelopeFollowerReady;
extern std::array<float, NUM_ENVELOPES> efBaseGains;
extern uint8_t activePot;
extern uint8_t activeChannel;
extern ButtonManagerContext buttonContext;
SystemDiagnostics captureDiagnosticsSnapshot();
extern bool envelopeFollowMode;
extern String g_envelopeModeLabel;
extern const char *envelopeMode;
extern int NORMAL_DISPLAY_TIME;
extern int SHORT_DISPLAY_TIME;
extern bool diagnosticMode;
extern uint8_t diagnosticPage;

#endif // FIRMWARE_STATE_H
