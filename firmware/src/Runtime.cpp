#include "Runtime.h"

#include <Arduino.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <imxrt.h>
#include "ButtonManager.h"
#include "ConfigManager.h"
#include "DisplayManager.h"
#include "LEDManager.h"
#include "MIDIHandler.h"
#include "PotentiometerManager.h"
#include "Arpeggiator.h"
#include "Utility.h"
#include "TimeUtils.h"
#include "Log.h"
#include "Globals.h"
#include "LFO/LFOManager.h"
#include "ARGMixer.h"
#include "Protocol.h"
#include "UI.h"
#include "Scheduler.h"
#include "interop/SeedBoxLink.h"

#include <EEPROM.h>
#include <TimerOne.h>
namespace {

volatile uint8_t midiServiceRequestCount = 0;
constexpr uint8_t kMaxMidiServiceRequestCount = 0xFF;

struct PendingNoteOff {
    uint32_t dueTime = 0;
    uint8_t note = 0;
    uint8_t channel = 0;
    bool active = false;
};

constexpr size_t kPendingNoteOffCapacity = 32;
std::array<PendingNoteOff, kPendingNoteOffCapacity> pendingNoteOffs{};

void queueMidiServiceRequest() {
    // ISR-safe flagging: keep the interrupt short and let `processMIDI()` grab the work later.
    if (midiServiceRequestCount < kMaxMidiServiceRequestCount) {
        ++midiServiceRequestCount;
    }
}

bool consumeMidiServiceRequest() {
    bool pending = false;
    noInterrupts();
    if (midiServiceRequestCount > 0) {
        pending = true;
        midiServiceRequestCount = 0;
    }
    interrupts();
    return pending;
}

} // namespace

namespace {
volatile unsigned long statusLedPulseDeadline = 0;
} // namespace

void requestStatusLEDPulse(uint16_t durationMs) {
    // Blink the status LED from any task as diagnostics fire.
    statusLedPulseDeadline = now() + durationMs;
}

void serviceStatusLEDPulse() {
    unsigned long deadline = statusLedPulseDeadline;
    if (deadline == 0) {
        ledManager.setStatusLED(false);
        return;
    }
    unsigned long current = now();
    if (static_cast<long>(current - deadline) >= 0) {
        statusLedPulseDeadline = 0;
        ledManager.setStatusLED(false);
    } else {
        ledManager.setStatusLED(true);
    }
}

void checkDiagnosticsForAlerts() {
    // Report any diagnostic counters over Serial and pulse the status LED so students can spot
    // trouble.
    static uint32_t lastMidiDrop = 0;
    static uint32_t lastMidiTaskOverrun = 0;
    static uint32_t lastUartOverrun = 0;

    const SystemDiagnostics diag = captureDiagnosticsSnapshot();

    const uint32_t midiDrop = diag.midiDropCount;
    if (midiDrop != lastMidiDrop) {
        LOG_PRINTF("{\"diagnostic\":\"midi_drop\",\"count\":%lu}\n",
                   static_cast<unsigned long>(midiDrop));
        requestStatusLEDPulse();
        lastMidiDrop = midiDrop;
    }

    const uint32_t midiTaskOverruns = diag.midiTaskOverrunCount;
    const uint32_t maxMidiMicros = diag.maxProcessMidiMicros;
    if (midiTaskOverruns != lastMidiTaskOverrun) {
        LOG_PRINTF("{\"diagnostic\":\"midi_task_overrun\",\"count\":%lu,\"max_us\":%lu}\n",
                   static_cast<unsigned long>(midiTaskOverruns),
                   static_cast<unsigned long>(maxMidiMicros));
        requestStatusLEDPulse();
        lastMidiTaskOverrun = midiTaskOverruns;
    }

    const uint32_t uartOverruns = diag.uartOverrunCount;
    if (uartOverruns != lastUartOverrun) {
        LOG_PRINTF("{\"diagnostic\":\"uart_overrun\",\"count\":%lu}\n",
                   static_cast<unsigned long>(uartOverruns));
        requestStatusLEDPulse();
        lastUartOverrun = uartOverruns;
    }
}

