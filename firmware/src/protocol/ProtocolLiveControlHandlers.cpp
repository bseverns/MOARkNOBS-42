#include "protocol/ProtocolLiveControlHandlers.h"

#include <Arduino.h>
#include <algorithm>

#include "Arpeggiator.h"
#include "BoardPowerProfile.h"
#include "ConfigManager.h"
#include "EfSettingsUtils.h"
#include "EnvelopeFollower.h"
#include "FirmwareState.h"
#include "Globals.h"
#include "Log.h"
#include "MIDIHandler.h"
#include "Modes.h"

// Direct, non-persistent runtime mutations. Protocol.cpp remains the routing
// boundary; this module owns parsing and applying the SET_* live-control lane.
namespace ProtocolLiveControlHandlers {
namespace {
const char *arpShapeName(Arpeggiator::Shape shape) {
    switch (shape) {
    case Arpeggiator::UP: return "up";
    case Arpeggiator::DOWN: return "down";
    case Arpeggiator::UPDOWN: return "up_down";
    case Arpeggiator::RANDOM: return "random";
    case Arpeggiator::DRUNK: return "drunk";
    case Arpeggiator::EUCLIDEAN: return "euclidean";
    }
    return "up";
}
} // namespace

// 4. Direct live-control writes.
void handleSetArgMethodCommand(const String &command) {
    int method = command.substring(14).toInt();
    if (method >= 0 && method <= static_cast<int>(ARGMethod::XORR)) {
        for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; ++slotIndex) {
            MIDISlot &slot = configManager.getSlot(slotIndex);
            slot.arg.method = static_cast<ARGMethod>(method);
        }
        configManager.setARGMethodLive(static_cast<uint8_t>(method));
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\",\"persisted\":false}");
    } else {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
    }
}

void handleSetArpCommand(const String &command) {
    int firstComma = command.indexOf(',');
    int secondComma = command.indexOf(',', firstComma + 1);
    int thirdComma = command.indexOf(',', secondComma + 1);
    int fourthComma = command.indexOf(',', thirdComma + 1);
    int fifthComma = command.indexOf(',', fourthComma + 1);
    int sixthComma = command.indexOf(',', fifthComma + 1);
    if (firstComma < 0 || secondComma < 0 || thirdComma < 0 || fourthComma < 0 || fifthComma < 0) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"command\":\"SET_ARP\","
                    "\"message\":\"missing values\"}");
        return;
    }

    const uint8_t lengthTicks =
        static_cast<uint8_t>(constrain(command.substring(firstComma + 1, secondComma).toInt(), 1,
                                       static_cast<int>(Arpeggiator::MAX_LENGTH)));
    const uint8_t shape =
        static_cast<uint8_t>(constrain(command.substring(secondComma + 1, thirdComma).toInt(), 0,
                                       static_cast<int>(Arpeggiator::EUCLIDEAN)));
    const float swingPercent =
        constrain(command.substring(thirdComma + 1, fourthComma).toFloat(), 0.0f, 80.0f);
    const float gatePercent =
        constrain(command.substring(fourthComma + 1, fifthComma).toFloat(), 5.0f, 100.0f);
    const int rawOctaveRange = sixthComma < 0
                                   ? command.substring(fifthComma + 1).toInt()
                                   : command.substring(fifthComma + 1, sixthComma).toInt();
    const uint8_t octaveRange = static_cast<uint8_t>(constrain(rawOctaveRange, 0, 3));
    const uint8_t patternLength =
        sixthComma < 0
            ? arpeggiator.getPatternLength()
            : static_cast<uint8_t>(constrain(command.substring(sixthComma + 1).toInt(),
                                             static_cast<int>(Arpeggiator::MIN_PATTERN_LENGTH),
                                             static_cast<int>(Arpeggiator::MAX_PATTERN_LENGTH)));

    arpeggiator.setLength(lengthTicks);
    arpeggiator.setShape(static_cast<Arpeggiator::Shape>(shape));
    arpeggiator.setSwingPercent(swingPercent);
    arpeggiator.setGatePercent(gatePercent);
    arpeggiator.setOctaveRange(octaveRange);
    arpeggiator.setPatternLength(patternLength);

    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"SET_ARP\","
               "\"active\":%s,\"slot\":%u,\"length_ticks\":%u,\"shape\":%u,"
               "\"shape_name\":\"%s\",\"swing_percent\":%u,\"gate_percent\":%u,"
               "\"octave_range\":%u,\"pattern_length\":%u,\"persisted\":%s}\n",
               arpeggiator.isActive() ? "true" : "false",
               static_cast<unsigned>(arpeggiator.getSlot()), static_cast<unsigned>(lengthTicks),
               static_cast<unsigned>(shape), arpShapeName(static_cast<Arpeggiator::Shape>(shape)),
               static_cast<unsigned>(constrain(swingPercent, 0.0f, 80.0f)),
               static_cast<unsigned>(constrain(gatePercent, 5.0f, 100.0f)),
               static_cast<unsigned>(octaveRange),
               static_cast<unsigned>(arpeggiator.getPatternLength()), "false");
}

