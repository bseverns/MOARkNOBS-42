#include <Arduino.h>
#include <EEPROM.h>
#include <imxrt.h>
#include <cctype>
#include <cstring>

#include "Globals.h"
#include "LEDManager.h"
#include "LedAnimator.h"
#include "LedMode.h"
#include "TimeUtils.h"
#include "version.h"

namespace {
constexpr unsigned long kSerialWaitMs = 2500;
constexpr unsigned long kPerLedHoldMs = 65;
constexpr unsigned long kPhaseGapMs = 250;
constexpr unsigned long kStatusPrintMs = 1000;
constexpr unsigned long kHeartbeatMs = 250;
constexpr unsigned long kClockPulseEveryMs = 300;
constexpr unsigned long kRandomSweepStepMs = 45;
constexpr unsigned long kRandomWashStepMs = 40;
constexpr unsigned long kPeakRateWindowMs = 250;
constexpr uint16_t kRandomBlastLogStride = 128;
constexpr size_t kSerialCommandBufferSize = 64;

enum class DemoPhaseStyle : uint8_t { Animator, RandomSweep, RandomWash, RandomBlast };

struct DemoPhase {
    DemoPhaseStyle style;
    LedMode mode;
    const char *label;
    unsigned long durationMs;
};

constexpr DemoPhase kDemoPhases[] = {
    {DemoPhaseStyle::Animator, LedMode::Static, "Static gradient sweep", 7000},
    {DemoPhaseStyle::Animator, LedMode::PeakHold, "Peak-hold spikes", 7000},
    {DemoPhaseStyle::Animator, LedMode::Trail, "Trailing chase", 7000},
    {DemoPhaseStyle::RandomSweep, LedMode::Static, "Random color sweep", 7000},
    {DemoPhaseStyle::RandomWash, LedMode::Static, "Full random wash", 7000},
    {DemoPhaseStyle::RandomBlast, LedMode::Static, "Max random blast", 7000},
    {DemoPhaseStyle::Animator, LedMode::ClockPulse, "Clock pulse wash", 7000},
};

constexpr size_t kDemoPhaseCount = sizeof(kDemoPhases) / sizeof(kDemoPhases[0]);

size_t g_phaseIndex = 0;
unsigned long g_phaseStartedMs = 0;
unsigned long g_lastStatusPrintMs = 0;
unsigned long g_lastClockPulseMs = 0;
unsigned long g_lastRandomSweepStepMs = 0;
uint32_t g_frameCounter = 0;
uint32_t g_phaseFrameCounter = 0;
uint32_t g_phaseRandomWrites = 0;
uint16_t g_phaseRandomPasses = 0;
uint16_t g_randomSweepIndex = 0;
unsigned long g_rateWindowStartedMs = 0;
uint32_t g_rateWindowFrameBase = 0;
uint32_t g_rateWindowWriteBase = 0;
uint16_t g_rateWindowPassBase = 0;
float g_peakFramesPerSecond = 0.0f;
float g_peakWritesPerSecond = 0.0f;
float g_peakPassesPerSecond = 0.0f;
bool g_phaseLocked = false;
size_t g_lockedPhaseIndex = 0;
char g_serialCommandBuffer[kSerialCommandBufferSize] = {};
size_t g_serialCommandLength = 0;

const char *phaseModeName(const DemoPhase &phase) {
    if (phase.style == DemoPhaseStyle::RandomSweep)
        return "RANDOM_SWEEP";
    if (phase.style == DemoPhaseStyle::RandomWash)
        return "RANDOM_WASH";
    if (phase.style == DemoPhaseStyle::RandomBlast)
        return "RANDOM_BLAST";
    return ledModeToString(phase.mode);
}

const char *phaseShortName(size_t index) {
    switch (index % kDemoPhaseCount) {
    case 0:
        return "static";
    case 1:
        return "peak";
    case 2:
        return "trail";
    case 3:
        return "sweep";
    case 4:
        return "wash";
    case 5:
        return "blast";
    case 6:
        return "clock";
    default:
        return "unknown";
    }
}

bool equalsIgnoreCase(const char *lhs, const char *rhs) {
    if (!lhs || !rhs)
        return false;
    while (*lhs != '\0' && *rhs != '\0') {
        const char left = static_cast<char>(std::tolower(static_cast<unsigned char>(*lhs)));
        const char right = static_cast<char>(std::tolower(static_cast<unsigned char>(*rhs)));
        if (left != right)
            return false;
        ++lhs;
        ++rhs;
    }
    return *lhs == '\0' && *rhs == '\0';
}

char *trimWhitespace(char *text) {
    while (*text != '\0' && std::isspace(static_cast<unsigned char>(*text))) {
        ++text;
    }

    char *end = text + std::strlen(text);
    while (end > text && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    *end = '\0';
    return text;
}

bool findPhaseIndex(const char *name, size_t &indexOut) {
    if (equalsIgnoreCase(name, "static")) {
        indexOut = 0;
        return true;
    }
    if (equalsIgnoreCase(name, "peak") || equalsIgnoreCase(name, "peak_hold")) {
        indexOut = 1;
        return true;
    }
    if (equalsIgnoreCase(name, "trail")) {
        indexOut = 2;
        return true;
    }
    if (equalsIgnoreCase(name, "sweep") || equalsIgnoreCase(name, "random_sweep")) {
        indexOut = 3;
        return true;
    }
    if (equalsIgnoreCase(name, "wash") || equalsIgnoreCase(name, "random_wash")) {
        indexOut = 4;
        return true;
    }
    if (equalsIgnoreCase(name, "blast") || equalsIgnoreCase(name, "random_blast") ||
        equalsIgnoreCase(name, "max")) {
        indexOut = 5;
        return true;
    }
    if (equalsIgnoreCase(name, "clock") || equalsIgnoreCase(name, "clock_pulse")) {
        indexOut = 6;
        return true;
    }
    return false;
}

const char *resetCauseToString(uint32_t cause) {
    if (cause & 0x01)
        return "Power-on";
    if (cause & 0x02)
        return "Reset pin";
    if (cause & 0x04)
        return "Watchdog";
    if (cause & 0x08)
        return "JTAG";
    if (cause & 0x10)
        return "Software";
    if (cause & 0x20)
        return "Lockup";
    if (cause & 0x40)
        return "Brown-out";
    return "Unknown";
}

LEDManager &demoLeds() {
    static LEDManager manager(hwConfig);
    return manager;
}

LedAnimator &demoAnimator() {
    static LedAnimator animator(demoLeds());
    return animator;
}

void waitForSerialWindow() {
    const unsigned long started = millis();
    while (!Serial && (millis() - started) < kSerialWaitMs) {
        delay(10);
    }
}

void readBootHealth() {
    g_resetCause = SRC_SRSR;
    EEPROM.get(EEPROM_BROWNOUT_COUNT, g_brownoutCount);
    if (g_brownoutCount == 0xFFFF) {
        g_brownoutCount = 0;
        EEPROM.put(EEPROM_BROWNOUT_COUNT, g_brownoutCount);
    }
    if (g_resetCause & 0x40) {
        ++g_brownoutCount;
        EEPROM.put(EEPROM_BROWNOUT_COUNT, g_brownoutCount);
    }
}

const char *sectionNameForIndex(uint16_t index) {
    if (index < SLOT_LED_COUNT)
        return "slot";
    if (index < EF_LED_OFFSET() + EF_LED_COUNT)
        return "envelope";
    if (index == CONTROL_LED_INDEX())
        return "control";
    return "pot";
}

void clearStrip() {
    demoLeds().setState(LEDState::IDLE);
    demoLeds().setColor(CRGB::Black);
    demoLeds().update();
}

void updatePeakRates(unsigned long nowMs) {
    const unsigned long elapsedMs = nowMs - g_rateWindowStartedMs;
    if (elapsedMs < kPeakRateWindowMs)
        return;

    const float elapsed = static_cast<float>(elapsedMs);
    const uint32_t frameDelta = g_phaseFrameCounter - g_rateWindowFrameBase;
    const uint32_t writeDelta = g_phaseRandomWrites - g_rateWindowWriteBase;
    const uint16_t passDelta = g_phaseRandomPasses - g_rateWindowPassBase;

    const float framesPerSecond = (static_cast<float>(frameDelta) * 1000.0f) / elapsed;
    const float writesPerSecond = (static_cast<float>(writeDelta) * 1000.0f) / elapsed;
    const float passesPerSecond = (static_cast<float>(passDelta) * 1000.0f) / elapsed;

    if (framesPerSecond > g_peakFramesPerSecond)
        g_peakFramesPerSecond = framesPerSecond;
    if (writesPerSecond > g_peakWritesPerSecond)
        g_peakWritesPerSecond = writesPerSecond;
    if (passesPerSecond > g_peakPassesPerSecond)
        g_peakPassesPerSecond = passesPerSecond;

    g_rateWindowStartedMs = nowMs;
    g_rateWindowFrameBase = g_phaseFrameCounter;
    g_rateWindowWriteBase = g_phaseRandomWrites;
    g_rateWindowPassBase = g_phaseRandomPasses;
}

void printHealth(const char *phaseLabel) {
    const DemoPhase &phase = kDemoPhases[g_phaseIndex];
    const unsigned long phaseElapsedMs = max<unsigned long>(1, now() - g_phaseStartedMs);
    const float framesPerSecond =
        (static_cast<float>(g_phaseFrameCounter) * 1000.0f) / static_cast<float>(phaseElapsedMs);
    const float writesPerSecond =
        (static_cast<float>(g_phaseRandomWrites) * 1000.0f) / static_cast<float>(phaseElapsedMs);
    const float passesPerSecond =
        (static_cast<float>(g_phaseRandomPasses) * 1000.0f) / static_cast<float>(phaseElapsedMs);
    Serial.printf(
        "[HEALTH] uptime=%lums phase=\"%s\" mode=%s leds=%u brightness=%u reset=%s brownouts=%u "
        "midiDrops=%lu loopOverruns=%lu maxLoopUs=%lu frames=%lu phaseFrames=%lu randomWrites=%lu "
        "randomPasses=%u fps=%.1f writesPerSec=%.1f passesPerSec=%.1f peakFps=%.1f "
        "peakWritesPerSec=%.1f peakPassesPerSec=%.1f lock=%s\n",
        now(), phaseLabel, phaseModeName(phase), NUM_LEDS(), demoLeds().getBrightness(),
        resetCauseToString(g_resetCause), g_brownoutCount, g_systemDiagnostics.midiDropCount,
        g_systemDiagnostics.loopOverrunCount, g_systemDiagnostics.maxLoopMicros, g_frameCounter,
        g_phaseFrameCounter, g_phaseRandomWrites, g_phaseRandomPasses, framesPerSecond,
        writesPerSecond, passesPerSecond, g_peakFramesPerSecond, g_peakWritesPerSecond,
        g_peakPassesPerSecond, g_phaseLocked ? phaseShortName(g_lockedPhaseIndex) : "auto");
}

void printCommandHelp() {
    Serial.println("[CMD] phase <static|peak|trail|sweep|wash|blast|clock>");
    Serial.println("[CMD] phase auto");
    Serial.println("[CMD] status");
    Serial.println("[CMD] help");
}

void runSingleLedHealthPass() {
    Serial.printf("[SELFTEST] Starting single-diode sweep across %u LEDs.\n", NUM_LEDS());
    for (uint16_t index = 0; index < NUM_LEDS(); ++index) {
        clearStrip();
        demoLeds().setState(LEDState::TEMP_FEEDBACK, static_cast<uint8_t>(index));
        demoLeds().setStatusLED((index & 0x01u) == 0u);
        Serial.printf("[SELFTEST] LED %u/%u section=%s status=ON\n", index + 1, NUM_LEDS(),
                      sectionNameForIndex(index));
        delay(kPerLedHoldMs);
    }
    clearStrip();
    demoLeds().setStatusLED(false);
    Serial.println("[SELFTEST] Single-diode sweep complete.");
    delay(kPhaseGapMs);
}

uint8_t makePotTarget(uint8_t index, unsigned long nowMs, LedMode mode) {
    switch (mode) {
    case LedMode::Static:
        return static_cast<uint8_t>(sin8(static_cast<uint8_t>((nowMs / 10) + index * 5)) >> 1);
    case LedMode::PeakHold: {
        const uint8_t wave = sin8(static_cast<uint8_t>((nowMs / 18) + index * 19));
        return (wave > 220) ? 127 : static_cast<uint8_t>(wave / 3);
    }
    case LedMode::Trail: {
        const int16_t head = static_cast<int16_t>((nowMs / 70) % NUM_POTS);
        int16_t distance = static_cast<int16_t>(index) - head;
        if (distance < 0)
            distance = -distance;
        const int16_t wrappedDistance = static_cast<int16_t>(NUM_POTS) - distance;
        if (wrappedDistance < distance)
            distance = wrappedDistance;
        if (distance > 6)
            return 0;
        return static_cast<uint8_t>(127 - distance * 18);
    }
    case LedMode::ClockPulse:
    default:
        return static_cast<uint8_t>(sin8(static_cast<uint8_t>((nowMs / 14) + index * 11)) >> 2);
    }
}

uint8_t makeEnvelopeTarget(uint8_t index, unsigned long nowMs, LedMode mode) {
    switch (mode) {
    case LedMode::Static:
        return static_cast<uint8_t>(sin8(static_cast<uint8_t>((nowMs / 16) + index * 29)) >> 1);
    case LedMode::PeakHold:
        return (((nowMs / 220) + index) % 3 == 0) ? 127 : 20;
    case LedMode::Trail:
        return static_cast<uint8_t>(sin8(static_cast<uint8_t>((nowMs / 22) + index * 37)) >> 1);
    case LedMode::ClockPulse:
    default:
        return static_cast<uint8_t>(30 +
                                    (sin8(static_cast<uint8_t>((nowMs / 20) + index * 17)) >> 2));
    }
}

void feedAnimationTargets(unsigned long nowMs) {
    LedAnimator &animator = demoAnimator();
    const LedMode mode = kDemoPhases[g_phaseIndex].mode;

    for (uint8_t index = 0; index < NUM_POTS; ++index) {
        animator.setPotTarget(index, makePotTarget(index, nowMs, mode));
    }

    for (uint8_t index = 0; index < NUM_ENVELOPES; ++index) {
        animator.setEnvelopeTarget(index, makeEnvelopeTarget(index, nowMs, mode));
    }
}

void runRandomSweepFrame(unsigned long nowMs) {
    if ((nowMs - g_lastRandomSweepStepMs) < kRandomSweepStepMs)
        return;

    g_lastRandomSweepStepMs = nowMs;
    demoLeds().setPixelColor(g_randomSweepIndex, CRGB(static_cast<uint8_t>(random(256)),
                                                      static_cast<uint8_t>(random(256)),
                                                      static_cast<uint8_t>(random(256))));
    ++g_phaseRandomWrites;

    ++g_randomSweepIndex;
    if (g_randomSweepIndex >= NUM_LEDS()) {
        g_randomSweepIndex = 0;
        ++g_phaseRandomPasses;
        Serial.println("[DEMO] Random color sweep pass complete.");
        delay(kPhaseGapMs);
        clearStrip();
    }
}

void paintRandomWash() {
    for (uint16_t index = 0; index < NUM_LEDS(); ++index) {
        demoLeds().setPixelColor(index, CRGB(static_cast<uint8_t>(random(256)),
                                             static_cast<uint8_t>(random(256)),
                                             static_cast<uint8_t>(random(256))));
        ++g_phaseRandomWrites;
    }

    ++g_phaseRandomPasses;
}

void runRandomWashFrame(unsigned long nowMs) {
    if ((nowMs - g_lastRandomSweepStepMs) < kRandomWashStepMs)
        return;

    g_lastRandomSweepStepMs = nowMs;
    paintRandomWash();
}

void runRandomBlastFrame() {
    paintRandomWash();
    if ((g_phaseRandomPasses % kRandomBlastLogStride) == 0u) {
        Serial.printf("[DEMO] Max random blast pass=%u writes=%lu\n", g_phaseRandomPasses,
                      g_phaseRandomWrites);
    }
}

void startPhase(size_t index) {
    g_phaseIndex = index % kDemoPhaseCount;
    g_phaseStartedMs = now();
    g_lastStatusPrintMs = g_phaseStartedMs;
    g_lastClockPulseMs = g_phaseStartedMs;
    g_lastRandomSweepStepMs = g_phaseStartedMs;
    g_phaseFrameCounter = 0;
    g_phaseRandomWrites = 0;
    g_phaseRandomPasses = 0;
    g_randomSweepIndex = 0;
    g_rateWindowStartedMs = g_phaseStartedMs;
    g_rateWindowFrameBase = 0;
    g_rateWindowWriteBase = 0;
    g_rateWindowPassBase = 0;
    g_peakFramesPerSecond = 0.0f;
    g_peakWritesPerSecond = 0.0f;
    g_peakPassesPerSecond = 0.0f;

    demoAnimator().setMode(kDemoPhases[g_phaseIndex].mode);
    clearStrip();

    Serial.printf("[DEMO] Entering phase %u/%u: %s (%s) for %lums.\n",
                  static_cast<unsigned>(g_phaseIndex + 1), static_cast<unsigned>(kDemoPhaseCount),
                  kDemoPhases[g_phaseIndex].label, phaseModeName(kDemoPhases[g_phaseIndex]),
                  kDemoPhases[g_phaseIndex].durationMs);
    printHealth(kDemoPhases[g_phaseIndex].label);
}

void applyPhaseCommand(size_t phaseIndex, bool lockPhase) {
    g_phaseLocked = lockPhase;
    g_lockedPhaseIndex = phaseIndex % kDemoPhaseCount;
    Serial.printf("[CMD] phase=%s lock=%s\n", phaseShortName(g_lockedPhaseIndex),
                  g_phaseLocked ? "on" : "off");
    startPhase(g_lockedPhaseIndex);
}

void processCommand(char *command) {
    char *trimmed = trimWhitespace(command);
    if (*trimmed == '\0')
        return;

    char *space = std::strchr(trimmed, ' ');
    char *arg = nullptr;
    if (space) {
        *space = '\0';
        arg = trimWhitespace(space + 1);
    }

    if (equalsIgnoreCase(trimmed, "help")) {
        printCommandHelp();
        return;
    }

    if (equalsIgnoreCase(trimmed, "status")) {
        printHealth(kDemoPhases[g_phaseIndex].label);
        return;
    }

    if (equalsIgnoreCase(trimmed, "phase")) {
        if (!arg || *arg == '\0') {
            Serial.println("[CMD] phase requires a target or 'auto'.");
            printCommandHelp();
            return;
        }

        if (equalsIgnoreCase(arg, "auto")) {
            g_phaseLocked = false;
            Serial.println("[CMD] phase lock cleared; automatic cycling resumed.");
            return;
        }

        size_t phaseIndex = 0;
        if (!findPhaseIndex(arg, phaseIndex)) {
            Serial.printf("[CMD] unknown phase '%s'\n", arg);
            printCommandHelp();
            return;
        }

        applyPhaseCommand(phaseIndex, true);
        return;
    }

    Serial.printf("[CMD] unknown command '%s'\n", trimmed);
    printCommandHelp();
}

void pollSerialCommands() {
    while (Serial.available() > 0) {
        const int next = Serial.read();
        if (next < 0)
            return;

        if (next == '\r')
            continue;

        if (next == '\n') {
            g_serialCommandBuffer[g_serialCommandLength] = '\0';
            processCommand(g_serialCommandBuffer);
            g_serialCommandLength = 0;
            continue;
        }

        if (g_serialCommandLength + 1 < kSerialCommandBufferSize) {
            g_serialCommandBuffer[g_serialCommandLength++] = static_cast<char>(next);
        } else {
            g_serialCommandLength = 0;
            Serial.println("[CMD] command too long; buffer cleared.");
        }
    }
}
} // namespace

void setup() {
    Serial.begin(SERIAL_BAUD);
    waitForSerialWindow();

    readBootHealth();
    loadHardwareRuntimeTuning();
    pinMode(hwConfig.statusLedPin, OUTPUT);
    digitalWrite(hwConfig.statusLedPin, LOW);

    Serial.println();
    Serial.printf("MN42 LED demo firmware %s schema %04X\n", FW_VERSION_STR, CONFIG_VERSION);
    Serial.printf("Reset 0x%08lX (%s) brownouts=%u\n", g_resetCause,
                  resetCauseToString(g_resetCause), g_brownoutCount);
    Serial.printf("LED pin=%u status pin=%u slot=%u ef=%u control=1 pot=%u total=%u\n",
                  hwConfig.ledPin, hwConfig.statusLedPin, SLOT_LED_COUNT, EF_LED_COUNT,
                  POT_LED_COUNT, NUM_LEDS());
    printCommandHelp();

    demoLeds().begin();
    demoLeds().setBrightness(MN42_DEMO_LED_BRIGHTNESS);
    randomSeed(micros() ^ g_resetCause ^ HW_OCOTP_CFG0);

    runSingleLedHealthPass();
    startPhase(0);
}

void loop() {
    unsigned long nowMs = now();
    pollSerialCommands();

    if ((nowMs - g_phaseStartedMs) >= kDemoPhases[g_phaseIndex].durationMs) {
        startPhase(g_phaseLocked ? g_lockedPhaseIndex : (g_phaseIndex + 1));
        nowMs = now();
    }

    const DemoPhase &phase = kDemoPhases[g_phaseIndex];

    if (phase.style == DemoPhaseStyle::RandomSweep) {
        runRandomSweepFrame(nowMs);
    } else if (phase.style == DemoPhaseStyle::RandomWash) {
        runRandomWashFrame(nowMs);
    } else if (phase.style == DemoPhaseStyle::RandomBlast) {
        runRandomBlastFrame();
    } else {
        feedAnimationTargets(nowMs);
    }

    bool clockPulse = false;
    if (phase.style == DemoPhaseStyle::Animator && phase.mode == LedMode::ClockPulse &&
        (nowMs - g_lastClockPulseMs) >= kClockPulseEveryMs) {
        g_lastClockPulseMs = nowMs;
        clockPulse = true;
    }

    if (phase.style == DemoPhaseStyle::Animator) {
        demoAnimator().tick(nowMs, clockPulse, false);
    }
    demoLeds().update();
    demoLeds().setStatusLED(((nowMs / kHeartbeatMs) % 2u) == 0u);

    if ((nowMs - g_lastStatusPrintMs) >= kStatusPrintMs) {
        g_lastStatusPrintMs = nowMs;
        printHealth(phase.label);
    }

    ++g_frameCounter;
    ++g_phaseFrameCounter;
    updatePeakRates(nowMs);
    if (phase.style != DemoPhaseStyle::RandomBlast) {
        delay(20);
    }
}
