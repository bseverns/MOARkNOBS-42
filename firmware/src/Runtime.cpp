#include "Runtime.h"

#include <Arduino.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <imxrt.h>
#include "ButtonManager.h"
#include "ConfigManager.h"
#include "DiagnosticRecord.h"
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

#include <TimerOne.h>

// Runtime.cpp is the execution layer for the standalone instrument path.
//
// Reading order:
// 1. small private queues/flags and clock helpers
// 2. boot-time bring-up in initializeRuntime()
// 3. high-frequency service lanes (MIDI, followers, LFOs)
// 4. mid-tier musical processing (envelopes, note-offs, internal clock)
// 5. diagnostics and health reporting
//
// This keeps the file aligned with Runtime.h and with the scheduler-driven
// shape named in firmware_main.cpp.

namespace {

volatile uint8_t midiServiceRequestCount = 0;
constexpr uint8_t kMaxMidiServiceRequestCount = 0xFF;
uint32_t lastMidiDropAlertCount = 0;
uint32_t lastMidiTaskOverrunAlertCount = 0;
uint32_t lastUartOverrunAlertCount = 0;

template <typename T> void storageGet(int address, T &value) {
    ConfigManager::getStorageBackend()->readBytes(address, &value, sizeof(T));
}

struct PendingNoteOff {
    uint32_t dueTime = 0;
    uint8_t note = 0;
    uint8_t channel = 0;
    bool active = false;
};

constexpr size_t kPendingNoteOffCapacity = 64;
std::array<PendingNoteOff, kPendingNoteOffCapacity> pendingNoteOffs{};

void queueMidiServiceRequest() {
    // ISR-safe flagging: keep the interrupt short and let `processMIDI()` grab the work later.
    if (midiServiceRequestCount < kMaxMidiServiceRequestCount) {
        ++midiServiceRequestCount;
    }
}

// Consume the ISR-raised MIDI service flag from task context.
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

bool queuePendingNoteOff(uint8_t note, uint8_t channel, unsigned long delayMs);

namespace {
volatile unsigned long statusLedPulseDeadline = 0;
unsigned long statusLedBootDeadline = 0;
unsigned long statusLedBeatPulseDeadline = 0;
uint32_t statusLedLastClockTickCount = 0;

constexpr unsigned long kStatusLedBootHoldMs = 3000UL;
constexpr unsigned long kStatusLedHeartbeatHalfPeriodMs = 1000UL;
constexpr unsigned long kStatusLedClockPulseMs = 120UL;
} // namespace

void requestStatusLEDPulse(uint16_t durationMs) {
    // Blink the status LED from any task as diagnostics fire.
    statusLedPulseDeadline = now() + durationMs;
}

namespace {
bool deadlineStillActive(unsigned long deadline, unsigned long current) {
    return deadline != 0 && static_cast<long>(current - deadline) < 0;
}

bool externalClockDominant() {
    return g_followExternalClock && midiHandler.isClockRunning() &&
           midiHandler.hasExternalClockSignal();
}

bool internalClockDominant() { return g_tappedBPM > 0.0f && !externalClockDominant(); }

bool performanceClockActive() { return externalClockDominant() || internalClockDominant(); }

uint8_t resolveSlotNoteVelocity(uint8_t slotIndex, const MIDISlot &slot) {
    uint8_t velo = 125;
    if (slotIndex < efVoices.size() && efVoices[slotIndex].hasRendered()) {
        velo = efVoices[slotIndex].latestLevel();
    } else if (slot.ef.followerIndex >= 0 &&
               static_cast<size_t>(slot.ef.followerIndex) < envelopeFollowers.size()) {
        velo = static_cast<uint8_t>(constrain(
            envelopeFollowers[static_cast<size_t>(slot.ef.followerIndex)].getEnvelopeLevel(), 0,
            127));
    }
    int lfoVelocityOffset = static_cast<int>(lroundf(g_lfoVelocityShift * 32.0f));
    return static_cast<uint8_t>(constrain(velo + velocityShift + lfoVelocityOffset, 0, 127));
}

uint16_t resolveClockedNoteGateMs() {
    float bpm = 0.0f;
    if (externalClockDominant()) {
        bpm = midiHandler.externalClockBpm();
    }
    if (bpm <= 0.0f) {
        bpm = g_tappedBPM;
    }
    if (bpm <= 0.0f) {
        return 120;
    }
    const float quarterNoteMs = 60000.0f / bpm;
    return static_cast<uint16_t>(constrain(lroundf(quarterNoteMs * 0.45f), 40L, 400L));
}

void emitNoteSlot(uint8_t slotIndex, MIDISlot &slot, unsigned long gateMs) {
    const uint8_t note = static_cast<uint8_t>(slot.data1 % 128);
    const uint8_t velocity = resolveSlotNoteVelocity(slotIndex, slot);
    int lfoChanceOffset = static_cast<int>(lroundf(g_lfoNoteChance * 40.0f));
    int effectiveChance = constrain(static_cast<int>(changeProbability) + lfoChanceOffset, 0, 100);
    if (random(100U) >= static_cast<uint8_t>(effectiveChance)) {
        return;
    }
    midiHandler.sendNoteOn(note, velocity, slot.midiChannel);
    queuePendingNoteOff(note, slot.midiChannel, gateMs);
}

void emitClockedNoteSlots(uint32_t quarterEvents) {
    if (quarterEvents == 0 || !performanceClockActive()) {
        return;
    }
    const unsigned long gateMs = resolveClockedNoteGateMs();
    for (uint32_t beat = 0; beat < quarterEvents; ++beat) {
        for (uint8_t slotIndex = 0; slotIndex < NUM_POTS; ++slotIndex) {
            MIDISlot &slot = configManager.getSlot(slotIndex);
            if (!slot.active || slot.type != MIDIMessageType::Note ||
                arpeggiator.isActive(slotIndex)) {
                continue;
            }
            emitNoteSlot(slotIndex, slot, gateMs);
        }
    }
}
} // namespace

// Keep the status LED pulse alive until its deadline expires, layered on top of a base liveness
// pattern that is meaningful on the bench.
void serviceStatusLEDPulse() {
    const unsigned long current = now();
    const bool bootHoldActive = deadlineStillActive(statusLedBootDeadline, current);
    const bool diagnosticPulseActive = deadlineStillActive(statusLedPulseDeadline, current);
    const bool clockActive = externalClockDominant() || internalClockDominant();
    const uint32_t currentClockTickCount = midiHandler.clockTickCount();

    if (clockActive) {
        if (currentClockTickCount != statusLedLastClockTickCount) {
            if ((currentClockTickCount / 24U) != (statusLedLastClockTickCount / 24U)) {
                statusLedBeatPulseDeadline = current + kStatusLedClockPulseMs;
            }
            statusLedLastClockTickCount = currentClockTickCount;
        }
    } else {
        statusLedLastClockTickCount = currentClockTickCount;
    }

    const bool beatPulseActive = deadlineStillActive(statusLedBeatPulseDeadline, current);
    const bool heartbeatActive = ((current / kStatusLedHeartbeatHalfPeriodMs) % 2UL) == 0UL;

    if (!diagnosticPulseActive) {
        statusLedPulseDeadline = 0;
    }
    if (!beatPulseActive) {
        statusLedBeatPulseDeadline = 0;
    }

    const bool ledOn = bootHoldActive || diagnosticPulseActive || beatPulseActive ||
                       (!clockActive && heartbeatActive);
    ledManager.setStatusLED(ledOn);
}

// Turn monotonically increasing diagnostic counters into one-shot user-visible alerts.
void checkDiagnosticsForAlerts() {
    // Report any diagnostic counters over Serial and pulse the status LED so students can spot
    // trouble.
    const SystemDiagnostics diag = captureDiagnosticsSnapshot();

    const uint32_t midiDrop = diag.midiDropCount;
    if (midiDrop != lastMidiDropAlertCount) {
        LOG_PRINTF("{\"diagnostic\":\"midi_drop\",\"count\":%lu}\n",
                   static_cast<unsigned long>(midiDrop));
        requestStatusLEDPulse();
        lastMidiDropAlertCount = midiDrop;
    }

    const uint32_t midiTaskOverruns = diag.midiTaskOverrunCount;
    const uint32_t maxMidiMicros = diag.maxProcessMidiMicros;
    if (midiTaskOverruns != lastMidiTaskOverrunAlertCount) {
        LOG_PRINTF("{\"diagnostic\":\"midi_task_overrun\",\"count\":%lu,\"max_us\":%lu}\n",
                   static_cast<unsigned long>(midiTaskOverruns),
                   static_cast<unsigned long>(maxMidiMicros));
        requestStatusLEDPulse();
        lastMidiTaskOverrunAlertCount = midiTaskOverruns;
    }

    const uint32_t uartOverruns = diag.uartOverrunCount;
    if (uartOverruns != lastUartOverrunAlertCount) {
        LOG_PRINTF("{\"diagnostic\":\"uart_overrun\",\"count\":%lu}\n",
                   static_cast<unsigned long>(uartOverruns));
        requestStatusLEDPulse();
        lastUartOverrunAlertCount = uartOverruns;
    }
}

// Wire the runtime managers together once globals, EEPROM, and UI state are ready.
void initializeRuntime(bool baselinesLoaded) {
    // Initialize runtime services now that the UI and EEPROM-backed state are ready.
    pinMode(VREF_ADC_PIN, INPUT);
    for (int pin : ENVELOPE_ANALOG_PINS) {
#if defined(INPUT_PULLDOWN)
        pinMode(pin, INPUT_PULLDOWN);
#else
        pinMode(pin, INPUT);
#endif
    }
    g_vref = Utility::readVrefADC(VREF_ADC_PIN);

    midiHandler.begin();
    midiHandler.setDiagnostics(&g_systemDiagnostics);
    midiHandler.setDisplayManager(&displayManager);
    if (g_seedboxInteropEnabled) {
        seedbox::interop::mn42::SeedBoxLink::instance().begin(&midiHandler);
    }
    lfoManager.attachMIDI(&midiHandler);
    ledAnimator.setMode(configManager.getLedMode());
    statusLedBootDeadline = now() + kStatusLedBootHoldMs;
    statusLedPulseDeadline = 0;
    statusLedBeatPulseDeadline = 0;
    statusLedLastClockTickCount = midiHandler.clockTickCount();

    potentiometerManager.setMidiCallback(
        [&](uint8_t /*ccNumber*/, uint8_t value, uint16_t rawValue, uint8_t potIdx) {
            auto &slot = configManager.getSlot(potIdx);
            if (!slot.active)
                return;

            switch (slot.type) {
            case MIDIMessageType::CC:
                midiHandler.sendControlChange(slot.data1, value, slot.midiChannel);
                break;

            case MIDIMessageType::Note:
                if (performanceClockActive()) {
                    break;
                }
                emitNoteSlot(potIdx, slot, 100);
                break;
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

#if defined(MN42_DIAG_DISABLE_TIMER1_ISR) && (MN42_DIAG_DISABLE_TIMER1_ISR != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"timer1_isr_disabled\"}");
#else
    Timer1.initialize(1000);
    Timer1.attachInterrupt(midiTimerISR);
#endif

    filter.configure(BiquadFilter::LOWPASS, 1000, 44100);

    for (auto &ef : envelopeFollowers) {
        ef.toggleActive(true);
        if (!baselinesLoaded) {
            ef.calibrate();
        }
    }
    float sf, sq;
    storageGet(EEPROM_FILTER_FREQ, sf);
    storageGet(EEPROM_FILTER_Q, sq);
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

    switch (configManager.consumeRecoveryEvent()) {
    case ConfigManager::RecoveryEvent::kBackupRestored:
        displayManager.setTemporaryMessage("Config restored\nfrom backup", 3000);
        break;
    case ConfigManager::RecoveryEvent::kDefaultsLoaded:
        displayManager.setTemporaryMessage("EEPROM corrupted\ndefaults loaded", 4000);
        break;
    default:
        break;
    }

#if defined(MN42_DIAG_DISABLE_SCHEDULERS) && (MN42_DIAG_DISABLE_SCHEDULERS != 0)
    LOG_PRINTLN("{\"type\":\"diag\",\"code\":\"schedulers_disabled\"}");
#else
    initializeSchedulers();
#endif
}

// Timer ISR is intentionally tiny: it only requests service and leaves real work to task
// context.
void midiTimerISR() { queueMidiServiceRequest(); }

// High-frequency MIDI ingress/service lane.
void processMIDI() {
    if (!consumeMidiServiceRequest()) {
        return;
    }
    midiHandler.processIncomingMIDI();

    static uint32_t lastDisplayTick = 0;
    static uint32_t lastQuarterNoteTick = 0;
    uint32_t tickCount = midiHandler.clockTickCount();
    if (tickCount != lastDisplayTick) {
        // Catch-up path: if multiple clock ticks arrive between scheduler slices, advance by the
        // full delta to keep beat position stable.
        // Keep this section display-free so the high-priority MIDI service path never blocks on
        // I2C.
        uint32_t diff = tickCount - lastDisplayTick;
        lastDisplayTick = tickCount;

        lastClockTime = now();
        midiBeatPosition = (midiBeatPosition + diff) % 8;
        lastClockTime = now();
        midiHandler.clearClockTick();

        const uint32_t currentQuarterNoteTick = tickCount / 24U;
        if (currentQuarterNoteTick != lastQuarterNoteTick) {
            emitClockedNoteSlots(currentQuarterNoteTick - lastQuarterNoteTick);
            lastQuarterNoteTick = currentQuarterNoteTick;
        }
    }

    monitorSerialHealth();
}

// High-tier follower sampling lane.
void processEnvelopeFollowers() {
    // Fast follower pass runs in the high-tier scheduler; downstream MIDI mapping happens in
    // `processEnvelopes()` on the mid tier.
    constexpr size_t kFollowersPerPass = 1;
    static size_t nextFollowerIndex = 0;
    float gainTrim = 1.0f + g_lfoEfGainTrim;
    gainTrim = constrain(gainTrim, 0.0f, 2.0f);
    const size_t followerCount = envelopeFollowers.size();
    if (followerCount == 0) {
        return;
    }
    const size_t passes = std::min(kFollowersPerPass, followerCount);
    for (size_t offset = 0; offset < passes; ++offset) {
        const size_t idx = (nextFollowerIndex + offset) % followerCount;
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
    nextFollowerIndex = (nextFollowerIndex + passes) % followerCount;
}

// High-tier LFO update lane.
void processLFOs() {
    lfoManager.update(now());
    const LFOBus &bus = lfoManager.bus();
    g_lfoEfGainTrim = bus.efGainTrim;
    g_lfoArpSwing = bus.arpSwing;
    g_lfoVelocityShift = bus.velocityShift;
    g_lfoNoteChance = bus.noteChance;
    g_lfoArpGate = bus.arpGate;
    g_lfoJitterDepth = bus.jitterDepth;
    g_lfoJitterSmoothness = bus.jitterSmoothness;
    g_lfoValues[0] = lfoManager.normalizedValue(0);
    g_lfoValues[1] = lfoManager.normalizedValue(1);
    ledManager.setBrightnessModulator(1.0f);
}

// Shared delayed note-off queue used by both direct pot gestures and clocked note emission.
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
    // Note-off queue overflow is treated as a dropped-MIDI diagnostic event.
    ++g_systemDiagnostics.midiDropCount;
    requestStatusLEDPulse();
    return false;
}

// Deferred cleanup lane for scheduled note endings.
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

// Mid-tier envelope-to-MIDI modulation lane.
void processEnvelopes() {
    // Track last emitted values so EF modulation sends CC only on change.
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
            ledAnimator.setEnvelopeTarget(static_cast<uint8_t>(idx),
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
            // Per-slot EfVoice applies filter/ARG semantics before we fold into base pot value.
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

            ledAnimator.setPotTarget(potIndex, modulatedValue);
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

// Internal transport clock lane when the device is not following external MIDI clock.
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

// Runtime diagnostics, overrun reporting, and status LED maintenance.
void monitorSystemLoad() {
    static unsigned long lastMonitorTime = 0;
    static unsigned long taskCounter = 0;
    static unsigned long maxLoopDuration = 0;
    static unsigned long lastLoopStart = micros();
    static unsigned long lastLoopOverrunLogMs = 0;
    static unsigned long suppressedLoopOverruns = 0;

    unsigned long currentMicros = micros();
    unsigned long loopDuration = currentMicros - lastLoopStart;
    lastLoopStart = currentMicros;

    g_systemDiagnostics.lastLoopMicros = loopDuration;
    if (loopDuration > maxLoopDuration) {
        maxLoopDuration = loopDuration;
    }
    // Scan budget reconciliation: pot scanning alone takes ~1700µs (42 pots × 4 samples × 10µs).
    // Threshold raised to 2500µs to accommodate full scan + mux settle + MIDI work.
    if (loopDuration > 2500UL) {
        ++g_systemDiagnostics.loopOverrunCount;
        DiagnosticRecord::recordLoopOverrunHighWater(loopDuration);
        const unsigned long currentMillis = now();
        if ((currentMillis - lastLoopOverrunLogMs) >= 1000UL) {
            LOG_PRINTF("{\"diagnostic\":\"loop_overrun\",\"duration_us\":%lu,\"count\":%lu,"
                       "\"suppressed\":%lu}\n",
                       static_cast<unsigned long>(loopDuration),
                       static_cast<unsigned long>(g_systemDiagnostics.loopOverrunCount),
                       static_cast<unsigned long>(suppressedLoopOverruns));
            lastLoopOverrunLogMs = currentMillis;
            suppressedLoopOverruns = 0;
        } else {
            ++suppressedLoopOverruns;
        }
        requestStatusLEDPulse();
    }

    taskCounter++;
    unsigned long currentMillis = now();
    if (currentMillis - lastMonitorTime >= 1000UL) {
        // Diagnostic metrics must be JSON for protocol discipline.
        LOG_PRINTF("{\"type\":\"diag\",\"metric\":\"tasks_per_second\",\"value\":%lu}\n",
                   taskCounter);
        g_systemDiagnostics.maxLoopMicros = maxLoopDuration;
        maxLoopDuration = 0;
        taskCounter = 0;
        lastMonitorTime = currentMillis;
    }

    checkDiagnosticsForAlerts();
    serviceStatusLEDPulse();
}

// Hardware serial health probe used by the configurator/runtime serial lane.
void monitorSerialHealth() {
#if defined(__IMXRT1062__)
    if (IMXRT_LPUART6.STAT & LPUART_STAT_OR) {
        IMXRT_LPUART6.STAT |= LPUART_STAT_OR;
        ++g_systemDiagnostics.uartOverrunCount;
    }
#endif
}

#if defined(UNIT_TEST)
void testOnly_resetRuntimeState() {
    midiServiceRequestCount = 0;
    pendingNoteOffs = {};
    statusLedPulseDeadline = 0;
    lastMidiDropAlertCount = 0;
    lastMidiTaskOverrunAlertCount = 0;
    lastUartOverrunAlertCount = 0;
}
#endif