void handleSetClockCommand(const String &command) {
    int firstComma = command.indexOf(',');
    int secondComma = command.indexOf(',', firstComma + 1);
    int thirdComma = command.indexOf(',', secondComma + 1);
    if (firstComma < 0 || secondComma < 0 || thirdComma < 0) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"command\":\"SET_CLOCK\","
                    "\"message\":\"missing values\"}");
        return;
    }

    const bool followExternal = command.substring(firstComma + 1, secondComma).toInt() != 0;
    const bool clockOutEnabled = command.substring(secondComma + 1, thirdComma).toInt() != 0;
    const float tappedBpm = constrain(command.substring(thirdComma + 1).toFloat(), 20.0f, 300.0f);

    g_followExternalClock = followExternal;
    g_clockOutEnabled = clockOutEnabled;
    g_tappedBPM = tappedBpm;

    const bool externalSignal = midiHandler.hasExternalClockSignal();
    const bool running = midiHandler.isClockRunning();
    const float externalBpm = midiHandler.externalClockBpm();
    const char *source = "idle";
    if (g_followExternalClock && externalSignal) {
        source = "external";
    } else if (g_tappedBPM > 0.0f) {
        source = "internal";
    }

    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"SET_CLOCK\","
               "\"follow_external\":%s,\"clock_out_enabled\":%s,\"tapped_bpm\":%.2f,"
               "\"external_bpm\":%.2f,\"external_signal\":%s,\"running\":%s,"
               "\"source\":\"%s\",\"persisted\":%s}\n",
               g_followExternalClock ? "true" : "false", g_clockOutEnabled ? "true" : "false",
               static_cast<double>(g_tappedBPM), static_cast<double>(externalBpm),
               externalSignal ? "true" : "false", running ? "true" : "false", source,
               "false");
}

void handleSetEfCommand(const String &command) {
    int comma = command.indexOf(',');
    if (comma == -1) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
        return;
    }
    int potIndex = command.substring(7, comma).toInt();
    int envIndex = command.substring(comma + 1).toInt();
    if (potIndex >= 0 && potIndex < NUM_POTS && envIndex >= 0 &&
        envIndex < static_cast<int>(envelopeFollowers.size())) {
        MIDISlot &slot = configManager.getSlot(static_cast<uint8_t>(potIndex));
        slot.setEnvelopeFollowerIndex(static_cast<int8_t>(envIndex));
        potToEnvelopeMap[potIndex] = slot.efSettings;
        envelopeFollowers[envIndex].toggleActive(true);
        applyEfSettingsToFollower(envelopeFollowers[envIndex], slot.efSettings,
                                  static_cast<uint8_t>(envIndex));
        refreshEfVoicesFromConfig();
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\",\"persisted\":false}");
    } else {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
    }
}

void handleSetLedCommand(const String &command) {
    int first = command.indexOf(',');
    int second = command.indexOf(',', first + 1);
    int third = command.indexOf(',', second + 1);
    if (first == -1 || second == -1 || third == -1) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
        return;
    }
    int brightness = command.substring(8, first).toInt();
    int r = command.substring(first + 1, second).toInt();
    int g = command.substring(second + 1, third).toInt();
    int b = command.substring(third + 1).toInt();
    if (brightness >= 0 && brightness <= 255 && r >= 0 && r <= 255 && g >= 0 && g <= 255 &&
        b >= 0 && b <= 255) {
        CRGB color(r, g, b);
        brightness = std::min<int>(brightness, BoardPowerProfile::kLedBrightnessCap);
        ledManager.setBrightness(static_cast<uint8_t>(brightness));
        ledManager.setColor(color);
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\",\"persisted\":false}");
    } else {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
    }
}

