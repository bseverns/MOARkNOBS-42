// DisplayManager.cpp — Merged and polished

#include "DisplayManager.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "ButtonManager.h"

 DisplayManager::DisplayManager(uint8_t i2cAddress,
                                uint16_t screenWidth,
                                uint16_t screenHeight)
     : _i2cAddress(i2cAddress),
       _screenWidth(screenWidth),
       _screenHeight(screenHeight),
       _display(screenWidth, screenHeight, &Wire) {
    _isDrawing = false;
    _updateIntervalMs = 100;
    _activePot = 0;
    _activeChannel = 0;
    _activeMode = "MIDI";
    _lastInteractionTime = millis();
}

bool DisplayManager::begin() {
    if (!_display.begin(SSD1306_SWITCHCAPVCC, _i2cAddress)) {
        return false;
    }
    _display.clearDisplay();
    _display.display();
    return true;
}

void DisplayManager::triggerFade(uint16_t ms) {
    _fadeAnim.state = AnimState::FADE_IN;
    _fadeAnim.duration = ms;
    _fadeAnim.lastTime = millis();
    _fadeAnim.brightness = 0;
}

void DisplayManager::updateFadeAnimation() {
    uint32_t now = millis();
    switch (_fadeAnim.state) {
        case AnimState::FADE_IN:
            if (now - _fadeAnim.lastTime >= _fadeAnim.duration) {
                _fadeAnim.state = AnimState::HOLD;
                _fadeAnim.lastTime = now;
                _fadeAnim.brightness = 255;
            } else {
                _fadeAnim.brightness = map(now - _fadeAnim.lastTime, 0, _fadeAnim.duration, 0, 255);
            }
            _display.ssd1306_command(SSD1306_SETCONTRAST, 255 - _fadeAnim.brightness);
            break;
        case AnimState::HOLD:
            if (now - _fadeAnim.lastTime >= 500) {
                _fadeAnim.state = AnimState::FADE_OUT;
                _fadeAnim.lastTime = now;
            }
            break;
        case AnimState::FADE_OUT:
            if (now - _fadeAnim.lastTime >= _fadeAnim.duration) {
                _fadeAnim.state = AnimState::DONE;
                _fadeAnim.brightness = 0;
            } else {
                _fadeAnim.brightness = map(now - _fadeAnim.lastTime, 0, _fadeAnim.duration, 255, 0);
            }
            _display.ssd1306_command(SSD1306_SETCONTRAST, 255 - _fadeAnim.brightness);
            break;
        default:
            break;
    }
}

void DisplayManager::runStartupAnimation() {
    _display.clearDisplay();
    for (int step = 0; step < 5; step++) {
        for (int i = 0; i < _display.width(); i += (1 << step)) {
            _display.drawLine(0, 0, i, _display.height() - 1, SSD1306_WHITE);
            _display.drawLine(_display.width() - 1, _display.height() - 1, _display.width() - 1 - i, 0, SSD1306_WHITE);
        }
        _display.display();
        delay(250);
    }
    delay(500);
    _display.clearDisplay();
    _display.setTextSize(2);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor((_display.width() - 12 * 6) / 2, _display.height() / 2 - 8);
    _display.println("MOARkNOBS-42");
    _display.display();
    delay(1500);
    _display.clearDisplay();
    _display.display();
}

void DisplayManager::runIdleScreensaver() {
    _display.clearDisplay();
    for (int i = 0; i < 20; i++) {
        int x = random(0, _display.width());
        int y = random(0, _display.height());
        _display.drawPixel(x, y, SSD1306_WHITE);
    }
    _display.display();
}

void DisplayManager::registerInteraction() {
    _lastInteractionTime = millis();
}

bool DisplayManager::shouldRunScreensaver() const {
    return (millis() - _lastInteractionTime > 90000);
}


void DisplayManager::drawBorder() {
    _display.drawRect(0, 0, _display.width(), _display.height(), SSD1306_WHITE);
}

void DisplayManager::showText(const char* line1, const char* line2, const char* line3) {
    if (millis() < _statusTimeout) return;

    clear();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    _display.setCursor(0, 0);
    _display.println(line1);

    if (line2 && line2[0] != '\0') {
        _display.setCursor(0, 10);
        _display.println(line2);
    }

    if (line3 && line3[0] != '\0') {
        _display.setCursor(0, 20);
        _display.println(line3);
    }

    drawBorder();
    _display.display();
}

