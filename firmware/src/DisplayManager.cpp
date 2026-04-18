// DisplayManager shows how we choreograph pixels with purpose. The comments in
// this file lean into the “why”: why we stage startup animations, why fades
// tweak contrast directly, and how modules feed data into the OLED without
// tripping over each other's buffers. Treat it like a gig poster explaining the
// set list.

#include <Arduino.h>
#include "DisplayManager.h"
#include "TimeUtils.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include "ButtonManager.h"
#include "MIDIHandler.h"
#include "Globals.h"

// Manages the OLED display. Other modules report user interaction and system
// state here so the screen always reflects the latest configuration.
// Updates are triggered from the low-priority scheduler in firmware_main.cpp.

namespace {

constexpr uint32_t kDisplayI2CFastHz = 400000UL;
constexpr uint32_t kDisplayI2CRestoreHz = 100000UL;
constexpr uint8_t kI2CDataPrefix = 0x40;
constexpr uint8_t kI2CDataChunkBytes = 16;
constexpr unsigned long kPeriodicFullRefreshMs = 5000UL;
constexpr uint8_t kPartialFallbackPercent = 70;
#if defined(MN42_OLED_PARTIAL_UPDATES) && (MN42_OLED_PARTIAL_UPDATES == 0)
constexpr bool kEnablePartialRegionUpdates = false;
#else
constexpr bool kEnablePartialRegionUpdates = true;
#endif

// Human-readable EF filter labels for the OLED detail views.
const char *efFilterLabel(MIDISlot::EfSettings::FilterType type) {
    switch (type) {
    case MIDISlot::EfSettings::FilterType::Linear:
        return "Linear";
    case MIDISlot::EfSettings::FilterType::OppositeLinear:
        return "Opp Linear";
    case MIDISlot::EfSettings::FilterType::Exponential:
        return "Exponential";
    case MIDISlot::EfSettings::FilterType::Random:
        return "Random";
    case MIDISlot::EfSettings::FilterType::Lowpass:
        return "Lowpass";
    case MIDISlot::EfSettings::FilterType::Highpass:
        return "Highpass";
    case MIDISlot::EfSettings::FilterType::Bandpass:
        return "Bandpass";
    }
    return "?";
}

// Convert schema/telemetry-style envelope mode names into friendlier OLED copy.
const char *friendlyEnvelopeMode(const char *raw, char *buffer, size_t bufferLen) {
    if (!raw || raw[0] == '\0') {
        return "Linear";
    }
    if (strcmp(raw, "LINEAR") == 0) {
        return "Linear";
    }
    if (strcmp(raw, "EXPONENTIAL") == 0) {
        return "Exponential";
    }
    if (strcmp(raw, "OPPOSITE_LINEAR") == 0) {
        return "Opposite Linear";
    }
    if (strcmp(raw, "LOWPASS") == 0) {
        return "Lowpass";
    }
    if (strcmp(raw, "HIGHPASS") == 0) {
        return "Highpass";
    }
    if (strcmp(raw, "BANDPASS") == 0) {
        return "Bandpass";
    }
    if (strcmp(raw, "RANDOM") == 0) {
        return "Random";
    }
    if (strcmp(raw, "SEF") == 0 || strcmp(raw, "ARG") == 0 || strcmp(raw, "LOG") == 0 ||
        strcmp(raw, "RMS") == 0 || strcmp(raw, "GATE") == 0 || strcmp(raw, "PEAK") == 0) {
        return raw;
    }
    if (buffer == nullptr || bufferLen == 0) {
        return raw;
    }

    size_t out = 0;
    bool capNext = true;
    for (size_t i = 0; raw[i] != '\0'; ++i) {
        char c = raw[i];
        if (c == '_' || c == '-') {
            if (out > 0 && buffer[out - 1] != ' ' && out + 1 < bufferLen) {
                buffer[out++] = ' ';
            }
            capNext = true;
            continue;
        }

        if (out + 1 >= bufferLen) {
            break;
        }
        unsigned char uc = static_cast<unsigned char>(c);
        buffer[out++] = static_cast<char>(capNext ? std::toupper(uc) : std::tolower(uc));
        capNext = (c == ' ');
    }

    while (out > 0 && buffer[out - 1] == ' ') {
        --out;
    }
    buffer[out] = '\0';

    return out > 0 ? buffer : raw;
}

} // namespace