void handleSetJitterCommand(const String &command) {
    int firstComma = command.indexOf(',');
    int secondComma = command.indexOf(',', firstComma + 1);
    if (firstComma < 0 || secondComma < 0) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"command\":\"SET_JITTER\","
                    "\"message\":\"missing values\"}");
        return;
    }

    const float depth =
        constrain(command.substring(firstComma + 1, secondComma).toFloat(), 0.0f, 1.0f);
    const float smoothness = constrain(command.substring(secondComma + 1).toFloat(), 0.0f, 1.0f);
    g_jitterSettings.depth = depth;
    g_jitterSettings.smoothness = smoothness;
    g_jitterRemoteControlActive = true;
    g_jitterDepthLatched = false;
    g_jitterSmoothnessLatched = false;
    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"SET_JITTER\",\"depth\":%.3f,"
               "\"smoothness\":%.3f,\"persisted\":%s}\n",
               static_cast<double>(depth), static_cast<double>(smoothness),
               "false");
}

void handleSetPotCommand(const String &command) {
    int firstComma = command.indexOf(',');
    int lastComma = command.lastIndexOf(',');
    if (firstComma == -1 || lastComma == -1 || firstComma == lastComma) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"message\":\"Malformed SET_POT "
                    "command\"}");
        return;
    }
    int potIndex = command.substring(8, firstComma).toInt();
    int channel = command.substring(firstComma + 1, lastComma).toInt();
    int ccNumber = command.substring(lastComma + 1).toInt();
    if (potIndex >= 0 && potIndex < NUM_POTS && channel >= 1 && channel <= 16 && ccNumber >= 0 &&
        ccNumber <= 127) {
        configManager.setPotChannelLive(potIndex, channel);
        configManager.setPotCCNumberLive(potIndex, ccNumber);
        potentiometerManager.setChannelLive(potIndex, channel);
        potentiometerManager.setCCNumberLive(potIndex, ccNumber);
        if (static_cast<size_t>(potIndex) < potChannels.size()) {
            potChannels[potIndex] = channel;
        }
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\",\"persisted\":false,"
                    "\"message\":\"Pot configuration updated in live state.\"}");
    } else {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"message\":\"Invalid values for "
                    "SET_POT\"}");
    }
}

void handleSetNoteDynamicsCommand(const String &command) {
    int firstComma = command.indexOf(',');
    int secondComma = command.indexOf(',', firstComma + 1);
    if (firstComma < 0 || secondComma < 0) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"command\":\"SET_NOTE_DYNAMICS\","
                    "\"message\":\"missing values\"}");
        return;
    }

    const int velocity = constrain(command.substring(firstComma + 1, secondComma).toInt(), -64, 63);
    const int probability = constrain(command.substring(secondComma + 1).toInt(), 0, 100);
    velocityShift = static_cast<int8_t>(velocity);
    changeProbability = static_cast<uint8_t>(probability);
    g_noteDynamicsRemoteControlActive = true;
    g_noteDynamicsShiftLatched = false;
    g_noteDynamicsProbabilityLatched = false;
    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"SET_NOTE_DYNAMICS\","
               "\"velocity_shift\":%d,\"change_probability\":%u,\"persisted\":%s}\n",
               velocity, static_cast<unsigned>(changeProbability), "false");
}

void handleSetUsbMidiCommand(const String &command) {
    int comma = command.indexOf(',');
    if (comma < 0) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\",\"command\":\"SET_USB_MIDI\","
                    "\"message\":\"missing value\"}");
        return;
    }

    String valueText = command.substring(comma + 1);
    valueText.trim();
    configManager.setUsbMidiOutEnabledLive(valueText.toInt() != 0);
    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"SET_USB_MIDI\","
               "\"usb_midi_out\":%s,\"persisted\":false}\n",
               g_usbMidiOutEnabled ? "true" : "false");
}

void handleSetSlotValueCommand(const String &command) {
    int firstComma = command.indexOf(',');
    int lastComma = command.lastIndexOf(',');
    if (firstComma == -1 || lastComma == -1 || firstComma == lastComma) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
        return;
    }

    int slotIndex = command.substring(firstComma + 1, lastComma).toInt();
    int midiValue = command.substring(lastComma + 1).toInt();
    if (slotIndex < 0 || slotIndex >= NUM_SLOTS || midiValue < 0 || midiValue > 127) {
        LOG_PRINTLN("{\"type\":\"response\",\"status\":\"error\"}");
        return;
    }

    potentiometerManager.injectMidiValue(static_cast<uint8_t>(slotIndex),
                                         static_cast<uint8_t>(midiValue));
    LOG_PRINTLN("{\"type\":\"response\",\"status\":\"ok\"}");
}
} // namespace ProtocolLiveControlHandlers
