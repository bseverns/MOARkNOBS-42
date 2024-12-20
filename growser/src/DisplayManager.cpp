#include "DisplayManager.h"

DisplayManager::DisplayManager(uint8_t i2cAddress, uint16_t width, uint16_t height)
    : _display(width, height, &Wire, -1), _i2cAddress(i2cAddress) {}

void DisplayManager::begin() {
    if (!_display.begin(_i2cAddress)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); // Halt if the display fails to initialize
    }
    _display.clearDisplay();
    _display.display();
}

void DisplayManager::showText(const char *text, bool drawBorder) {
    _display.clearDisplay();
    if (drawBorder) {
        _display.drawRect(0, 0, _display.width(), _display.height(), SSD1306_WHITE);
    }

    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.print(text);
    _display.display();
}

void DisplayManager::showValue(uint8_t value, bool drawBorder) {
    _display.clearDisplay();

    if (drawBorder) {
        _display.drawRect(0, 0, _display.width(), _display.height(), SSD1306_WHITE);
    }

    _display.setTextSize(2);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.print(value);
    _display.display();
}

void DisplayManager::showMode(const char *mode, bool drawBorder) {
    _display.clearDisplay();

    if (drawBorder) {
        _display.drawRect(0, 0, _display.width(), _display.height(), SSD1306_WHITE);
    }

    _display.setTextSize(2);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.print(mode);
    _display.display();
}

void DisplayManager::clear() {
    _display.clearDisplay();
    _display.display();
}

void DisplayManager::showMIDIClock(uint8_t beatPosition) {
    _display.clearDisplay();
    _display.drawCircle(_display.width() / 2, _display.height() / 2, 10, SSD1306_WHITE);
    float angle = beatPosition * (PI / 4); // Map beat position to 8 segments
    int x = (_display.width() / 2) + 10 * cos(angle);
    int y = (_display.height() / 2) - 10 * sin(angle);
    _display.fillCircle(x, y, 3, SSD1306_WHITE);
    _display.display();
}

void DisplayManager::showEnvelopeLevel(uint8_t level) {
    _display.clearDisplay();
    int barHeight = map(level, 0, 127, 0, _display.height());
    _display.fillRect(_display.width() - 10, _display.height() - barHeight, 5, barHeight, SSD1306_WHITE);
    _display.display();
}

void DisplayManager::showDigitalSnow() {
    _display.clearDisplay();
    for (int i = 0; i < 20; i++) {
        int x = random(0, _display.width());
        int y = random(0, _display.height());
        _display.drawPixel(x, y, SSD1306_WHITE);
    }
    _display.display();
}

void DisplayManager::displayStatus(const char *status, unsigned long duration) {
    _statusMessage = status;
    _statusTimeout = millis() + duration; // Set the expiration time
    _display.clearDisplay();
    _display.setTextSize(2);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.print(_statusMessage);
    _display.display();
}

void DisplayManager::updateDisplay(uint8_t beatPosition, const std::vector<uint8_t>& envelopeLevels, const char* statusMessage, uint8_t activePot, uint8_t activeChannel) {
    _display.clearDisplay();

    // Top Section: MIDI Clock
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.print("Clock:");
    _display.setCursor(50, 0);
    _display.print(beatPosition);
    _display.drawCircle(_display.width() / 2, 10, 10, SSD1306_WHITE);
    float angle = beatPosition * (PI / 4);
    int x = (_display.width() / 2) + 10 * cos(angle);
    int y = 10 - 10 * sin(angle);
    _display.fillCircle(x, y, 3, SSD1306_WHITE);

    // Middle Section: Envelope Bars
    int barWidth = 20;
    int spacing = 5;
    int barHeight = _display.height() - 30; // Adjust for bottom section
    int xPos = 0;
    for (const auto& level : envelopeLevels) {
        int scaledHeight = map(level, 0, 127, 0, barHeight);
        _display.fillRect(xPos, barHeight - scaledHeight + 15, barWidth, scaledHeight, SSD1306_WHITE);
        xPos += barWidth + spacing;
    }

    // Bottom Section: Status and Active Pot/Channel
    _display.setCursor(0, _display.height() - 15);
    _display.print("Pot:");
    _display.print(activePot);
    _display.print(" Chan:");
    _display.print(activeChannel);
    _display.setCursor(80, _display.height() - 15);
    _display.print(statusMessage);

    _display.display();
}