DisplayManager::DisplayManager(uint8_t i2cAddress, uint16_t screenWidth, uint16_t screenHeight)
    : _display(screenWidth, screenHeight, &Wire), _i2cAddress(i2cAddress) {
    _statusTimeout = 0;
    _isDrawing = false;
    _updateIntervalMs = 33;
    _frameBufferBytes = std::min<std::size_t>(
        kMaxFrameBufferBytes, static_cast<std::size_t>(screenWidth) *
                                  ((static_cast<std::size_t>(screenHeight) + 7U) / 8U));
    _activePot = 0;
    _activeChannel = 0;
    _activeMode = "MIDI";
    _lastInteractionTime = now();
}

// Bring up the OLED hardware and clear any startup garbage.
bool DisplayManager::begin() {
    if (!_display.begin(SSD1306_SWITCHCAPVCC, _i2cAddress)) {
        _initialized = false;
        return false;
    }
    _initialized = true;
    _display.clearDisplay();
    _display.display();
    _shadowValid = false;
    _lastDisplayPushMs = now();
    _lastFullRefreshMs = _lastDisplayPushMs;
    syncShadowBuffer();
    return true;
}

bool DisplayManager::isReady() const { return _initialized; }

bool DisplayManager::isStartupAnimationDone() const {
    return !_initialized || _startupAnim.phase == StartupPhase::DONE;
}

void DisplayManager::syncShadowBuffer() {
    if (_frameBufferBytes == 0) {
        _shadowValid = false;
        return;
    }
    uint8_t *buffer = _display.getBuffer();
    if (buffer == nullptr) {
        _shadowValid = false;
        return;
    }
    memcpy(_lastPushedFrame.data(), buffer, _frameBufferBytes);
    _shadowValid = true;
}

