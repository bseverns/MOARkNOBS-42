#include "Utility.h"
#include <Arduino.h>
#include "EnvelopeFollower.h"
#include "LEDManager.h"
#include "EEPROM.h"
#include <imxrt.h>

// Mapping and Value Transformations
uint8_t Utility::mapToMidiValue(int analogValue, int minValue, int maxValue) {
    return map(analogValue, minValue, maxValue, 0, 127);
}

int Utility::mapToRange(int value, int inMin, int inMax, int outMin, int outMax) {
    return map(value, inMin, inMax, outMin, outMax);
}

float Utility::mapExponential(float value, float inMin, float inMax, float outMin, float outMax, float exponent) {
    float normalized = (value - inMin) / (inMax - inMin);
    float scaled = pow(normalized, exponent);
    return scaled * (outMax - outMin) + outMin;
}

// Debouncing
bool Utility::debounce(bool& previousState, bool currentState, unsigned long& lastDebounceTime, unsigned long currentTime, unsigned long debounceDelay) {
    if (currentState != previousState) {
        lastDebounceTime = currentTime; // Update debounce time
    }
    if ((currentTime - lastDebounceTime) > debounceDelay) {
        previousState = currentState; // Update state
        return true; // Stable state change
    }
    return false; // Not stable
}

// EEPROM Operations
uint8_t Utility::readEEPROMByte(int address) {
    return EEPROM.read(address);
}

void Utility::writeEEPROMByte(int address, uint8_t value) {
    EEPROM.update(address, value);
}

// Timer Helpers
bool Utility::isTimeElapsed(unsigned long& lastTime, unsigned long interval) {
    unsigned long currentTime = millis();
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
void Utility::logError(const char* errorMessage) {
    Serial.print("[ERROR]: ");
    Serial.println(errorMessage);
}

void Utility::logDebug(const char* debugMessage) {
    Serial.print("[DEBUG]: ");
    Serial.println(debugMessage);
}

// Filtering
int Utility::exponentialMovingAverage(int currentValue, int previousValue, float alpha) {
    return alpha * currentValue + (1 - alpha) * previousValue;
}

// System Operations
void Utility::rebootTeensy() {
    SCB_AIRCR = 0x05FA0004; // System reset for ARM Cortex-M
    while (1);              // Ensure the system halts
}

void Utility::displayCenteredText(Adafruit_SSD1306& display, const char* text) {
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

void Utility::displayStatus(Adafruit_SSD1306& display, const char* status, unsigned long duration) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1); // Standard text size
    display.setTextColor(SSD1306_WHITE);
    display.println(status);
    display.display();
    delay(duration); // Hold the status for the given duration
}

void Utility::updateDisplay(
    Adafruit_SSD1306& display,
    uint8_t beatPosition,
    const std::vector<EnvelopeFollower>& envelopeFollowers,
    const char* statusMessage,
    uint8_t activePot,
    uint8_t activeChannel,
    const char* envelopeMode
) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

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
    for (const auto& follower : envelopeFollowers) {
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
    EEPROM.update(address, value & 0xFF);       // Write low byte
    EEPROM.update(address + 1, (value >> 8));  // Write high byte
}

void Utility::resetEEPROM(int startAddress, int endAddress, uint8_t defaultValue) {
    for (int i = startAddress; i <= endAddress; i++) {
        EEPROM.update(i, defaultValue);
    }
}

void Utility::processBulkUpdate(const String& command, uint8_t numPots) {
    if (!command.startsWith("SET_ALL")) {
        Serial.println("Error: Command must start with 'SET_ALL'");
        return;
    }

    int startIdx = 8; // Skip "SET_ALL "
    unsigned int currentPot = 0;

    while (static_cast<unsigned int>(startIdx) < static_cast<unsigned int>(command.length()) &&
           currentPot < static_cast<unsigned int>(numPots)) {
        int ccEnd = command.indexOf(',', startIdx);
        int channelEnd = command.indexOf(';', startIdx);

        if (ccEnd == -1 || channelEnd == -1 || ccEnd >= channelEnd) {
            Serial.println("Error: Malformed command");
            return;
        }

        int ccNumber = command.substring(startIdx, ccEnd).toInt();
        int channel = command.substring(ccEnd + 1, channelEnd).toInt();

        // Validate CC number and channel
        if (ccNumber < 0 || ccNumber > 127 || channel < 1 || channel > 16) {
            Serial.println("Error: Invalid CC number or channel");
            return;
        }

        // Update EEPROM (2 bytes per potentiometer: channel and CC number)
        int address = currentPot * 2;
        EEPROM.update(address, channel);
        EEPROM.update(address + 1, ccNumber);

        currentPot++;
        startIdx = channelEnd + 1;
    }

    if (currentPot == static_cast<unsigned int>(numPots)) {
        Serial.println("Bulk update successful");
    } else {
        Serial.println("Error: Insufficient data for all pots");
    }
}

// --- Task Struct ---
ScheduledTask::ScheduledTask(std::function<void()> cb, unsigned long intv)
    : callback(cb), interval(intv), lastRun(0) {}

// --- Task Scheduler ---
void TaskScheduler::addTask(std::function<void()> callback, unsigned long interval) {
    tasks.emplace_back(callback, interval);
}

void TaskScheduler::update() {
    unsigned long now = millis();
    for (auto& task : tasks) {
        if (now - task.lastRun >= task.interval) {
            task.callback();
            task.lastRun = now;
        }
    }
}

TaskScheduler Utility::schedulerHigh;
TaskScheduler Utility::schedulerMid;
TaskScheduler Utility::schedulerLow;
