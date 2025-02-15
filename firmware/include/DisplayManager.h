#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vector>

// Forward declaration of ButtonManagerContext to avoid circular dependency
struct ButtonManagerContext;

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
     * @param line1: First line of text.
     * @param line2: Second line of text (optional).
     * @param line3: Third line of text (optional).
     */
    void showText(const char* line1, const char* line2 = "", const char* line3 = "");

    /**
     * Display details about the active virtual button (pot).
     * @param activePot: Index of the active button.
     * @param filterType: Filter type (e.g., "Low-pass").
     * @param frequency: Filter frequency in Hz.
     * @param qFactor: Filter Q factor.
     * @param channel: MIDI channel associated with the button.
     */
    void showVirtualButtonInfo(uint8_t activePot, const char* filterType, float frequency, float qFactor, uint8_t channel);

    /**
     * Display the status of a control button.
     * @param buttonIndex: Index of the control button.
     * @param statusMessage: Status message to display.
     */
    void showControlButtonStatus(uint8_t buttonIndex, const char* statusMessage);

    /**
     * Update the display with real-time performance data.
     * @param beatPosition: Current beat position.
     * @param envelopeLevels: Envelope levels to display.
     * @param statusMessage: Status message to display.
     * @param activePot: Index of the active button.
     * @param activeChannel: MIDI channel of the active button.
     * @param envelopeMode: Current envelope mode.
     */
    void updateDisplay(uint8_t beatPosition, const std::vector<uint8_t>& envelopeLevels, const char* statusMessage, uint8_t activePot, uint8_t activeChannel, const char* envelopeMode);

    /**
     * Update the display based on the current ButtonManagerContext.
     * @param context: Reference to the ButtonManagerContext.
     */
    void updateFromContext(const ButtonManagerContext& context); // Forward-declared type

    /**
     * Clear the display.
     */
    void clear();

    /**
     * Display a temporary status message.
     * @param status: The message to display.
     * @param duration: How long to display the message (in milliseconds).
     */
    void displayStatus(const char* status, unsigned long duration);

    void showValue(uint8_t value, bool clearDisplay = true);
    void showMode(const char* mode, bool clearDisplay = true);

private:
    Adafruit_SSD1306 _display; // OLED display object
    uint8_t _i2cAddress;       // I2C address of the display
    String _statusMessage;     // Status message for temporary displays
    unsigned long _statusTimeout; // Timeout for clearing status messages
};

#endif // DISPLAYMANAGER_H