void DisplayManager::present(bool force) {
    if (!_initialized) {
        return;
    }
    if (_isDrawing && !force) {
        return;
    }

    const unsigned long current = now();
    if (!force && _updateIntervalMs > 0 &&
        static_cast<unsigned long>(current - _lastDisplayPushMs) < _updateIntervalMs) {
        return;
    }

    uint8_t *buffer = _display.getBuffer();
    if (!_shadowValid || buffer == nullptr || _frameBufferBytes == 0) {
        _display.display();
        syncShadowBuffer();
        _lastDisplayPushMs = current;
        _lastFullRefreshMs = current;
        return;
    }

    const int16_t width = _display.width();
    const int16_t height = _display.height();
    if (width <= 0 || height <= 0) {
        _display.display();
        syncShadowBuffer();
        _lastDisplayPushMs = current;
        _lastFullRefreshMs = current;
        return;
    }

    const uint8_t pages = std::min<uint8_t>(static_cast<uint8_t>((height + 7) / 8),
                                            static_cast<uint8_t>((OLED_HEIGHT + 7) / 8));
    std::array<int16_t, (OLED_HEIGHT + 7) / 8> firstChanged;
    std::array<int16_t, (OLED_HEIGHT + 7) / 8> lastChanged;
    firstChanged.fill(-1);
    lastChanged.fill(-1);

    std::size_t changedBytes = 0;
    std::size_t bytesToSend = 0;
    for (uint8_t page = 0; page < pages; ++page) {
        const std::size_t pageOffset =
            static_cast<std::size_t>(page) * static_cast<std::size_t>(width);
        for (int16_t col = 0; col < width; ++col) {
            const std::size_t idx = pageOffset + static_cast<std::size_t>(col);
            if (idx >= _frameBufferBytes) {
                break;
            }
            if (buffer[idx] == _lastPushedFrame[idx]) {
                continue;
            }
            ++changedBytes;
            if (firstChanged[page] < 0) {
                firstChanged[page] = col;
            }
            lastChanged[page] = col;
        }
        if (firstChanged[page] >= 0) {
            bytesToSend += static_cast<std::size_t>(lastChanged[page] - firstChanged[page] + 1);
        }
    }

    if (changedBytes == 0) {
        if (static_cast<unsigned long>(current - _lastFullRefreshMs) >= kPeriodicFullRefreshMs) {
            _display.display();
            syncShadowBuffer();
            _lastDisplayPushMs = current;
            _lastFullRefreshMs = current;
        }
        return;
    }

    if (!kEnablePartialRegionUpdates) {
        _display.display();
        syncShadowBuffer();
        _lastDisplayPushMs = current;
        _lastFullRefreshMs = current;
        return;
    }

    if ((bytesToSend * 100U) >= (_frameBufferBytes * kPartialFallbackPercent)) {
        _display.display();
        syncShadowBuffer();
        _lastDisplayPushMs = current;
        _lastFullRefreshMs = current;
        return;
    }

    Wire.setClock(kDisplayI2CFastHz);
    bool busOk = true;
    for (uint8_t page = 0; page < pages && busOk; ++page) {
        if (firstChanged[page] < 0) {
            continue;
        }
        const uint8_t startCol = static_cast<uint8_t>(firstChanged[page]);
        const uint8_t endCol = static_cast<uint8_t>(lastChanged[page]);

        _display.ssd1306_command(SSD1306_PAGEADDR);
        _display.ssd1306_command(page);
        _display.ssd1306_command(page);
        _display.ssd1306_command(SSD1306_COLUMNADDR);
        _display.ssd1306_command(startCol);
        _display.ssd1306_command(endCol);

        uint8_t col = startCol;
        while (col <= endCol) {
            const uint8_t remaining = static_cast<uint8_t>(endCol - col + 1);
            const uint8_t chunk = std::min<uint8_t>(kI2CDataChunkBytes, remaining);
            Wire.beginTransmission(_i2cAddress);
            Wire.write(kI2CDataPrefix);
            for (uint8_t i = 0; i < chunk; ++i) {
                const std::size_t idx =
                    static_cast<std::size_t>(page) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(col + i);
                Wire.write(buffer[idx]);
            }
            if (Wire.endTransmission() != 0) {
                busOk = false;
                break;
            }
            col = static_cast<uint8_t>(col + chunk);
        }
    }
    Wire.setClock(kDisplayI2CRestoreHz);

    if (!busOk) {
        _display.display();
        _lastFullRefreshMs = current;
    }

    syncShadowBuffer();
    _lastDisplayPushMs = current;
}

// Start the temporary fade animation used for status transitions.
void DisplayManager::triggerFade(uint16_t ms) {
    _fadeAnim.state = AnimState::FADE_IN;
    _fadeAnim.duration = ms;
    _fadeAnim.lastTime = now();
    _fadeAnim.brightness = 0;
}

// Advance the OLED fade animation and translate it into contrast commands.
void DisplayManager::updateFadeAnimation() {
    if (!_initialized) {
        return;
    }
    if (_fadeAnim.state == AnimState::IDLE || _fadeAnim.state == AnimState::DONE) {
        return;
    }

    uint32_t now = ::now();
    uint32_t elapsed = now - _fadeAnim.lastTime;

    switch (_fadeAnim.state) {
    case AnimState::FADE_IN: {
        if (elapsed >= _fadeAnim.duration) {
            _fadeAnim.state = AnimState::HOLD;
            _fadeAnim.lastTime = now;
            _fadeAnim.brightness = 255;
        } else {
            float progress = static_cast<float>(elapsed) / _fadeAnim.duration;
            _fadeAnim.brightness = static_cast<uint8_t>(progress * 255);
        }
        break;
    }
    case AnimState::HOLD:
        if (elapsed >= 500) {
            _fadeAnim.state = AnimState::FADE_OUT;
            _fadeAnim.lastTime = now;
        }
        break;
    case AnimState::FADE_OUT: {
        if (elapsed >= _fadeAnim.duration) {
            _fadeAnim.state = AnimState::DONE;
            _fadeAnim.brightness = 0;
        } else {
            float progress = 1.0f - static_cast<float>(elapsed) / _fadeAnim.duration;
            _fadeAnim.brightness = static_cast<uint8_t>(progress * 255);
        }
        break;
    }
    default:
        break;
    }

    _display.ssd1306_command(SSD1306_SETCONTRAST);
    _display.ssd1306_command(_fadeAnim.brightness);
}

