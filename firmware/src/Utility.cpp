// Utility.cpp is the grab bag where we stash math tricks, timing helpers, and
// display conveniences that would otherwise clutter the core modules. The
// commentary leans toward teaching: every helper is contextualized so students
// know when to reach for it and what trade-offs lurk under the hood.

#include "Utility.h"
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include "Hardware/IO.h"
#include "TimeUtils.h"
#include "EnvelopeFollower.h"
#include "LEDManager.h"
#include "MIDIHandler.h"
#include "ConfigManager.h"
#include <imxrt.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <limits>
#include "Log.h"

// Collection of helpers used across the firmware. These range from value
// mappings and EEPROM utilities to simple schedulers that run tasks at different
// priorities.

// Mapping and Value Transformations

// Default time source; tests can override by defining their own now().
unsigned long __attribute__((weak)) now() { return millis(); }

namespace {
bool looksLikeBulkConfigFrameStart(const String &chunk) {
    return chunk.startsWith("{\"seq\"") || chunk.startsWith("{\"checksum\"") ||
           chunk.startsWith("{\"config_id\"") || chunk.startsWith("{\"config\"");
}
} // namespace

// Map a raw analog reading into the 7-bit MIDI range used by most messages.
uint8_t Utility::mapToMidiValue(int analogValue, int minValue, int maxValue) {
    return map(analogValue, minValue, maxValue, 0, 127);
}

// Map a raw analog reading into the full 14-bit range used by pitch bend/NRPN style payloads.
uint16_t Utility::mapTo14Bit(int analogValue, int minValue, int maxValue) {
    return static_cast<uint16_t>(map(analogValue, minValue, maxValue, 0, 16383));
}

// Generic integer remap helper used by UI/firmware code that needs explicit bounds.
int Utility::mapToRange(int value, int inMin, int inMax, int outMin, int outMax) {
    return map(value, inMin, inMax, outMin, outMax);
}

// Floating-point version of map() with zero-range protection.
float Utility::scale(float value, float inMin, float inMax, float outMin, float outMax) {
    const float range = inMax - inMin;
    if (std::fabs(range) <= std::numeric_limits<float>::epsilon()) {
        return outMin;
    }
    const float ratio = (value - inMin) / range;
    return outMin + ratio * (outMax - outMin);
}

// Apply a power curve after normalizing into 0..1, then scale into the target range.
float Utility::mapExponential(float value, float inMin, float inMax, float outMin, float outMax,
                              float exponent) {
    const float range = inMax - inMin;
    if (std::fabs(range) <= std::numeric_limits<float>::epsilon()) {
        return outMin;
    }
    float normalized = (value - inMin) / range;
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    const float scaled = std::pow(normalized, exponent);
    return scaled * (outMax - outMin) + outMin;
}

// Note On/Off scheduling
// Fire a note immediately and schedule its note-off on the shared high-priority scheduler.
void Utility::scheduleNoteOnOff(MIDIHandler &midiHandler, uint8_t note, uint8_t velocity,
                                uint8_t channel, unsigned long durationMs) {
    midiHandler.sendNoteOn(note, velocity, channel);
    Utility::schedulerHigh.addTask(
        [note, channel, &midiHandler]() { midiHandler.sendNoteOff(note, 0, channel); }, durationMs,
        false);
}

// Debouncing
/*
`stableState`     holds the last rock-solid reading.
`lastRawState`    tracks the most recent unconfirmed sample so bouncing
                   edges can restart the timer without losing the stable
                   output state.
`currentState`    the raw sample we're checking out right now.
`lastDebounceTime` time stamp of the most recent flip.
`debounceDelay`   minimum interval the input has to keep screaming the same
                   value before we believe it.
*/
bool Utility::debounce(bool &stableState, bool &lastRawState, bool currentState,
                       unsigned long &lastDebounceTime, unsigned long currentTime,
                       unsigned long debounceDelay) {
    if (currentState != lastRawState) {
        lastDebounceTime = currentTime;
        lastRawState = currentState;
    }
    if ((currentTime - lastDebounceTime) >= debounceDelay && stableState != lastRawState) {
        stableState = lastRawState;
        return true;
    }
    return false;
}