void DisplayManager::showValue(uint8_t value, bool clearDisplay) {
    if (millis() < _statusTimeout) return;

    if (clearDisplay) {
        _display.clearDisplay();
    }

    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.print("Value: ");
    _display.println(value);

    drawBorder();
    _display.display();
}

void DisplayManager::showEnvelopeAssignment(int potIndex, int efIndex, const char* mode, const char* argMethod) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    _display.setCursor(0, 0);
    _display.print("Slot ");
    _display.print(potIndex);
    _display.print(" -> EF ");
    _display.println(efIndex);

    if (mode != nullptr) {
        _display.setCursor(0, 10);
        _display.print("Mode: ");
        _display.println(mode);
    }

    if (mode != nullptr && strcmp(mode, "ARG") == 0 && argMethod != nullptr) {
        _display.setCursor(0, 20);
        _display.print("Method: ");
        _display.println(argMethod);
    }
    drawBorder();
    _display.display();
    _statusTimeout = millis() + NORMAL_DISPLAY_TIME;
}

void DisplayManager::showMode(const char *mode, bool clearDisplay) {
    if (millis() < _statusTimeout) return;

    if (clearDisplay) {
        _display.clearDisplay();
    }

    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.print("Mode: ");
    _display.println(mode);
    drawBorder();
    _display.display();
}

void DisplayManager::clear() {
    if (millis() < _statusTimeout) return;

    _display.clearDisplay();
    _display.display();
}

void DisplayManager::showFilterTuning(float frequency, float q) {
    if (millis() < _statusTimeout) return;  // Added timeout check for consistency

    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.print("Filter Freq: ");
    _display.print(frequency, 1);
    _display.setCursor(0, 10);
    _display.print("Q Factor: ");
    _display.print(q, 2);

    drawBorder();
    _display.display();
}

void DisplayManager::updateDisplay(uint8_t beatPosition, const std::vector<uint8_t>& envelopeLevels, const char* statusMessage, uint8_t activePot, uint8_t activeChannel, const char* envelopeMode) {
    if (millis() < _statusTimeout) return;

    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.print("Beat: ");
    _display.println(beatPosition);
    _display.setCursor(0, 10);
    _display.print("Pot: ");
    _display.print(activePot);
    _display.print(" Ch: ");
    _display.println(activeChannel);
    _display.setCursor(0, 20);
    _display.print("Mode: ");
    _display.println(envelopeMode);

    // Visualize envelope levels as vertical bars along the bottom
    const int numEnvelopes = envelopeLevels.size();
    const int barWidth = 6;
    const int maxHeight = 20;
    const int baseY = _display.height() - 1;

    for (int i = 0; i < numEnvelopes; i++) {
        int barHeight = map(envelopeLevels[i], 0, 127, 0, maxHeight);
        int x = i * (barWidth + 2);
        _display.fillRect(x, baseY - barHeight, barWidth, barHeight, SSD1306_WHITE);
    }

    drawBorder();
    _display.display();

    if (statusMessage && statusMessage[0] != '\0') {
        _statusTimeout = millis() + NORMAL_DISPLAY_TIME;
    }
}

void DisplayManager::displayStatus(const char *status, unsigned long duration) {
    _statusMessage = status;
    _statusTimeout = millis() + duration;

    _display.clearDisplay();
    _display.setTextSize(2);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.println(status);
    _display.display();
}

void DisplayManager::updateFromContext(const ButtonManagerContext& context) {
    if (millis() < _statusTimeout) return;

    _display.clearDisplay();
    _display.setCursor(0, 0);
    _display.print("BTN: ");
    _display.print(context.activePot);
    _display.print(" CH: ");
    _display.println(context.activeChannel);
    _display.setCursor(0, 10);
    _display.print("EF: ");
    _display.println(context.envelopeFollowMode ? "ON" : "OFF");

    if (context.potToEnvelopeMap.count(context.activePot)) {
        int envelopeIndex = context.potToEnvelopeMap.at(context.activePot);
        _display.setCursor(0, 20);
        _display.print("ENV->POT: ");
        _display.println(envelopeIndex);
    }

    _display.display();
}