// Run the blocking startup animation sequence before normal status rendering takes over.
void DisplayManager::runStartupAnimation() {
    uint32_t current = now();

    switch (_startupAnim.phase) {
    case StartupPhase::IDLE:
        // Kick off the line-drawing frenzy
        _display.clearDisplay();
        _startupAnim.phase = StartupPhase::DRAW_LINES;
        _startupAnim.step = 0;
        _startupAnim.lastTime = current; // draw immediately
        break;

    case StartupPhase::DRAW_LINES:
        if (current - _startupAnim.lastTime >= 250 || _startupAnim.step == 0) {
            _display.clearDisplay();
            for (int i = 0; i < _display.width(); i += (1 << _startupAnim.step)) {
                _display.drawLine(0, 0, i, _display.height() - 1, SSD1306_COLOR_WHITE);
                _display.drawLine(_display.width() - 1, _display.height() - 1,
                                  _display.width() - 1 - i, 0, SSD1306_COLOR_WHITE);
            }
            present(true);
            _startupAnim.lastTime = current;
            if (++_startupAnim.step >= 5) {
                _startupAnim.phase = StartupPhase::HOLD_LINES;
            }
        }
        break;

    case StartupPhase::HOLD_LINES:
        if (current - _startupAnim.lastTime >= 500) {
            _display.clearDisplay();
            _display.setTextSize(2);
            _display.setTextColor(SSD1306_COLOR_WHITE);
            _display.setCursor((_display.width() - 12 * 6) / 2, _display.height() / 2 - 8);
            _display.println("MOARkNOBS-42");
            present(true);
            _startupAnim.phase = StartupPhase::HOLD_LOGO;
            _startupAnim.lastTime = current;
        }
        break;

    case StartupPhase::HOLD_LOGO:
        if (current - _startupAnim.lastTime >= 1500) {
            _display.clearDisplay();
            present(true);
            _startupAnim.phase = StartupPhase::DONE;
        }
        break;

    case StartupPhase::DONE:
        // No-op. We've already made our grand entrance.
        break;
    }
}

// Draw the simple idle screensaver used when no active UI should be shown.
void DisplayManager::runIdleScreensaver() {
    _display.clearDisplay();
    for (int i = 0; i < 20; i++) {
        int x = random(0, _display.width());
        int y = random(0, _display.height());
        _display.drawPixel(x, y, SSD1306_COLOR_WHITE);
    }
    present(false);
}

void DisplayManager::registerInteraction() { _lastInteractionTime = now(); }

bool DisplayManager::shouldRunScreensaver() const { return (now() - _lastInteractionTime > 90000); }

void DisplayManager::drawBorder() {
    _display.drawRect(0, 0, _display.width(), _display.height(), SSD1306_COLOR_WHITE);
}

void DisplayManager::showText(const char *line1, const char *line2, const char *line3) {
    if (now() < _statusTimeout)
        return;

    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_COLOR_WHITE);

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
    present(true);
}

void DisplayManager::showValue(uint8_t value, bool clearDisplay) {
    if (now() < _statusTimeout)
        return;

    if (clearDisplay) {
        _display.clearDisplay();
    }

    _display.setTextSize(1);
    _display.setTextColor(SSD1306_COLOR_WHITE);
    _display.setCursor(0, 0);
    _display.print("Value: ");
    _display.println(value);

    drawBorder();
    present(true);
}

void DisplayManager::showEnvelopeAssignment(int potIndex, int efIndex, const char *mode,
                                            const char *argMethod) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_COLOR_WHITE);

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
    present(true);
    _statusTimeout = now() + NORMAL_DISPLAY_TIME;
}

void DisplayManager::showMode(const char *mode, bool clearDisplay) {
    if (now() < _statusTimeout)
        return;

    if (clearDisplay) {
        _display.clearDisplay();
    }

    _display.setTextSize(1);
    _display.setTextColor(SSD1306_COLOR_WHITE);
    _display.setCursor(0, 0);
    _display.print("Mode: ");
    _display.println(mode);
    drawBorder();
    present(true);
}