void initializeRuntime(bool baselinesLoaded) {
    // Initialize runtime services now that the UI and EEPROM-backed state are ready.
    pinMode(VREF_ADC_PIN, INPUT);
    g_vref = Utility::readVrefADC(VREF_ADC_PIN);

    midiHandler.begin();
    midiHandler.setDiagnostics(&g_systemDiagnostics);
    midiHandler.setDisplayManager(&displayManager);
    seedbox::interop::mn42::SeedBoxLink::instance().begin(&midiHandler);
    lfoManager.attachMIDI(&midiHandler);

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
                uint8_t velo = 125;
                if (potIdx < efVoices.size() && efVoices[potIdx].hasRendered()) {
                    velo = efVoices[potIdx].latestLevel();
                } else if (slot.ef.followerIndex >= 0 &&
                           static_cast<size_t>(slot.ef.followerIndex) < envelopeFollowers.size()) {
                    velo = static_cast<uint8_t>(
                        constrain(envelopeFollowers[static_cast<size_t>(slot.ef.followerIndex)]
                                      .getEnvelopeLevel(),
                                  0, 127));
                }
                int shifted = velo + velocityShift;
                if (shifted < 0)
                    shifted = 0;
                if (shifted > 127)
                    shifted = 127;
                if (random(100U) >= changeProbability)
                    break;
                midiHandler.sendNoteOn(note, shifted, slot.midiChannel);
                queuePendingNoteOff(note, slot.midiChannel, 100);
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
                uint16_t param = static_cast<uint16_t>(slot.data1) << 7;
                uint16_t val = static_cast<uint16_t>(Utility::mapToMidiValue(rawValue)) << 7;
                midiHandler.sendNRPN(param, val, slot.midiChannel);
                break;
            }

            case MIDIMessageType::RPN: {
                uint16_t param = static_cast<uint16_t>(slot.data1) << 7;
                uint16_t val = static_cast<uint16_t>(Utility::mapToMidiValue(rawValue)) << 7;
                midiHandler.sendRPN(param, val, slot.midiChannel);
                break;
            }

            case MIDIMessageType::SysEx: {
                std::array<uint8_t, SysExTemplate::kMaxLength> msg{};
                uint8_t length = buildSysExPayload(slot, rawValue, msg.data(), msg.size());
                if (length > 0) {
                    midiHandler.sendSysEx(msg.data(), length);
                }
                break;
            }

            default:
                break;
            }
        });

    potentiometerManager.loadFromEEPROM();

    Timer1.initialize(1000);
    Timer1.attachInterrupt(midiTimerISR);

    filter.configure(BiquadFilter::LOWPASS, 1000, 44100);

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

    for (uint8_t i = 0; i < NUM_SLOTS; i++) {
        if (potentiometerManager.getChannel(i) == 0)
            potentiometerManager.setChannel(i, 1);
        if (potentiometerManager.getCCNumber(i) > 127)
            potentiometerManager.setCCNumber(i, i % 128);
    }

    if (!configManager.loadConfiguration(potChannels)) {
        LOG_PRINTLN("EEPROM corrupted → resetting.");
        potentiometerManager.resetEEPROM();
    }

    initializeSchedulers();
}

void midiTimerISR() { queueMidiServiceRequest(); }

void processMIDI() {
    if (!consumeMidiServiceRequest()) {
        return;
    }
    midiHandler.processIncomingMIDI();

    static uint32_t lastDisplayTick = 0;
    uint32_t tickCount = midiHandler.clockTickCount();
    if (tickCount != lastDisplayTick) {
        uint32_t diff = tickCount - lastDisplayTick;
        lastDisplayTick = tickCount;

        lastClockTime = now();
        midiBeatPosition = (midiBeatPosition + diff) % 8;

        std::array<uint8_t, NUM_ENVELOPES> envelopeSnapshot{};
        for (size_t i = 0; i < NUM_ENVELOPES; ++i) {
            envelopeSnapshot[i] =
                static_cast<uint8_t>(constrain(envelopeFollowerLevels[i], 0, 127));
        }
        displayManager.updateDisplay(
            midiBeatPosition, envelopeSnapshot.data(), envelopeSnapshot.size(),
            envelopeFollowMode ? "EF ON" : "EF OFF", activePot, activeChannel, envelopeMode);

        lastClockTime = now();
        midiHandler.clearClockTick();
    }

    monitorSerialHealth();
}

