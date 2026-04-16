#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FastLED.h>
#include <cmath>
#include <memory>

#include "Globals.h"
#include "LEDManager.h"

// Standalone hardware exerciser for OLED + LED strip.
// Runs visual loops without requiring the full runtime/scheduler stack.

namespace {

constexpr unsigned long kSerialWaitMs = 2000UL;
constexpr unsigned long kFrameIntervalMs = 33UL; // ~30 FPS
constexpr unsigned long kPhaseDurationMs = 10000UL;
constexpr unsigned long kStatusBlinkMs = 250UL;
constexpr uint8_t kLedBrightness = 96;
constexpr uint8_t kContrastDefault = 0xCF;
constexpr uint8_t kContrastLow = 0x25;
constexpr uint8_t kContrastHigh = 0xFF;
constexpr uint8_t kPhaseButtonPin = 12; // Control button #0 on current prototype wiring

enum class DemoPhase : uint8_t { Intro = 0, WaveText, ContrastSwap, LedChase, LedRainbow, Combo };

constexpr uint8_t kDemoPhaseCount = 6;

Adafruit_SSD1306 gDisplay(OLED_WIDTH, OLED_HEIGHT, &Wire);
std::unique_ptr<LEDManager> gLedManager;

DemoPhase gPhase = DemoPhase::Intro;
unsigned long gPhaseStartedMs = 0;
unsigned long gLastFrameMs = 0;
unsigned long gLastStatusBlinkMs = 0;
unsigned long gLastPhaseButtonEdgeMs = 0;
bool gStatusLedState = false;
bool gLastPhaseButtonPressed = false;
uint8_t gCurrentContrast = kContrastDefault;

const char *phaseName(DemoPhase phase) {
    switch (phase) {
    case DemoPhase::Intro:
        return "Intro";
    case DemoPhase::WaveText:
        return "Wave";
    case DemoPhase::ContrastSwap:
        return "Contrast";
    case DemoPhase::LedChase:
        return "Chase";
    case DemoPhase::LedRainbow:
        return "Rainbow";
    case DemoPhase::Combo:
        return "Combo";
    }
    return "Unknown";
}

unsigned long nowMs() { return millis(); }

void setContrast(uint8_t contrast) {
    if (contrast == gCurrentContrast) {
        return;
    }
    gDisplay.ssd1306_command(SSD1306_SETCONTRAST);
    gDisplay.ssd1306_command(contrast);
    gCurrentContrast = contrast;
}

void clearStrip(const CRGB &color = CRGB::Black) {
    for (uint16_t i = 0; i < NUM_LEDS(); ++i) {
        gLedManager->setPixelColor(i, color);
    }
}

void fillGradient(uint8_t hueBase, uint8_t hueStep) {
    for (uint16_t i = 0; i < NUM_LEDS(); ++i) {
        const uint8_t hue = static_cast<uint8_t>(hueBase + static_cast<uint8_t>(i * hueStep));
        gLedManager->setPixelColor(i, CHSV(hue, 255, 255));
    }
}

void advancePhase() {
    const uint8_t next =
        static_cast<uint8_t>((static_cast<uint8_t>(gPhase) + 1U) % kDemoPhaseCount);
    gPhase = static_cast<DemoPhase>(next);
    gPhaseStartedMs = nowMs();
    setContrast(kContrastDefault);
    gDisplay.invertDisplay(false);
    gDisplay.clearDisplay();
    clearStrip();
    gLedManager->update();
    Serial.printf("[HW TEST] phase -> %s\n", phaseName(gPhase));
}

void drawTopBanner(const char *label) {
    gDisplay.setTextSize(1);
    gDisplay.setTextColor(SSD1306_COLOR_WHITE);
    gDisplay.setCursor(0, 0);
    gDisplay.print("HW ");
    gDisplay.print(label);
    gDisplay.drawLine(0, 9, OLED_WIDTH - 1, 9, SSD1306_COLOR_WHITE);
}

void renderDisplay(unsigned long elapsedMs) {
    gDisplay.clearDisplay();
    drawTopBanner(phaseName(gPhase));

    switch (gPhase) {
    case DemoPhase::Intro: {
        gDisplay.setCursor(0, 16);
        gDisplay.println("Display + LED");
        gDisplay.println("hardware test");
        gDisplay.println("Btn0: next phase");
        const uint8_t dots = static_cast<uint8_t>((elapsedMs / 250UL) % 4UL);
        gDisplay.setCursor(0, 52);
        gDisplay.print("running");
        for (uint8_t i = 0; i < dots; ++i) {
            gDisplay.print('.');
        }
        break;
    }
    case DemoPhase::WaveText: {
        gDisplay.setCursor(0, 14);
        gDisplay.println("Small-screen");
        gDisplay.println("motion check");

        const float t = static_cast<float>(elapsedMs) * 0.012f;
        for (int16_t x = 0; x < OLED_WIDTH; ++x) {
            const float yF = 42.0f + 9.0f * sinf((static_cast<float>(x) * 0.18f) + t);
            const int16_t y = static_cast<int16_t>(yF);
            if (y >= 10 && y < OLED_HEIGHT) {
                gDisplay.drawPixel(x, y, SSD1306_COLOR_WHITE);
            }
        }
        break;
    }
    case DemoPhase::ContrastSwap: {
        const bool high = ((elapsedMs / 600UL) % 2UL) == 0UL;
        setContrast(high ? kContrastHigh : kContrastLow);

        gDisplay.setCursor(0, 16);
        gDisplay.print("Contrast: ");
        gDisplay.println(high ? "HIGH" : "LOW");
        gDisplay.setCursor(0, 28);
        gDisplay.print("Value: 0x");
        gDisplay.println(gCurrentContrast, HEX);
        gDisplay.setCursor(0, 40);
        gDisplay.println("Checks OLED drive");
        gDisplay.setCursor(0, 52);
        gDisplay.println("under fast swaps");

        const int16_t barW = static_cast<int16_t>(map(gCurrentContrast, 0, 255, 4, OLED_WIDTH - 4));
        gDisplay.drawRect(0, 56, OLED_WIDTH, 8, SSD1306_COLOR_WHITE);
        gDisplay.fillRect(2, 58, barW - 2, 4, SSD1306_COLOR_WHITE);
        break;
    }
    case DemoPhase::LedChase: {
        gDisplay.setCursor(0, 16);
        gDisplay.println("LED chase");
        gDisplay.println("segment walk");

        const uint16_t idx =
            static_cast<uint16_t>((elapsedMs / 35UL) % static_cast<unsigned long>(NUM_LEDS()));
        gDisplay.setCursor(0, 40);
        gDisplay.print("active: ");
        gDisplay.println(idx);
        gDisplay.drawRect(0, 54, OLED_WIDTH, 10, SSD1306_COLOR_WHITE);
        const int16_t markerX =
            static_cast<int16_t>(map(static_cast<long>(idx), 0L, static_cast<long>(NUM_LEDS() - 1),
                                     2L, static_cast<long>(OLED_WIDTH - 4)));
        gDisplay.fillRect(markerX, 56, 3, 6, SSD1306_COLOR_WHITE);
        break;
    }
    case DemoPhase::LedRainbow: {
        gDisplay.setCursor(0, 16);
        gDisplay.println("Rainbow sweep");
        gDisplay.println("hue progression");
        gDisplay.setCursor(0, 40);
        gDisplay.print("hue base: ");
        gDisplay.println(static_cast<uint16_t>((elapsedMs / 12UL) & 0xFFUL));
        break;
    }
    case DemoPhase::Combo: {
        const bool invert = ((elapsedMs / 1000UL) % 2UL) != 0UL;
        gDisplay.invertDisplay(invert);
        gDisplay.setCursor(0, 16);
        gDisplay.println("Combo stress");
        gDisplay.println("OLED + LED sync");
        gDisplay.setCursor(0, 40);
        gDisplay.print("invert: ");
        gDisplay.println(invert ? "ON" : "OFF");
        break;
    }
    }

    gDisplay.display();
}

void renderLeds(unsigned long elapsedMs) {
    switch (gPhase) {
    case DemoPhase::Intro: {
        const uint8_t beat = beatsin8(20, 8, 180);
        clearStrip(CRGB::Black);
        for (uint16_t i = 0; i < NUM_LEDS(); ++i) {
            if ((i % 6U) == 0U) {
                gLedManager->setPixelColor(i, CHSV(160, 220, beat));
            }
        }
        break;
    }
    case DemoPhase::WaveText: {
        clearStrip(CRGB::Black);
        for (uint16_t i = 0; i < NUM_LEDS(); ++i) {
            const uint8_t v = static_cast<uint8_t>(
                96 + (sin8(static_cast<uint8_t>(i * 13U + elapsedMs / 5UL)) >> 1));
            gLedManager->setPixelColor(i, CHSV(30, 255, v));
        }
        break;
    }
    case DemoPhase::ContrastSwap: {
        const bool high = ((elapsedMs / 600UL) % 2UL) == 0UL;
        clearStrip(high ? CRGB(128, 128, 128) : CRGB(10, 10, 10));
        break;
    }
    case DemoPhase::LedChase: {
        clearStrip(CRGB(2, 2, 2));
        const uint16_t lead =
            static_cast<uint16_t>((elapsedMs / 35UL) % static_cast<unsigned long>(NUM_LEDS()));
        gLedManager->setPixelColor(lead, CRGB::White);
        if (lead > 0) {
            gLedManager->setPixelColor(static_cast<uint16_t>(lead - 1U), CRGB(100, 0, 255));
        }
        break;
    }
    case DemoPhase::LedRainbow: {
        const uint8_t hueBase = static_cast<uint8_t>((elapsedMs / 12UL) & 0xFFUL);
        fillGradient(hueBase, 5);
        break;
    }
    case DemoPhase::Combo: {
        const uint8_t hueBase = static_cast<uint8_t>((elapsedMs / 8UL) & 0xFFUL);
        fillGradient(hueBase, 3);
        const uint16_t blink =
            static_cast<uint16_t>((elapsedMs / 70UL) % static_cast<unsigned long>(NUM_LEDS()));
        gLedManager->setPixelColor(blink, CRGB::White);
        break;
    }
    }

    gLedManager->update();
}

void servicePhaseButton() {
    const bool pressed = digitalRead(kPhaseButtonPin) == LOW;
    const unsigned long current = nowMs();
    if (pressed && !gLastPhaseButtonPressed && (current - gLastPhaseButtonEdgeMs) > 120UL) {
        gLastPhaseButtonEdgeMs = current;
        advancePhase();
    }
    gLastPhaseButtonPressed = pressed;
}

void serviceStatusLed() {
    const unsigned long current = nowMs();
    if ((current - gLastStatusBlinkMs) < kStatusBlinkMs) {
        return;
    }
    gLastStatusBlinkMs = current;
    gStatusLedState = !gStatusLedState;
    digitalWrite(hwConfig.statusLedPin, gStatusLedState ? HIGH : LOW);
}

} // namespace