// EEPROM Operations
// Read one byte from EEPROM without any extra interpretation.
uint8_t Utility::readEEPROMByte(int address) {
    return ConfigManager::getStorageBackend()->read(address);
}

// Update one EEPROM byte, relying on EEPROM.update() to avoid unnecessary wear.
void Utility::writeEEPROMByte(int address, uint8_t value) {
    ConfigManager::getStorageBackend()->update(address, value);
}

// Timer Helpers
// Check whether an interval has elapsed and roll the caller's timestamp forward if so.
bool Utility::isTimeElapsed(unsigned long &lastTime, unsigned long interval) {
    unsigned long currentTime = now();
    if ((currentTime - lastTime) >= interval) {
        lastTime = currentTime; // Reset timer
        return true;
    }
    return false;
}

// LED Utilities
// Blend between two colors based on a 7-bit MIDI value.
CRGB Utility::mapValueToColor(uint8_t value, CRGB lowColor, CRGB highColor) {
    return blend(lowColor, highColor, map(value, 0, 127, 0, 255));
}

// Debugging
// Emit a standardized error log line.
void Utility::logError(const char *errorMessage) {
    LOG_PRINT("[ERROR]: ");
    LOG_PRINTLN(errorMessage);
}

// Emit a standardized debug log line.
void Utility::logDebug(const char *debugMessage) {
    LOG_PRINT("[DEBUG]: ");
    LOG_PRINTLN(debugMessage);
}

// Filtering
// Simple EWMA used throughout the repo to calm noisy sensor values.
int Utility::exponentialMovingAverage(int currentValue, int previousValue, float alpha) {
    const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
    const float weighted = (clampedAlpha * static_cast<float>(currentValue)) +
                           ((1.0f - clampedAlpha) * static_cast<float>(previousValue));
    const long rounded = std::lround(weighted);
    if (rounded > std::numeric_limits<int>::max()) {
        return std::numeric_limits<int>::max();
    }
    if (rounded < std::numeric_limits<int>::min()) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(rounded);
}

// System Operations
// Force a Cortex-M software reset when the firmware needs a clean restart.
void Utility::rebootTeensy() {
    SCB_AIRCR = 0x05FA0004; // System reset for ARM Cortex-M
    while (1)
        ; // Ensure the system halts
}

// Center one line of text on the OLED and show it immediately.
void Utility::displayCenteredText(Adafruit_SSD1306 &display, const char *text) {
    const char *safeText = text ? text : "";
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(safeText, 0, 0, &x1, &y1, &w, &h);

    int x = (display.width() - w) / 2;
    int y = (display.height() - h) / 2;

    display.clearDisplay();
    display.setCursor(x, y);
    display.print(safeText);
    display.display();
}

// Adafruit keeps flip-flopping between SSD1306_WHITE and SSD1306_COLOR_WHITE. We don't
// particularly care which fad is in vogue, we just want white pixels. Route everything
// through a helper that resolves whichever constant exists at compile time and falls back to
// the traditional "1 means on" vibe if neither macro shows up.
namespace {
inline uint16_t resolveDisplayWhite() {
#if defined(SSD1306_WHITE)
    return SSD1306_WHITE;
#elif defined(SSD1306_COLOR_WHITE)
    return SSD1306_COLOR_WHITE;
#else
    return 1;
#endif
}
} // namespace

void Utility::displayStatus(Adafruit_SSD1306 &display, const char *status, unsigned long duration) {
    const char *safeStatus = status ? status : "";
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1); // Standard text size
    const uint16_t textColor = resolveDisplayWhite();
    display.setTextColor(textColor);
    display.println(safeStatus);
    display.display();
    delay(duration); // Hold the status for the given duration
}