void processEnvelopeFollowers() {
    float gainTrim = 1.0f + g_lfoEfGainTrim;
    gainTrim = constrain(gainTrim, 0.0f, 2.0f);
    for (size_t idx = 0; idx < envelopeFollowers.size(); ++idx) {
        EnvelopeFollower &follower = envelopeFollowers[idx];
        if (!follower.getActiveState()) {
            envelopeFollowerReady[idx] = false;
            continue;
        }
        follower.setExternalGainTrim(gainTrim);
        follower.update();
        envelopeFollowerLevels[idx] = follower.getEnvelopeLevel();
        envelopeFollowerReady[idx] = true;
    }
}

void processLFOs() {
    lfoManager.update(now());
    const LFOBus &bus = lfoManager.bus();
    g_lfoEfGainTrim = bus.efGainTrim;
    g_lfoArpSwing = bus.arpSwing;
    g_lfoLedBrightness = bus.ledBrightness;
    g_lfoValues[0] = lfoManager.normalizedValue(0);
    g_lfoValues[1] = lfoManager.normalizedValue(1);

    float brightnessScale = 1.0f + g_lfoLedBrightness;
    brightnessScale = constrain(brightnessScale, 0.0f, 2.0f);
    ledManager.setBrightnessModulator(brightnessScale);
}

bool queuePendingNoteOff(uint8_t note, uint8_t channel, unsigned long delayMs) {
    uint32_t due = now() + delayMs;
    for (auto &entry : pendingNoteOffs) {
        if (!entry.active) {
            entry.active = true;
            entry.note = note;
            entry.channel = channel;
            entry.dueTime = due;
            return true;
        }
    }
    ++g_systemDiagnostics.midiDropCount;
    requestStatusLEDPulse();
    return false;
}

void processPendingNoteOffs() {
    unsigned long current = now();
    for (auto &entry : pendingNoteOffs) {
        if (!entry.active)
            continue;
        if (static_cast<long>(current - entry.dueTime) >= 0) {
            midiHandler.sendNoteOff(entry.note, 0, entry.channel);
            entry.active = false;
        }
    }
}

