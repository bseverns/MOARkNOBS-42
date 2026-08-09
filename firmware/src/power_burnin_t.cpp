#include <Arduino.h>
#include <EEPROM.h>
#include <imxrt.h>

#include <cctype>
#include <cstring>

#include "Globals.h"
#include "LEDManager.h"
#include "TimeUtils.h"
#include "Utility.h"
#include "sys/report.h"
#include "version.h"

namespace {
constexpr unsigned long kSerialWaitMs = 2500;
constexpr unsigned long kStatusPrintMs = 1000;
constexpr unsigned long kVrefSampleMs = 100;
constexpr unsigned long kPeakRateWindowMs = 250;
constexpr unsigned long kRandomWashStepMs = 40;
constexpr uint16_t kRandomBlastLogStride = 128;
constexpr size_t kSerialCommandBufferSize = 64;

enum class BurnPhaseStyle : uint8_t { StaticFill, WhiteSweep, RandomWash, RandomBlast };

struct BurnPhase {
    BurnPhaseStyle style;
    const char *label;
    const char *shortName;
    unsigned long durationMs;
    uint8_t brightness;
    CRGB color;
};

// These high-brightness phases are intentional stress tests.
// They are not normal operating visuals and assume a verified power path.
constexpr BurnPhase kBurnPhases[] = {
    {BurnPhaseStyle::StaticFill, "Idle black settle", "idle", 5000, MN42_SAFE_BENCH_LED_BRIGHTNESS,
     CRGB::Black},
    {BurnPhaseStyle::StaticFill, "White 25%", "white25", 15000, 64, CRGB::White},
    {BurnPhaseStyle::StaticFill, "White 50%", "white50", 15000, 128, CRGB::White},
    {BurnPhaseStyle::StaticFill, "White 100%", "white100", 15000, 255, CRGB::White},
    {BurnPhaseStyle::StaticFill, "Red max", "red", 10000, 255, CRGB::Red},
    {BurnPhaseStyle::StaticFill, "Green max", "green", 10000, 255, CRGB::Green},
    {BurnPhaseStyle::StaticFill, "Blue max", "blue", 10000, 255, CRGB::Blue},
    {BurnPhaseStyle::WhiteSweep, "White brightness sweep", "sweep", 20000, 255, CRGB::White},
    {BurnPhaseStyle::RandomWash, "Random wash", "wash", 15000, 224, CRGB::Black},
    {BurnPhaseStyle::RandomBlast, "Max random blast", "blast", 15000, 255, CRGB::Black},
};

constexpr size_t kBurnPhaseCount = sizeof(kBurnPhases) / sizeof(kBurnPhases[0]);

size_t g_phaseIndex = 0;
bool g_phaseLocked = false;
size_t g_lockedPhaseIndex = 0;
unsigned long g_phaseStartedMs = 0;
unsigned long g_lastStatusPrintMs = 0;
unsigned long g_lastVrefSampleMs = 0;
unsigned long g_lastRandomStepMs = 0;
uint32_t g_frameCounter = 0;
uint32_t g_phaseFrameCounter = 0;
uint32_t g_phasePixelWrites = 0;
uint16_t g_phasePasses = 0;
float g_lastVrefVolts = 0.0f;
float g_minVrefVolts = 0.0f;
float g_maxVrefVolts = 0.0f;
unsigned long g_rateWindowStartedMs = 0;
uint32_t g_rateWindowFrameBase = 0;
uint32_t g_rateWindowWriteBase = 0;
uint16_t g_rateWindowPassBase = 0;
float g_peakFramesPerSecond = 0.0f;
float g_peakWritesPerSecond = 0.0f;
float g_peakPassesPerSecond = 0.0f;
char g_serialCommandBuffer[kSerialCommandBufferSize] = {};
size_t g_serialCommandLength = 0;

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

LEDManager &burnLeds() {
    static LEDManager manager(hwConfig);
    return manager;
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

void clearStrip() {
    burnLeds().setState(LEDState::IDLE);
    burnLeds().setColor(CRGB::Black);
    burnLeds().update();
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

bool findPhaseIndex(const char *name, size_t &indexOut) {
    for (size_t i = 0; i < kBurnPhaseCount; ++i) {
        if (equalsIgnoreCase(name, kBurnPhases[i].shortName)) {
            indexOut = i;
            return true;
        }
    }
    if (equalsIgnoreCase(name, "white"))
        return findPhaseIndex("white100", indexOut);
    if (equalsIgnoreCase(name, "fullwhite"))
        return findPhaseIndex("white100", indexOut);
    return false;
}

void sampleVref() {
    g_lastVrefVolts = Utility::readVrefADC(hwConfig.vrefAdcPin);
    if (g_phaseFrameCounter == 0 && g_phasePasses == 0 && g_phasePixelWrites == 0) {
        g_minVrefVolts = g_lastVrefVolts;
        g_maxVrefVolts = g_lastVrefVolts;
        return;
    }
    if (g_lastVrefVolts < g_minVrefVolts)
        g_minVrefVolts = g_lastVrefVolts;
    if (g_lastVrefVolts > g_maxVrefVolts)
        g_maxVrefVolts = g_lastVrefVolts;
}

void updatePeakRates(unsigned long nowMs) {
    const unsigned long elapsedMs = nowMs - g_rateWindowStartedMs;
    if (elapsedMs < kPeakRateWindowMs)
        return;

    const float elapsed = static_cast<float>(elapsedMs);
    const uint32_t frameDelta = g_phaseFrameCounter - g_rateWindowFrameBase;
    const uint32_t writeDelta = g_phasePixelWrites - g_rateWindowWriteBase;
    const uint16_t passDelta = g_phasePasses - g_rateWindowPassBase;

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
    g_rateWindowWriteBase = g_phasePixelWrites;
    g_rateWindowPassBase = g_phasePasses;
}

void printHealth() {
    const BurnPhase &phase = kBurnPhases[g_phaseIndex];
    const unsigned long current = now();
    const unsigned long phaseElapsedMs = current - g_phaseStartedMs;
    const float elapsed = static_cast<float>(phaseElapsedMs == 0 ? 1 : phaseElapsedMs);
    const float framesPerSecond = (static_cast<float>(g_phaseFrameCounter) * 1000.0f) / elapsed;
    const float writesPerSecond = (static_cast<float>(g_phasePixelWrites) * 1000.0f) / elapsed;
    const float passesPerSecond = (static_cast<float>(g_phasePasses) * 1000.0f) / elapsed;

    Serial.printf(
        "[BURN] uptime=%lums phase=%s mode=%u brightness=%u vref=%.3f vrefMin=%.3f vrefMax=%.3f "
        "frames=%lu passes=%u pixelWrites=%lu fps=%.1f writesPerSec=%.1f passesPerSec=%.1f "
        "peakFps=%.1f peakWritesPerSec=%.1f peakPassesPerSec=%.1f brownouts=%u lock=%s\n",
        current, phase.shortName, static_cast<unsigned>(phase.style), burnLeds().getBrightness(),
        g_lastVrefVolts, g_minVrefVolts, g_maxVrefVolts, g_phaseFrameCounter, g_phasePasses,
        g_phasePixelWrites, framesPerSecond, writesPerSecond, passesPerSecond,
        g_peakFramesPerSecond, g_peakWritesPerSecond, g_peakPassesPerSecond, g_brownoutCount,
        g_phaseLocked ? kBurnPhases[g_lockedPhaseIndex].shortName : "auto");
}

void printCommandHelp() {
    Serial.println("[CMD] phase <idle|white25|white50|white100|red|green|blue|sweep|wash|blast>");
    Serial.println("[CMD] phase auto");
    Serial.println("[CMD] status");
    Serial.println("[CMD] help");
    Serial.println("[SAFETY] white100/blast are deliberate high-current tests; verify LED rail "
                   "topology first.");
}

void paintAll(const CRGB &color) {
    burnLeds().setAll(color);
    g_phasePixelWrites += NUM_LEDS();
    ++g_phasePasses;
}

void paintRandomWash() {
    for (uint16_t index = 0; index < NUM_LEDS(); ++index) {
        burnLeds().setPixelColor(index, CRGB(static_cast<uint8_t>(random(256)),
                                             static_cast<uint8_t>(random(256)),
                                             static_cast<uint8_t>(random(256))));
        ++g_phasePixelWrites;
    }
    ++g_phasePasses;
}

void applyWhiteSweep(unsigned long nowMs) {
    const unsigned long elapsedMs = nowMs - g_phaseStartedMs;
    const unsigned long cycleMs =
        kBurnPhases[g_phaseIndex].durationMs == 0 ? 1 : kBurnPhases[g_phaseIndex].durationMs;
    const unsigned long halfCycleMs = cycleMs / 2;
    const unsigned long phaseMs = (halfCycleMs == 0) ? 0 : (elapsedMs % cycleMs);
    uint8_t brightness = 0;

    if (halfCycleMs == 0) {
        brightness = 255;
    } else if (phaseMs <= halfCycleMs) {
        brightness = static_cast<uint8_t>(map(phaseMs, 0, halfCycleMs, 0, 255));
    } else {
        brightness = static_cast<uint8_t>(map(phaseMs - halfCycleMs, 0, halfCycleMs, 255, 0));
    }

    burnLeds().setBrightness(brightness);
    paintAll(CRGB::White);
}

void startPhase(size_t index) {
    g_phaseIndex = index % kBurnPhaseCount;
    g_phaseStartedMs = now();
    g_lastStatusPrintMs = g_phaseStartedMs;
    g_lastVrefSampleMs = 0;
    g_lastRandomStepMs = g_phaseStartedMs;
    g_phaseFrameCounter = 0;
    g_phasePixelWrites = 0;
    g_phasePasses = 0;
    g_rateWindowStartedMs = g_phaseStartedMs;
    g_rateWindowFrameBase = 0;
    g_rateWindowWriteBase = 0;
    g_rateWindowPassBase = 0;
    g_peakFramesPerSecond = 0.0f;
    g_peakWritesPerSecond = 0.0f;
    g_peakPassesPerSecond = 0.0f;

    const BurnPhase &phase = kBurnPhases[g_phaseIndex];
    burnLeds().setBrightness(phase.brightness);
    clearStrip();

    if (phase.style == BurnPhaseStyle::StaticFill) {
        paintAll(phase.color);
    } else if (phase.style == BurnPhaseStyle::WhiteSweep) {
        applyWhiteSweep(g_phaseStartedMs);
    }

    g_lastVrefVolts = Utility::readVrefADC(hwConfig.vrefAdcPin);
    g_minVrefVolts = g_lastVrefVolts;
    g_maxVrefVolts = g_lastVrefVolts;

    Serial.printf("[BURN] Entering phase %u/%u: %s (%s) duration=%lums brightness=%u\n",
                  static_cast<unsigned>(g_phaseIndex + 1), static_cast<unsigned>(kBurnPhaseCount),
                  phase.label, phase.shortName, phase.durationMs, phase.brightness);
    printHealth();
}

void applyPhaseCommand(size_t phaseIndex, bool lockPhase) {
    g_phaseLocked = lockPhase;
    g_lockedPhaseIndex = phaseIndex % kBurnPhaseCount;
    Serial.printf("[CMD] phase=%s lock=%s\n", kBurnPhases[g_lockedPhaseIndex].shortName,
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
        printHealth();
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
    Serial.printf("MN42 power burn-in firmware %s schema %04X\n", FW_VERSION_STR, CONFIG_VERSION);
    Serial.printf("Reset 0x%08lX (%s) brownouts=%u\n", g_resetCause,
                  resetCauseToString(g_resetCause), g_brownoutCount);
    sys::printReport();
    Serial.printf("LED pin=%u status pin=%u vrefPin=%u totalLeds=%u\n", hwConfig.ledPin,
                  hwConfig.statusLedPin, hwConfig.vrefAdcPin, NUM_LEDS());
    Serial.println("[SAFETY] 52x WS2812 full white can exceed 3A; use regulated 5V supply and "
                   "confirmed fuse topology.");
    printCommandHelp();

    burnLeds().begin();
    randomSeed(micros() ^ g_resetCause ^ HW_OCOTP_CFG0);
    startPhase(0);
}

void loop() {
    unsigned long nowMs = now();
    pollSerialCommands();

    if ((nowMs - g_phaseStartedMs) >= kBurnPhases[g_phaseIndex].durationMs) {
        startPhase(g_phaseLocked ? g_lockedPhaseIndex : (g_phaseIndex + 1));
        nowMs = now();
    }

    const BurnPhase &phase = kBurnPhases[g_phaseIndex];

    if ((nowMs - g_lastVrefSampleMs) >= kVrefSampleMs) {
        g_lastVrefSampleMs = nowMs;
        sampleVref();
    }

    if (phase.style == BurnPhaseStyle::WhiteSweep) {
        applyWhiteSweep(nowMs);
    } else if (phase.style == BurnPhaseStyle::RandomWash) {
        if ((nowMs - g_lastRandomStepMs) >= kRandomWashStepMs) {
            g_lastRandomStepMs = nowMs;
            paintRandomWash();
        }
    } else if (phase.style == BurnPhaseStyle::RandomBlast) {
        paintRandomWash();
        if ((g_phasePasses % kRandomBlastLogStride) == 0u) {
            Serial.printf("[BURN] blast pass=%u writes=%lu vref=%.3f\n", g_phasePasses,
                          g_phasePixelWrites, g_lastVrefVolts);
        }
    }

    if (phase.style == BurnPhaseStyle::RandomWash || phase.style == BurnPhaseStyle::RandomBlast) {
        burnLeds().update();
    }

    burnLeds().setStatusLED(((nowMs / 250) % 2u) == 0u);

    if ((nowMs - g_lastStatusPrintMs) >= kStatusPrintMs) {
        g_lastStatusPrintMs = nowMs;
        printHealth();
    }

    ++g_frameCounter;
    ++g_phaseFrameCounter;
    updatePeakRates(nowMs);

    if (phase.style != BurnPhaseStyle::RandomBlast) {
        delay(20);
    }
}