// Paint the older all-in-one status screen used by some diagnostic and workshop flows.
void Utility::updateDisplay(Adafruit_SSD1306 &display, uint8_t beatPosition,
                            const std::vector<EnvelopeFollower> &envelopeFollowers,
                            const char *statusMessage, uint8_t activePot, uint8_t activeChannel,
                            const char *envelopeMode) {
    const char *safeStatus = statusMessage ? statusMessage : "";
    const char *safeMode = envelopeMode ? envelopeMode : "";
    display.clearDisplay();
    display.setTextSize(1);
    const uint16_t textColor = resolveDisplayWhite();
    display.setTextColor(textColor);

    // Display beat position
    display.setCursor(0, 0);
    display.print("Beat: ");
    display.println(beatPosition);

    // Display active pot and channel
    display.setCursor(0, 10);
    display.print("Pot: ");
    display.print(activePot);
    display.print(" Ch: ");
    display.println(activeChannel);

    // Display envelope mode
    display.setCursor(0, 20);
    display.print("Mode: ");
    display.println(safeMode);

    // Display envelope levels
    display.setCursor(0, 30);
    display.print("Env: ");
    for (const auto &follower : envelopeFollowers) {
        display.print(follower.getEnvelopeLevel());
        display.print(" ");
    }

    // Display status message
    display.setCursor(0, 40);
    display.print("Status: ");
    display.println(safeStatus);

    display.display();
}

// Read a little-endian 16-bit word from EEPROM.
uint16_t Utility::readEEPROMWord(int address) {
    uint8_t low = ConfigManager::getStorageBackend()->read(address);
    uint8_t high = ConfigManager::getStorageBackend()->read(address + 1);
    return (high << 8) | low;
}

void Utility::writeEEPROMWord(int address, uint16_t value) {
    ConfigManager::getStorageBackend()->update(address, value & 0xFF);     // Write low byte
    ConfigManager::getStorageBackend()->update(address + 1, (value >> 8)); // Write high byte
}

void Utility::resetEEPROM(int startAddress, int endAddress, uint8_t defaultValue) {
    if (startAddress > endAddress) {
        return;
    }
    for (int i = startAddress; i <= endAddress; i++) {
        ConfigManager::getStorageBackend()->update(i, defaultValue);
    }
}

void Utility::BulkConfigAssembler::reset() {
    receiving = false;
    buffer = "";
    seqHint = 0;
    checksum = "";
    braceDepth = 0;
    inString = false;
    escaped = false;
    sawRootOpen = false;
}

bool Utility::BulkConfigAssembler::ingestChunk(const String &chunk, String &error) {
    if (chunk.length() == 0) {
        return true; // Ignore empty fragments.
    }

    // Detect only top-level frame starts. Later chunks may legitimately begin
    // with nested JSON objects when the browser splits the upload mid-array.
    const bool startsObject = chunk.charAt(0) == '{';
    const bool startsNewFrame = startsObject && (!receiving || buffer.length() == 0 ||
                                                 looksLikeBulkConfigFrameStart(chunk));
    if (startsNewFrame) {
        reset();
        receiving = true;
    } else if (!receiving) {
        error = "orphan";
        return false;
    }

    if (buffer.length() + chunk.length() > Utility::kMaxBulkConfigSize) {
        error = "overflow";
        reset();
        return false;
    }

    buffer.reserve(buffer.length() + chunk.length());
    buffer += chunk;
    receiving = true;
    updateCompletionState(chunk);
    refreshHints();
    return true;
}