void processEnvelopes() {
    static std::array<uint8_t, NUM_POTS> lastEnvelopeMidiValues;
    static bool envelopeMidiInitialized = false;
    if (!envelopeMidiInitialized) {
        lastEnvelopeMidiValues.fill(0xFF);
        envelopeMidiInitialized = true;
    }

    std::array<int, NUM_ENVELOPES> rawFollowerLevels{};
    std::array<bool, NUM_ENVELOPES> followerReady{};
    for (size_t idx = 0; idx < envelopeFollowers.size(); ++idx) {
        rawFollowerLevels[idx] = envelopeFollowerLevels[idx];
        followerReady[idx] = envelopeFollowerReady[idx];
        if (followerReady[idx]) {
            ledManager.setEnvelopeLevel(static_cast<uint8_t>(idx),
                                        constrain(rawFollowerLevels[idx], 0, 127));
        }
    }

    for (const auto &entry : potToEnvelopeMap) {
        const int potIndex = entry.first;
        const MIDISlot::EfSettings &settings = entry.second;
        const int envelopeIndex = settings.followerIndex;
        if (potIndex < 0 || potIndex >= NUM_POTS)
            continue;
        if (envelopeIndex >= 0 && envelopeIndex < static_cast<int>(envelopeFollowers.size())) {
            int currentPotReading = potentiometerManager.getLastValue(potIndex);
            if (currentPotReading < 0)
                continue;

            uint8_t baselineMidi = Utility::mapToMidiValue(currentPotReading);

            EnvelopeFollower *envelope = &envelopeFollowers[envelopeIndex];
            bool envelopeActive = envelope->getActiveState();

            if (!envelopeActive) {
                lastEnvelopeMidiValues[potIndex] = baselineMidi;
                continue;
            }

            if (!followerReady[envelopeIndex]) {
                lastEnvelopeMidiValues[potIndex] = baselineMidi;
                continue;
            }

            const MIDISlot &slot = configManager.getSlot(potIndex);
            EfVoice &voice = efVoices[potIndex];
            voice.assignFollower(envelopeIndex);
            voice.syncSettings(slot.efSettings);
            voice.render(rawFollowerLevels[envelopeIndex]);

            if (potIndex >= NUM_SLOTS)
                continue;
            uint8_t envelopeContribution = computeSlotArgLevel(slot, envelopeFollowers);
            int modulatedInt = static_cast<int>(baselineMidi) + envelopeContribution;
            uint8_t modulatedValue = static_cast<uint8_t>(constrain(modulatedInt, 0, 127));

            bool valueChanged = modulatedValue != lastEnvelopeMidiValues[potIndex];
            if (valueChanged) {
                uint8_t ccNumber = potentiometerManager.getCCNumber(potIndex);
                uint8_t channel = potentiometerManager.getChannel(potIndex);
                midiHandler.sendControlChange(ccNumber, modulatedValue, channel);
                lastEnvelopeMidiValues[potIndex] = modulatedValue;
            }

            ledManager.setPotValue(potIndex, modulatedValue);
        }
    }

    if (buttonContext.activePot < NUM_POTS) {
        int activePotReading = potentiometerManager.getLastValue(buttonContext.activePot);
        if (activePotReading >= 0) {
            uint8_t potMidiValue = Utility::mapToMidiValue(activePotReading);
            for (uint8_t i = 0; i < POT_LED_COUNT; ++i) {
                ledManager.setPotIndicator(i, potMidiValue);
            }
        }
    }
}

void processInternalClock() {
    static unsigned long lastInternalTick = 0;
    if (g_tappedBPM <= 0.0f)
        return;

    float msPerTick = 60000.0f / (g_tappedBPM * 24.0f);
    unsigned long now = ::now();
    if (now - lastInternalTick >= msPerTick) {
        lastInternalTick = now;
        lastClockTime = now;
        midiHandler.generateClockTick();
    }
}

void monitorSystemLoad() {
    static unsigned long lastMonitorTime = 0;
    static unsigned long taskCounter = 0;
    static unsigned long maxLoopDuration = 0;
    static unsigned long lastLoopStart = micros();

    unsigned long currentMicros = micros();
    unsigned long loopDuration = currentMicros - lastLoopStart;
    lastLoopStart = currentMicros;

    g_systemDiagnostics.lastLoopMicros = loopDuration;
    if (loopDuration > maxLoopDuration) {
        maxLoopDuration = loopDuration;
    }
    if (loopDuration > 1000UL) {
        ++g_systemDiagnostics.loopOverrunCount;
        LOG_PRINTF("{\"diagnostic\":\"loop_overrun\",\"duration_us\":%lu}\n",
                   static_cast<unsigned long>(loopDuration));
        requestStatusLEDPulse();
    }

    taskCounter++;
    unsigned long currentMillis = now();
    if (currentMillis - lastMonitorTime >= 1000UL) {
        LOG_PRINTF("Tasks per second: %lu\n", taskCounter);
        g_systemDiagnostics.maxLoopMicros = maxLoopDuration;
        maxLoopDuration = 0;
        taskCounter = 0;
        lastMonitorTime = currentMillis;
    }

    checkDiagnosticsForAlerts();
    serviceStatusLEDPulse();
}

void monitorSerialHealth() {
#if defined(__IMXRT1062__)
    if (IMXRT_LPUART6.STAT & LPUART_STAT_OR) {
        IMXRT_LPUART6.STAT |= LPUART_STAT_OR;
        ++g_systemDiagnostics.uartOverrunCount;
    }
#endif
}