void DisplayManager::showARGInfo(const char* methodName, int envA, int envB) {
    if (millis() < _statusTimeout) return;

    clear();

    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    _display.setCursor(0, 0);
    _display.print("MODE: ARG");

    _display.setCursor(0, 10);
    _display.print("Method: ");
    _display.println(methodName);

    _display.setCursor(0, 20);
    _display.print("Envs: A=");
    _display.print(envA);
    _display.print(" B=");
    _display.println(envB);

    _display.display();
    _statusTimeout = millis() + NORMAL_DISPLAY_TIME;
}

void DisplayManager::setTemporaryMessage(const char* message, unsigned long duration) {
    _statusMessage = message;
    _statusTimeout = millis() + duration;
    clear();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.println(message);
    _display.display();
}

void DisplayManager::showMIDIMessage(uint8_t cc, uint8_t value, uint8_t channel) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.print("CC: ");
    _display.print(cc);
    _display.print(" Value: ");
    _display.println(value);
    _display.setCursor(0, 10);
    _display.print("Ch: ");
    _display.println(channel);
    _display.display();
    _statusTimeout = millis() + SHORT_DISPLAY_TIME;
}

void DisplayManager::updateBeat(uint8_t beatPosition, bool clockRunning) {
    if (millis() < _statusTimeout) return;

    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);

    if (clockRunning) {
        _display.print("Beat: ");
        _display.println(beatPosition);
    } else {
        _display.println("No Clock");
    }

    _display.display();
}

void DisplayManager::beginDraw() {
    _display.clearDisplay();
    _isDrawing = true;
}

void DisplayManager::endDraw() {
    _display.display();
    _isDrawing = false;
}

void DisplayManager::showError(const char* errorMessage, bool persistent) {
    if (millis() < _statusTimeout) return;

    beginDraw();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.println(F("ERROR:"));
    _display.println(errorMessage);
    endDraw();
    if (persistent) {
        while (1);
    }
}

void DisplayManager::showEnvelopeLevel(uint8_t level) {
    if (millis() < _statusTimeout) return;

    const int barHeight = 10;
    const int barY = SCREEN_HEIGHT - barHeight;
    int barWidth = map(level, 0, 127, 0, SCREEN_WIDTH);
    _display.fillRect(0, barY, SCREEN_WIDTH, barHeight, SSD1306_BLACK);
    _display.fillRect(0, barY, barWidth, barHeight, SSD1306_WHITE);
}

void DisplayManager::showEnvelopeLevels(uint8_t envA, uint8_t envB) {
    if (millis() < _statusTimeout) return;

    const int barHeight = 5;
    const int gap = 2;
    int widthA = map(envA, 0, 127, 0, SCREEN_WIDTH);
    _display.fillRect(0, SCREEN_HEIGHT - barHeight * 2 - gap, SCREEN_WIDTH, barHeight, SSD1306_BLACK);
    _display.fillRect(0, SCREEN_HEIGHT - barHeight * 2 - gap, widthA, barHeight, SSD1306_WHITE);

    int widthB = map(envB, 0, 127, 0, SCREEN_WIDTH);
    _display.fillRect(0, SCREEN_HEIGHT - barHeight, SCREEN_WIDTH, barHeight, SSD1306_BLACK);
    _display.fillRect(0, SCREEN_HEIGHT - barHeight, widthB, barHeight, SSD1306_WHITE);
}

void DisplayManager::updateActiveSelection(uint8_t activePot, uint8_t activeChannel) {
    _activePot = activePot;
    _activeChannel = activeChannel;
}

void DisplayManager::highlightActivePot(uint8_t potIndex) {
    _display.drawRect(5 + potIndex * 10, 50, 8, 8, SSD1306_WHITE);
}

void DisplayManager::highlightActiveMode(const String& modeName) {
    _activeMode = modeName;
    _display.setCursor(0, 56);
    _display.print(F("MODE: "));
    _display.print(_activeMode);
}

void DisplayManager::setUpdateInterval(unsigned long intervalMs) {
    _updateIntervalMs = intervalMs;
}

unsigned long DisplayManager::getUpdateInterval() const {
    return _updateIntervalMs;
}
