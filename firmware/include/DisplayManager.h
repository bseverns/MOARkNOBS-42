#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vector>

// Forward declaration of ButtonManagerContext to avoid circular dependency
struct ButtonManagerContext;

#define SHORT_DISPLAY_TIME 1000  // 1s for quick updates
#define NORMAL_DISPLAY_TIME 1500 // 1.5s for status changes
#define LONG_DISPLAY_TIME 3000   // 3s for confirmations

struct DisplayState {
    uint8_t lastCC;
    uint8_t lastCCValue;
};

extern DisplayState lastDisplay;

class DisplayManager {
public:
    /**
     * Constructor to initialize the DisplayManager.
     * @param i2cAddress: I2C address of the display.
     * @param width: Width of the display in pixels.
     * @param height: Height of the display in pixels.
     */
    DisplayManager(uint8_t i2cAddress, uint16_t width = 128, uint16_t height = 64);

    /**
     * Initialize the display.
     */
    void begin();

    /**
     * Display a generic text message across up to three lines.
     */
    void showText(const char* line1, const char* line2 = "", const char* line3 = "");

    /**
     * Display details about the active virtual button (pot).
     */
    void showVirtualButtonInfo(uint8_t activePot, const char* filterType, float frequency, float qFactor, uint8_t channel);

    /**
     * Display the status of a control button.
     */
    void showControlButtonStatus(uint8_t buttonIndex, const char* statusMessage);

    /**
     * Update the display with real-time performance data.
     */
    void updateDisplay(uint8_t beatPosition,
                       const std::vector<uint8_t>& envelopeLevels,
                       const char* statusMessage,
                       uint8_t activePot,
                       uint8_t activeChannel,
                       const char* envelopeMode);

    /**
     * Update the display based on the current ButtonManagerContext.
     */
    void updateFromContext(const ButtonManagerContext& context);

    void setTemporaryMessage(const char* message, unsigned long duration = NORMAL_DISPLAY_TIME);

    /**
     * Clear the display (both memory buffer and actual screen).
     */
    void clear();

    /**
     * Display a temporary status message.
     */
    void displayStatus(const char* status, unsigned long duration);

    /**
     * Show a numeric value, optionally clearing first.
     */
    void showValue(uint8_t value, bool clearDisplay = true);

    /**
     * Show a mode string, optionally clearing first.
     */
    void showMode(const char* mode, bool clearDisplay = true);

    /**
     * NEW: A helper method for showing ARG info (method + envelope pair).
     */
    void showARGInfo(const char* methodName, int envA, int envB);
    void showEnvelopeAssignment(int potIndex, int efIndex, const char* mode = nullptr, const char* argMethod = nullptr);
    void showMIDIMessage(uint8_t cc, uint8_t value, uint8_t channel);
    void updateBeat(uint8_t beatPosition, bool clockRunning); 


// 🔹 Buffered draw batching
void beginDraw();      // Prepares frame (clears, setups)
void endDraw();        // Commits frame with .display()

// 🔹 Error/debug overlay
void showError(const char* errorMessage, bool persistent = false);

// 🔹 Specific update helpers
void updateEnvelope(uint8_t level);
void showEnvelopeLevel(uint8_t level);  // Single
void showEnvelopeLevels(uint8_t envA, uint8_t envB);  // Dual
void updateActiveSelection(uint8_t activePot, uint8_t activeChannel);

// 🔹 Highlighting helpers
void highlightActivePot(uint8_t potIndex);
void highlightActiveMode(const String& modeName);

// 🔹 Dynamic update interval control
void setUpdateInterval(unsigned long intervalMs);
unsigned long getUpdateInterval() const;
    

private:
    // The Adafruit SSD1306 display object
    Adafruit_SSD1306 _display;

    uint8_t _i2cAddress;        // I2C address of the display
    String _statusMessage;      // For storing any temp status messages
    unsigned long _statusTimeout; // For clearing status messages after a delay

    // 🔹 Internal state for batching
bool _isDrawing = false;

// 🔹 Configurable update interval
unsigned long _updateIntervalMs = 100;  // Default value or configurable

// 🔹 Active selection tracking
uint8_t _activePot = 0;
uint8_t _activeChannel = 0;
String _activeMode = "MIDI";
};

#endif // DISPLAYMANAGER_H