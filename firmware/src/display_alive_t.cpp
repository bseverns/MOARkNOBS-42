#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

namespace {

constexpr uint16_t kDisplayWidth = 128;
constexpr uint16_t kDisplayHeight = 64;
constexpr uint8_t kPrimaryDisplayAddress = 0x3C;
constexpr uint8_t kAlternateDisplayAddress = 0x3D;
constexpr unsigned long kSerialBaud = 115200UL;
constexpr unsigned long kSerialWaitMs = 1500UL;
constexpr unsigned long kFrameIntervalMs = 80UL;
constexpr unsigned long kFailBlinkMs = 80UL;
constexpr unsigned long kPhaseDurationMs = 1600UL;
constexpr uint8_t kStatusLedPin = 23;
constexpr uint8_t kMaxContrast = 0xFF;

enum class DisplayPhase : uint8_t { AllOn, InvertAllOn, Framebuffer, Motion };

Adafruit_SSD1306 gDisplay(kDisplayWidth, kDisplayHeight, &Wire);
unsigned long gBootMs = 0;
unsigned long gLastFrameMs = 0;
uint8_t gActiveAddress = kPrimaryDisplayAddress;
DisplayPhase gLastLoggedPhase = DisplayPhase::Motion;

unsigned long nowMs() { return millis(); }

bool probeAddress(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

void logDetectedAddresses() {
    bool foundAny = false;
    Serial.print("[DISPLAY ALIVE] I2C scan:");
    for (uint8_t address = 0x03; address < 0x78; ++address) {
        if (!probeAddress(address)) {
            continue;
        }
        foundAny = true;
        Serial.print(" 0x");
        if (address < 0x10) {
            Serial.print('0');
        }
        Serial.print(address, HEX);
    }
    if (!foundAny) {
        Serial.print(" none");
    }
    Serial.println();
}

bool beginDisplayAt(uint8_t address) {
    if (!gDisplay.begin(SSD1306_SWITCHCAPVCC, address)) {
        return false;
    }
    gActiveAddress = address;
    return true;
}

void forcePanelOn(bool invert) {
    gDisplay.ssd1306_command(SSD1306_DISPLAYON);
    gDisplay.ssd1306_command(SSD1306_SETCONTRAST);
    gDisplay.ssd1306_command(kMaxContrast);
    gDisplay.ssd1306_command(invert ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY);
    gDisplay.ssd1306_command(SSD1306_DISPLAYALLON);
}

void restoreFramebufferMode(bool invert) {
    gDisplay.ssd1306_command(SSD1306_DISPLAYON);
    gDisplay.ssd1306_command(SSD1306_SETCONTRAST);
    gDisplay.ssd1306_command(kMaxContrast);
    gDisplay.ssd1306_command(invert ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY);
    gDisplay.ssd1306_command(SSD1306_DISPLAYALLON_RESUME);
}

[[noreturn]] void failLoop() {
    while (true) {
        digitalWrite(kStatusLedPin, HIGH);
        delay(kFailBlinkMs);
        digitalWrite(kStatusLedPin, LOW);
        delay(kFailBlinkMs);
    }
}

DisplayPhase currentPhase(unsigned long elapsedMs) {
    const unsigned long phaseIndex = (elapsedMs / kPhaseDurationMs) % 4UL;
    switch (phaseIndex) {
    case 0:
        return DisplayPhase::AllOn;
    case 1:
        return DisplayPhase::InvertAllOn;
    case 2:
        return DisplayPhase::Framebuffer;
    default:
        return DisplayPhase::Motion;
    }
}

const char *phaseLabel(DisplayPhase phase) {
    switch (phase) {
    case DisplayPhase::AllOn:
        return "all-on";
    case DisplayPhase::InvertAllOn:
        return "invert-all-on";
    case DisplayPhase::Framebuffer:
        return "framebuffer";
    case DisplayPhase::Motion:
        return "motion";
    }
    return "unknown";
}

void drawFramebufferFrame(unsigned long elapsedMs, bool invert, bool animateBar) {
    gDisplay.clearDisplay();
    gDisplay.drawRect(0, 0, kDisplayWidth, kDisplayHeight, SSD1306_COLOR_WHITE);
    gDisplay.drawLine(0, 12, kDisplayWidth - 1, 12, SSD1306_COLOR_WHITE);

    gDisplay.setTextColor(SSD1306_COLOR_WHITE);
    gDisplay.setTextSize(1);
    gDisplay.setCursor(4, 2);
    gDisplay.print("MN42 DISPLAY CHECK");

    gDisplay.setTextSize(2);
    gDisplay.setCursor(10, 18);
    gDisplay.print("ALIVE");

    gDisplay.drawRect(4, 44, kDisplayWidth - 8, 12, SSD1306_COLOR_WHITE);
    if (animateBar) {
        const int16_t barWidth = 22;
        const int16_t travel = kDisplayWidth - 10 - barWidth;
        const int16_t barX =
            5 + static_cast<int16_t>((elapsedMs / 18UL) % static_cast<unsigned long>(travel));
        gDisplay.fillRect(barX, 47, barWidth, 6, SSD1306_COLOR_WHITE);
    } else {
        gDisplay.fillRect(8, 47, kDisplayWidth - 16, 6, SSD1306_COLOR_WHITE);
    }

    gDisplay.setTextSize(1);
    gDisplay.setCursor(8, 58);
    gDisplay.print(invert ? "invert:on " : "invert:off");
    gDisplay.setCursor(72, 58);
    gDisplay.print("0x");
    if (gActiveAddress < 0x10) {
        gDisplay.print('0');
    }
    gDisplay.print(gActiveAddress, HEX);

    gDisplay.display();
}

void drawFrame(unsigned long elapsedMs) {
    const DisplayPhase phase = currentPhase(elapsedMs);
    if (phase != gLastLoggedPhase) {
        Serial.print("[DISPLAY ALIVE] phase -> ");
        Serial.println(phaseLabel(phase));
        gLastLoggedPhase = phase;
    }
    switch (phase) {
    case DisplayPhase::AllOn:
        forcePanelOn(false);
        break;
    case DisplayPhase::InvertAllOn:
        forcePanelOn(true);
        break;
    case DisplayPhase::Framebuffer:
        restoreFramebufferMode(false);
        drawFramebufferFrame(elapsedMs, false, false);
        break;
    case DisplayPhase::Motion:
        restoreFramebufferMode(true);
        drawFramebufferFrame(elapsedMs, true, true);
        break;
    }
}

} // namespace

void setup() {
    pinMode(kStatusLedPin, OUTPUT);
    digitalWrite(kStatusLedPin, LOW);

    Serial.begin(kSerialBaud);
    const unsigned long serialStartMs = nowMs();
    while (!Serial && (nowMs() - serialStartMs) < kSerialWaitMs) {
        delay(10);
    }

    Wire.begin();
    logDetectedAddresses();

    const bool primaryFound = probeAddress(kPrimaryDisplayAddress);
    const bool alternateFound = probeAddress(kAlternateDisplayAddress);

    bool initialized = false;
    if (primaryFound) {
        initialized = beginDisplayAt(kPrimaryDisplayAddress);
    }
    if (!initialized && alternateFound) {
        initialized = beginDisplayAt(kAlternateDisplayAddress);
    }
    if (!initialized && !primaryFound && !alternateFound) {
        initialized = beginDisplayAt(kPrimaryDisplayAddress);
    }

    if (!initialized) {
        Serial.println("[DISPLAY ALIVE] init failed at 0x3C and 0x3D");
        failLoop();
    }

    gBootMs = nowMs();
    gLastFrameMs = gBootMs;
    digitalWrite(kStatusLedPin, HIGH);

    Serial.print("[DISPLAY ALIVE] display initialized at 0x");
    if (gActiveAddress < 0x10) {
        Serial.print('0');
    }
    Serial.println(gActiveAddress, HEX);
    Serial.println("[DISPLAY ALIVE] phases: all-on -> invert-all-on -> framebuffer -> motion");
    gLastLoggedPhase = DisplayPhase::Motion;
    drawFrame(0);
}

void loop() {
    const unsigned long currentMs = nowMs();
    if ((currentMs - gLastFrameMs) < kFrameIntervalMs) {
        return;
    }
    gLastFrameMs = currentMs;
    drawFrame(currentMs - gBootMs);
}
