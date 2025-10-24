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
#include "EEPROM.h"
#include "MIDIHandler.h"
#include "ConfigManager.h"
#include <imxrt.h>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include "Log.h"

// Collection of helpers used across the firmware. These range from value
// mappings and EEPROM utilities to simple schedulers that run tasks at different
// priorities.

// Mapping and Value Transformations

// Default time source; tests can override by defining their own now().
unsigned long __attribute__((weak)) now() { return millis(); }
uint8_t Utility::mapToMidiValue(int analogValue, int minValue, int maxValue) {
    return map(analogValue, minValue, maxValue, 0, 127);
}

uint16_t Utility::mapTo14Bit(int analogValue, int minValue, int maxValue) {
    return static_cast<uint16_t>(map(analogValue, minValue, maxValue, 0, 16383));
}

int Utility::mapToRange(int value, int inMin, int inMax, int outMin, int outMax) {
    return map(value, inMin, inMax, outMin, outMax);
}

float Utility::scale(float value, float inMin, float inMax, float outMin, float outMax) {
    if (inMax - inMin == 0) {
        return outMin;
    }
    float ratio = (value - inMin) / (inMax - inMin);
    return outMin + ratio * (outMax - outMin);
}

float Utility::mapExponential(float value, float inMin, float inMax, float outMin, float outMax,
                              float exponent) {
    float normalized = (value - inMin) / (inMax - inMin);
    float scaled = pow(normalized, exponent);
    return scaled * (outMax - outMin) + outMin;
}

// Note On/Off scheduling
void Utility::scheduleNoteOnOff(MIDIHandler &midiHandler, uint8_t note, uint8_t velocity,
                                uint8_t channel, unsigned long durationMs) {
    midiHandler.sendNoteOn(note, velocity, channel);
    Utility::schedulerHigh.addTask(
        [note, channel, &midiHandler]() { midiHandler.sendNoteOff(note, 0, channel); }, durationMs,
        false);
}

// Debouncing
/*
 * `previousState`   holds the last rock-solid reading and gets clobbered when
 *                    a new state proves itself.
 * `currentState`    the raw sample we're checking out right now.
 * `lastDebounceTime` time stamp of the most recent flip.
 * `debounceDelay`   minimum interval the input has to keep screaming the same
 *                    value before we believe it.
 */
bool Utility::debounce(bool &previousState, bool currentState, unsigned long &lastDebounceTime,
                       unsigned long currentTime, unsigned long debounceDelay) {
    if (currentState != previousState) {
        lastDebounceTime = currentTime; // Update debounce time
    }
    if ((currentTime - lastDebounceTime) > debounceDelay) {
        previousState = currentState; // Update state
        return true;                  // Stable state change
    }
    return false; // Not stable
}

// EEPROM Operations
uint8_t Utility::readEEPROMByte(int address) { return EEPROM.read(address); }

void Utility::writeEEPROMByte(int address, uint8_t value) { EEPROM.update(address, value); }

// Timer Helpers
bool Utility::isTimeElapsed(unsigned long &lastTime, unsigned long interval) {
    unsigned long currentTime = now();
    if ((currentTime - lastTime) >= interval) {
        lastTime = currentTime; // Reset timer
        return true;
    }
    return false;
}

// LED Utilities
CRGB Utility::mapValueToColor(uint8_t value, CRGB lowColor, CRGB highColor) {
    return blend(lowColor, highColor, map(value, 0, 127, 0, 255));
}

// Debugging
void Utility::logError(const char *errorMessage) {
    LOG_PRINT("[ERROR]: ");
    LOG_PRINTLN(errorMessage);
}

void Utility::logDebug(const char *debugMessage) {
    LOG_PRINT("[DEBUG]: ");
    LOG_PRINTLN(debugMessage);
}

// Filtering
int Utility::exponentialMovingAverage(int currentValue, int previousValue, float alpha) {
    return alpha * currentValue + (1 - alpha) * previousValue;
}

// System Operations
void Utility::rebootTeensy() {
    SCB_AIRCR = 0x05FA0004; // System reset for ARM Cortex-M
    while (1)
        ; // Ensure the system halts
}

void Utility::displayCenteredText(Adafruit_SSD1306 &display, const char *text) {
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

    int x = (display.width() - w) / 2;
    int y = (display.height() - h) / 2;

    display.clearDisplay();
    display.setCursor(x, y);
    display.print(text);
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
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1); // Standard text size
    const uint16_t textColor = resolveDisplayWhite();
    display.setTextColor(textColor);
    display.println(status);
    display.display();
    delay(duration); // Hold the status for the given duration
}

void Utility::updateDisplay(Adafruit_SSD1306 &display, uint8_t beatPosition,
                            const std::vector<EnvelopeFollower> &envelopeFollowers,
                            const char *statusMessage, uint8_t activePot, uint8_t activeChannel,
                            const char *envelopeMode) {
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
    display.println(envelopeMode);

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
    display.println(statusMessage);

    display.display();
}

uint16_t Utility::readEEPROMWord(int address) {
    uint8_t low = EEPROM.read(address);
    uint8_t high = EEPROM.read(address + 1);
    return (high << 8) | low;
}

void Utility::writeEEPROMWord(int address, uint16_t value) {
    EEPROM.update(address, value & 0xFF);     // Write low byte
    EEPROM.update(address + 1, (value >> 8)); // Write high byte
}

void Utility::resetEEPROM(int startAddress, int endAddress, uint8_t defaultValue) {
    for (int i = startAddress; i <= endAddress; i++) {
        EEPROM.update(i, defaultValue);
    }
}

void Utility::BulkConfigAssembler::reset() {
    receiving = false;
    buffer = "";
    seqHint = 0;
    checksum = "";
}

bool Utility::BulkConfigAssembler::ingestChunk(const String &chunk, String &error) {
    if (chunk.length() == 0) {
        return true; // Ignore empty fragments.
    }

    // Detect the start of a new frame.
    if (chunk.charAt(0) == '{') {
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
    refreshHints();
    return true;
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
        int key = buffer.indexOf("\"checksum\"");
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

String Utility::formatAck(const char *checksumValue, uint32_t sequence) {
    String out = "{\"type\":\"ack\"";
    if (sequence != 0) {
        out += ",\"seq\":";
        out += sequence;
    }
    out += ",\"checksum\":\"";
    if (checksumValue) {
        out += checksumValue;
    }
    out += "\"}";
    return out;
}

void TaskScheduler::addTask(std::function<void()> callback, unsigned long delayMs, bool repeat) {
    tasks.emplace_back(callback, delayMs, repeat);
}

void TaskScheduler::update() {
    unsigned long now = ::now();

    // Stage callbacks and track which one-shot tasks need culling.
    std::vector<std::function<void()>> dueCallbacks;
    std::vector<size_t> finished;

    for (size_t i = 0; i < tasks.size(); ++i) {
        ScheduledTask &task = tasks[i];
        if (now >= task.runAt) {
            dueCallbacks.push_back(task.callback);
            if (task.repeat) {
                task.runAt = now + task.interval; // reschedule next run
            } else {
                finished.push_back(i); // mark for removal
            }
        }
    }

    // Run callbacks outside of the bookkeeping loop.
    for (auto &cb : dueCallbacks) {
        cb();
    }

    // Remove completed one-shot tasks, highest index first.
    for (auto it = finished.rbegin(); it != finished.rend(); ++it) {
        tasks.erase(tasks.begin() + *it);
    }
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