// TimeUtils hook used by LEDManager.
unsigned long now() { return millis(); }

void setup() {
    loadHardwareConfig();
    pinMode(hwConfig.statusLedPin, OUTPUT);
    pinMode(kPhaseButtonPin, INPUT_PULLUP);
    digitalWrite(hwConfig.statusLedPin, LOW);

    Serial.begin(SERIAL_BAUD);
    const unsigned long serialStart = nowMs();
    while (!Serial && (nowMs() - serialStart) < kSerialWaitMs) {
        delay(10);
    }

    gLedManager = std::make_unique<LEDManager>(hwConfig);
    gLedManager->begin();
    gLedManager->setState(LEDState::IDLE);
    gLedManager->setBrightness(kLedBrightness);
    clearStrip();
    gLedManager->update();

    const bool ok = gDisplay.begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDRESS);
    if (!ok) {
        Serial.println("[HW TEST] OLED init failed at 0x3C");
        while (true) {
            digitalWrite(hwConfig.statusLedPin, HIGH);
            delay(120);
            digitalWrite(hwConfig.statusLedPin, LOW);
            delay(120);
        }
    }

    setContrast(kContrastDefault);
    gDisplay.clearDisplay();
    gDisplay.setTextSize(1);
    gDisplay.setTextColor(SSD1306_COLOR_WHITE);
    gDisplay.setCursor(0, 0);
    gDisplay.println("MOARkNOBS HW TEST");
    gDisplay.println("OLED + LED");
    gDisplay.println("Btn0 next phase");
    gDisplay.display();

    Serial.println("[HW TEST] display_led_hw_t booted");
    gPhaseStartedMs = nowMs();
    gLastFrameMs = gPhaseStartedMs;
    gLastStatusBlinkMs = gPhaseStartedMs;
}

void loop() {
    servicePhaseButton();
    serviceStatusLed();

    const unsigned long current = nowMs();
    if ((current - gPhaseStartedMs) > kPhaseDurationMs) {
        advancePhase();
    }
    if ((current - gLastFrameMs) < kFrameIntervalMs) {
        return;
    }
    gLastFrameMs = current;

    const unsigned long elapsed = current - gPhaseStartedMs;
    renderDisplay(elapsed);
    renderLeds(elapsed);
}