void DisplayManager::clear() {
    if (now() < _statusTimeout)
        return;

    _display.clearDisplay();
    present(true);
}

void DisplayManager::showFilterTuning(const char *labelFreq, float freqValue, const char *labelQ,
                                      float qValue) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_COLOR_WHITE);
    _display.setCursor(0, 0);
    _display.print(labelFreq);
    _display.print(": ");
    _display.println(freqValue, 2);

    _display.setCursor(0, 10);
    _display.print(labelQ);
    _display.print(": ");
    _display.println(qValue, 2);

    drawBorder();
    present(true);
}

void DisplayManager::showArpSettings(uint8_t lengthTicks, const char *shapeName) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_COLOR_WHITE);
    _display.setCursor(0, 0);
    // Show the raw tick span so the groove math is crystal clear.
    _display.print("Len (ticks): ");
    _display.println(lengthTicks);
    _display.setCursor(0, 10);
    _display.print("Shape: ");
    _display.println(shapeName);
    drawBorder();
    present(true);
}

void DisplayManager::updateDisplay(uint8_t beatPosition, const uint8_t *envelopeLevels,
                                   size_t envelopeCount, const char *statusMessage,
                                   uint8_t activePot, uint8_t activeChannel,
                                   const char *envelopeMode) {
    if (now() < _statusTimeout)
        return;

    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_COLOR_WHITE);
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
    char modeLabel[24];
    _display.println(friendlyEnvelopeMode(envelopeMode, modeLabel, sizeof(modeLabel)));

    // Visualize envelope levels as vertical bars along the bottom
    size_t numEnvelopes = 0;
    if (envelopeLevels && envelopeCount > 0) {
        numEnvelopes = envelopeCount;
        if (numEnvelopes > NUM_ENVELOPES) {
            numEnvelopes = NUM_ENVELOPES;
        }
    }
    const int barWidth = 6;
    const int maxHeight = 20;
    const int baseY = _display.height() - 1;

    for (size_t i = 0; i < numEnvelopes; i++) {
        int barHeight = map(envelopeLevels[i], 0, 127, 0, maxHeight);
        int x = static_cast<int>(i) * (barWidth + 2);
        _display.fillRect(x, baseY - barHeight, barWidth, barHeight, SSD1306_COLOR_WHITE);
    }

    drawBorder();
    present(false);

    if (statusMessage && statusMessage[0] != '\0') {
        _statusTimeout = now() + NORMAL_DISPLAY_TIME;
    }
}

void DisplayManager::displayStatus(const char *status, unsigned long duration) {
    _statusMessage = status;
    _statusTimeout = now() + duration;

    _display.clearDisplay();
    _display.setTextSize(2);
    _display.setTextColor(SSD1306_COLOR_WHITE);
    _display.setCursor(0, 0);
    _display.println(status);
    present(true);
}

void DisplayManager::updateFromContext(const ButtonManagerContext &context) {
    if (now() < _statusTimeout)
        return;

    _display.clearDisplay();
    _display.setCursor(0, 0);
    _display.print("BTN: ");
    _display.print(context.activePot);
    _display.print(" CH: ");
    _display.print(context.activeChannel);
    _display.print(" B:");
    _display.println(midiBeatPosition);
    _display.setCursor(0, 10);
    _display.print("EF: ");
    _display.println(context.envelopeFollowMode ? "ON" : "OFF");

    auto efIt = context.potToEnvelopeMap.find(context.activePot);
    if (efIt == context.potToEnvelopeMap.end()) {
        _display.setCursor(0, 20);
        _display.println("ENV: NONE");
        _display.setCursor(0, 30);
        _display.println("F: --   Q: --");
        _display.setCursor(0, 40);
        _display.println("B: --   G: --");
        present(false);
        return;
    }

    const MIDISlot::EfSettings &settings = efIt->second;
    const int followerIndex = settings.followerIndex;

    _display.setCursor(0, 20);
    _display.print("ENV: ");
    if (followerIndex >= 0) {
        _display.print(followerIndex);
        _display.print(" ");
        _display.println(efFilterLabel(settings.filterType));
    } else {
        _display.println("NONE");
    }

    _display.setCursor(0, 30);
    _display.print("F: ");
    _display.print(static_cast<int>(settings.frequency));
    _display.print("Hz  Q:");
    _display.print(settings.q, 2);

    _display.setCursor(0, 40);
    _display.print("B:");
    _display.print(settings.baseline, 2);
    _display.print("  G:");
    _display.print(settings.gain, 2);

    present(false);
}