void Utility::BulkConfigAssembler::updateCompletionState(const String &chunk) {
    for (size_t i = 0; i < chunk.length(); ++i) {
        const char c = chunk.charAt(i);
        if (escaped) {
            escaped = false;
            continue;
        }
        if (inString) {
            if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (c == '"') {
            inString = true;
        } else if (c == '{') {
            sawRootOpen = true;
            ++braceDepth;
        } else if (c == '}' && braceDepth > 0) {
            --braceDepth;
        }
    }
}

void Utility::BulkConfigAssembler::refreshHints() {
    if (seqHint == 0) {
        int key = buffer.indexOf("\"seq\"");
        if (key >= 0) {
            int colon = buffer.indexOf(':', key);
            if (colon >= 0) {
                int start = colon + 1;
                while (start < buffer.length() &&
                       isspace(static_cast<unsigned char>(buffer[start]))) {
                    ++start;
                }
                int end = start;
                while (end < buffer.length() && isdigit(static_cast<unsigned char>(buffer[end]))) {
                    ++end;
                }
                if (end > start) {
                    seqHint = buffer.substring(start, end).toInt();
                }
            }
        }
    }

    if (checksum.length() == 0) {
        int key = buffer.indexOf("\"config_id\"");
        if (key < 0) {
            key = buffer.indexOf("\"checksum\"");
        }
        if (key >= 0) {
            int colon = buffer.indexOf(':', key);
            if (colon >= 0) {
                int start = colon + 1;
                while (start < buffer.length() &&
                       isspace(static_cast<unsigned char>(buffer[start]))) {
                    ++start;
                }
                if (start < buffer.length() && buffer[start] == '"') {
                    ++start;
                    int end = buffer.indexOf('"', start);
                    if (end > start) {
                        checksum = buffer.substring(start, end);
                    }
                }
            }
        }
    }
}

String Utility::formatAck(const char *checksumValue, uint32_t sequence,
                          const char *appliedChecksum, uint32_t storageGeneration) {
    String out = "{\"type\":\"ack\"";
    if (sequence != 0) {
        out += ",\"seq\":";
        out += sequence;
    }
    out += ",\"checksum\":\"";
    if (checksumValue) {
        out += checksumValue;
    }
    out += "\"";
    if (checksumValue) {
        out += ",\"request_checksum\":\"";
        out += checksumValue;
        out += "\"";
    }
    if (appliedChecksum) {
        out += ",\"applied_checksum\":\"";
        out += appliedChecksum;
        out += "\"";
    }
    if (storageGeneration != 0) {
        out += ",\"storage_generation\":";
        out += storageGeneration;
    }
    out += "}";
    return out;
}

TaskScheduler::TaskScheduler() {
    tasks.reserve(kReservedTaskCapacity);
    dueTaskIndices.reserve(kReservedTaskCapacity);
}

void TaskScheduler::addTask(std::function<void()> callback, unsigned long delayMs, bool repeat) {
    tasks.emplace_back(callback, delayMs, repeat);
    if (dueTaskIndices.capacity() < tasks.capacity()) {
        dueTaskIndices.reserve(tasks.capacity());
    }
}

void TaskScheduler::update() {
    unsigned long now = ::now();

    // Stage due task indices and mark which one-shot tasks need culling.
    dueTaskIndices.clear();

    for (size_t i = 0; i < tasks.size(); ++i) {
        ScheduledTask &task = tasks[i];
        if (now >= task.runAt) {
            if (task.repeat && task.interval > 0 && now > task.runAt + task.interval) {
                g_systemDiagnostics.schedulerMissedRuns +=
                    static_cast<uint32_t>((now - task.runAt) / task.interval);
            }
            dueTaskIndices.push_back(i);
            if (task.repeat) {
                task.runAt = now + task.interval; // reschedule next run
            } else {
                task.runAt = 0; // sentinel: mark for removal after firing
            }
        }
    }

    // Run callbacks outside of the bookkeeping loop.
    for (size_t index : dueTaskIndices) {
        if (index < tasks.size()) {
            const uint32_t started = micros();
            tasks[index].callback();
            const uint32_t elapsed = micros() - started;
            if (elapsed > g_systemDiagnostics.schedulerMaxTaskMicros) {
                g_systemDiagnostics.schedulerMaxTaskMicros = elapsed;
            }
        }
    }

    // Sweep completed one-shots in O(n) using erase-remove_if instead of
    // the previous reverse-erase loop that was O(n²) per removal.
    tasks.erase(std::remove_if(tasks.begin(), tasks.end(),
                               [](const ScheduledTask &t) { return !t.repeat && t.runAt == 0; }),
                tasks.end());
}

TaskScheduler Utility::schedulerHigh;
TaskScheduler Utility::schedulerMid;
TaskScheduler Utility::schedulerLow;

float Utility::readVrefADC(uint8_t pin) {
    const uint8_t samples = 4;
    uint32_t total = 0;
    for (uint8_t i = 0; i < samples; ++i) {
        total += hardware::readAnalog(pin);
        delayMicroseconds(10);
    }
    float avg = static_cast<float>(total) / samples;
    return avg * VadcScale;
}
