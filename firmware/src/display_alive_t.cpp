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
constexpr unsigned long kNoBusBlinkMs = 600UL;
constexpr unsigned long kNoBusScanMs = 5000UL;
constexpr unsigned long kNoBusReportMs = 1000UL;
constexpr unsigned long kPhaseDurationMs = 1600UL;
constexpr uint8_t kStatusLedPin = 23;
constexpr uint8_t kMaxContrast = 0xFF;
constexpr uint8_t kSdaPin = 18;
constexpr uint8_t kSclPin = 19;

enum class DisplayPhase : uint8_t { AllOn, InvertAllOn, Framebuffer, Motion };
enum class LaneMode : uint8_t { NoBusAck, DisplayOk, InitFailed };

Adafruit_SSD1306 gDisplay(kDisplayWidth, kDisplayHeight, &Wire);
unsigned long gBootMs = 0;
unsigned long gLastFrameMs = 0;
unsigned long gLastNoBusBlinkMs = 0;
unsigned long gLastNoBusScanMs = 0;
unsigned long gLastNoBusReportMs = 0;
uint8_t gActiveAddress = kPrimaryDisplayAddress;
DisplayPhase gLastLoggedPhase = DisplayPhase::Motion;
LaneMode gLaneMode = LaneMode::NoBusAck;
bool gStatusLedOn = false;

unsigned long nowMs() { return millis(); }

const char *lineLevelLabel(int level) { return level == HIGH ? "HIGH" : "LOW"; }

bool probeAddress(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

bool logDetectedAddresses() {
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
    return foundAny;
}

void logBusLevels() {
    Serial.print("[DISPLAY ALIVE] bus levels SDA=");
    Serial.print(lineLevelLabel(digitalRead(kSdaPin)));
    Serial.print(" SCL=");
    Serial.println(lineLevelLabel(digitalRead(kSclPin)));
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

void serviceNoBusBlink(unsigned long currentMs) {
    if ((currentMs - gLastNoBusBlinkMs) < kNoBusBlinkMs) {
        return;
    }
    gLastNoBusBlinkMs = currentMs;
    gStatusLedOn = !gStatusLedOn;
    digitalWrite(kStatusLedPin, gStatusLedOn ? HIGH : LOW);
}

void serviceNoBusReporting(unsigned long currentMs) {
    if ((currentMs - gLastNoBusReportMs) >= kNoBusReportMs) {
        gLastNoBusReportMs = currentMs;
        logBusLevels();
    }
    if ((currentMs - gLastNoBusScanMs) >= kNoBusScanMs) {
        gLastNoBusScanMs = currentMs;
        Serial.println("[DISPLAY ALIVE] quiet window ended, rescanning I2C");
        const bool foundAny = logDetectedAddresses();
        if (foundAny) {
            Serial.println("[DISPLAY ALIVE] device ACK appeared on bus");
        }
        Serial.println("[DISPLAY ALIVE] quiet window restarted for meter checks");
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
    pinMode(kSdaPin, INPUT);
    pinMode(kSclPin, INPUT);

    Serial.begin(kSerialBaud);
    const unsigned long serialStartMs = nowMs();
    while (!Serial && (nowMs() - serialStartMs) < kSerialWaitMs) {
        delay(10);
    }

    Wire.begin();
    const bool foundAny = logDetectedAddresses();
    logBusLevels();

    const bool primaryFound = probeAddress(kPrimaryDisplayAddress);
    const bool alternateFound = probeAddress(kAlternateDisplayAddress);

    gBootMs = nowMs();
    gLastFrameMs = gBootMs;
    gLastNoBusBlinkMs = gBootMs;
    gLastNoBusScanMs = gBootMs;
    gLastNoBusReportMs = gBootMs;

    if (!foundAny) {
        gLaneMode = LaneMode::NoBusAck;
        Serial.println("[DISPLAY ALIVE] no device ACK on I2C; leaving bus quiet between rescans");
        Serial.println("[DISPLAY ALIVE] status LED slow-blink means no bus ACK");
        Serial.println("[DISPLAY ALIVE] meter target: SDA and SCL should both idle HIGH");
        return;
    }

    bool initialized = false;
    if (primaryFound) {
        initialized = beginDisplayAt(kPrimaryDisplayAddress);
    }
    if (!initialized && alternateFound) {
        initialized = beginDisplayAt(kAlternateDisplayAddress);
    }

    if (!initialized) {
        gLaneMode = LaneMode::InitFailed;
        Serial.println("[DISPLAY ALIVE] panel ACKed but init failed at 0x3C/0x3D");
        failLoop();
    }

    gLaneMode = LaneMode::DisplayOk;
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
    if (gLaneMode == LaneMode::NoBusAck) {
        serviceNoBusBlink(currentMs);
        serviceNoBusReporting(currentMs);
        return;
    }
    if ((currentMs - gLastFrameMs) < kFrameIntervalMs) {
        return;
    }
    gLastFrameMs = currentMs;
    drawFrame(currentMs - gBootMs);
}