void DisplayManager::showARGInfo(const char *methodName, int envA, int envB) {
    if (now() < _statusTimeout)
        return;

    _display.clearDisplay();

    _display.setTextSize(1);
    _display.setTextColor(SSD1306_COLOR_WHITE);

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

    present(true);
    _statusTimeout = now() + NORMAL_DISPLAY_TIME;
}

void DisplayManager::setTemporaryMessage(const char *message, unsigned long duration) {
    _statusMessage = message;
    _statusTimeout = now() + duration;
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_COLOR_WHITE);
    _display.setCursor(0, 0);
    _display.println(message);
    present(true);
}

void DisplayManager::showMIDIMessage(uint8_t cc, uint8_t value, uint8_t channel) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_COLOR_WHITE);
    _display.setCursor(0, 0);
    _display.print("CC: ");
    _display.print(cc);
    _display.print(" Value: ");
    _display.println(value);
    _display.setCursor(0, 10);
    _display.print("Ch: ");
    _display.println(channel);
    present(true);
    _statusTimeout = now() + SHORT_DISPLAY_TIME;
}

void DisplayManager::showDiagnostic(uint8_t page, const ButtonManager &bm,
                                    const ButtonManagerContext &ctx, const MIDIHandler &midi,
                                    const SystemDiagnostics &diag) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_COLOR_WHITE);

    switch (page % kDiagnosticPageCount) {
    case 0: { // Button matrix snapshot
        _display.setCursor(0, 0);
        for (uint8_t r = 0; r < BUTTON_ROWS; ++r) {
            for (uint8_t c = 0; c < BUTTON_COLS; ++c) {
                uint8_t idx = r * BUTTON_COLS + c;
                _display.print(bm.isMuxButtonPressed(idx) ? '1' : '0');
            }
            _display.println();
        }
        break;
    }
    case 1: { // Envelope baselines
        for (uint8_t i = 0; i < ctx.envelopes.size() && i < 6; ++i) {
            _display.setCursor(0, i * 10);
            _display.print("EF");
            _display.print(i);
            _display.print(':');
            _display.println(ctx.envelopes[i].getBaseline(), 2);
        }
        break;
    }
    case 2: { // MIDI counters
        _display.setCursor(0, 0);
        _display.print("RX:");
        _display.println(midi.getRxCount());
        _display.setCursor(0, 10);
        _display.print("TX:");
        _display.println(midi.getTxCount());
        _display.setCursor(0, 20);
        _display.print("Drop:");
        _display.println(diag.midiDropCount);
        _display.setCursor(0, 30);
        _display.print("UART:");
        _display.println(diag.uartOverrunCount);
        break;
    }
    case 3: { // Loop and ISR timing
        _display.setCursor(0, 0);
        _display.print("Loop mx:");
        _display.println(diag.maxLoopMicros);
        _display.setCursor(0, 10);
        _display.print("ISR mx:");
        _display.println(diag.maxProcessMidiMicros);
        _display.setCursor(0, 20);
        _display.print(">1ms:");
        _display.print(diag.loopOverrunCount);
        _display.print('/');
        _display.println(diag.midiTaskOverrunCount);
        _display.setCursor(0, 30);
        _display.print("Last:");
        _display.println(diag.lastLoopMicros);
        break;
    }
    case kDiagnosticPageDebug: { // Deep dive: EF health, LFO taps, and tempo lock.
        // Line 0: tempo and clock lock status.
        _display.setCursor(0, 0);
        _display.print("DBG BPM:");
        if (g_tappedBPM > 0.0f) {
            _display.print(g_tappedBPM, 1);
        } else {
            _display.print("--");
        }
        _display.print(" CLK:");
        _display.print(midi.isClockRunning() ? "ON" : "OFF");

        // Lines 1-6: one envelope follower per row (baseline, gain, value).
        for (uint8_t i = 0; i < NUM_ENVELOPES; ++i) {
            uint8_t y = static_cast<uint8_t>((i + 1) * 8);
            _display.setCursor(0, y);
            _display.print('E');
            _display.print(i);
            _display.print(' ');
            if (i < ctx.envelopes.size()) {
                EnvelopeFollower::EfStats stats = ctx.envelopes[i].getStats();
                _display.print('B');
                _display.print(stats.baseline, 2);
                _display.print(" G");
                _display.print(stats.gain, 2);
                _display.print(" V");
                _display.print(stats.value);
            } else {
                _display.print("B-- G-- V--");
            }
        }

        // Line 7: LFO outputs mirrored in normalized 0..1 space.
        _display.setCursor(0, 56);
        _display.print("L1:");
        _display.print(g_lfoValues[0], 2);
        _display.print(" L2:");
        _display.print(g_lfoValues[1], 2);
        break;
    }
    }
    drawBorder();
    present(false);
}

void DisplayManager::updateBeat(uint8_t beatPosition, bool clockRunning) {
    if (now() < _statusTimeout)
        return;

    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_COLOR_WHITE);
    _display.setCursor(0, 0);

    if (clockRunning) {
        _display.print("Beat: ");
        _display.println(beatPosition);
    } else {
        _display.println("No Clock");
    }

    present(true);
}

void DisplayManager::beginDraw() {
    _display.clearDisplay();
    _isDrawing = true;
}

void DisplayManager::endDraw() {
    _isDrawing = false;
    present(false);
}

void DisplayManager::showError(const char *errorMessage, bool persistent) {
    if (now() < _statusTimeout)
        return;

    beginDraw();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_COLOR_WHITE);
    _display.setCursor(0, 0);
    _display.println(F("ERROR:"));
    _display.println(errorMessage);
    endDraw();
    if (persistent) {
        while (1)
            ;
    }
}

void DisplayManager::showEnvelopeLevel(uint8_t level) {
    if (now() < _statusTimeout)
        return;

    const int barHeight = 10;
    const int barY = _display.height() - barHeight;
    int barWidth = map(level, 0, 127, 0, _display.width());
    _display.fillRect(0, barY, _display.width(), barHeight, SSD1306_COLOR_BLACK);
    _display.fillRect(0, barY, barWidth, barHeight, SSD1306_COLOR_WHITE);
}

void DisplayManager::showEnvelopeLevels(uint8_t envA, uint8_t envB) {
    if (now() < _statusTimeout)
        return;

    const int barHeight = 5;
    const int gap = 2;
    int widthA = map(envA, 0, 127, 0, _display.width());
    _display.fillRect(0, _display.height() - barHeight * 2 - gap, _display.width(), barHeight,
                      SSD1306_COLOR_BLACK);
    _display.fillRect(0, _display.height() - barHeight * 2 - gap, widthA, barHeight,
                      SSD1306_COLOR_WHITE);

    int widthB = map(envB, 0, 127, 0, _display.width());
    _display.fillRect(0, _display.height() - barHeight, _display.width(), barHeight,
                      SSD1306_COLOR_BLACK);
    _display.fillRect(0, _display.height() - barHeight, widthB, barHeight, SSD1306_COLOR_WHITE);
}

void DisplayManager::updateActiveSelection(uint8_t activePot, uint8_t activeChannel) {
    _activePot = activePot;
    _activeChannel = activeChannel;
}

void DisplayManager::highlightActivePot(uint8_t potIndex) {
    _display.drawRect(5 + potIndex * 10, 50, 8, 8, SSD1306_COLOR_WHITE);
}

void DisplayManager::highlightActiveMode(const String &modeName) {
    _activeMode = modeName;
    _display.setCursor(0, 56);
    _display.print(F("MODE: "));
    _display.print(_activeMode);
}

void DisplayManager::setUpdateInterval(unsigned long intervalMs) { _updateIntervalMs = intervalMs; }

unsigned long DisplayManager::getUpdateInterval() const { return _updateIntervalMs; }
